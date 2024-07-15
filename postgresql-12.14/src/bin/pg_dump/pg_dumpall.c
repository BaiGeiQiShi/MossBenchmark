                                                                            
   
                
   
                                                                         
                                                                        
   
                                                                          
                                     
   
                                
   
                                                                            
   

#include "postgres_fe.h"

#include <time.h>
#include <unistd.h>

#include "getopt_long.h"

#include "dumputils.h"
#include "pg_backup.h"
#include "common/file_utils.h"
#include "common/logging.h"
#include "fe_utils/connect.h"
#include "fe_utils/string_utils.h"

                                                
#define PGDUMP_VERSIONSTR "pg_dump (PostgreSQL) " PG_VERSION "\n"

static void
help(void);

static void
dropRoles(PGconn *conn);
static void
dumpRoles(PGconn *conn);
static void
dumpRoleMembership(PGconn *conn);
static void
dumpGroups(PGconn *conn);
static void
dropTablespaces(PGconn *conn);
static void
dumpTablespaces(PGconn *conn);
static void
dropDBs(PGconn *conn);
static void
dumpUserConfig(PGconn *conn, const char *username);
static void
dumpDatabases(PGconn *conn);
static void
dumpTimestamp(const char *msg);
static int
runPgDump(const char *dbname, const char *create_opts);
static void
buildShSecLabels(PGconn *conn, const char *catalog_name, Oid objectId, const char *objtype, const char *objname, PQExpBuffer buffer);
static PGconn *
connectDatabase(const char *dbname, const char *connstr, const char *pghost, const char *pgport, const char *pguser, trivalue prompt_password, bool fail_on_error);
static char *
constructConnStr(const char **keywords, const char **values);
static PGresult *
executeQuery(PGconn *conn, const char *query);
static void
executeCommand(PGconn *conn, const char *query);
static void
expand_dbname_patterns(PGconn *conn, SimpleStringList *patterns, SimpleStringList *names);

static char pg_dump_bin[MAXPGPATH];
static const char *progname;
static PQExpBuffer pgdumpopts;
static char *connstr = "";
static bool output_clean = false;
static bool skip_acls = false;
static bool verbose = false;
static bool dosync = true;

static int binary_upgrade = 0;
static int column_inserts = 0;
static int disable_dollar_quoting = 0;
static int disable_triggers = 0;
static int if_exists = 0;
static int inserts = 0;
static int no_tablespaces = 0;
static int use_setsessauth = 0;
static int no_comments = 0;
static int no_publications = 0;
static int no_security_labels = 0;
static int no_subscriptions = 0;
static int no_unlogged_table_data = 0;
static int no_role_passwords = 0;
static int server_version;
static int load_via_partition_root = 0;
static int on_conflict_do_nothing = 0;

static char role_catalog[10];
#define PG_AUTHID "pg_authid"
#define PG_ROLES "pg_roles "

static FILE *OPF;
static char *filename = NULL;

static SimpleStringList database_exclude_patterns = {NULL, NULL};
static SimpleStringList database_exclude_names = {NULL, NULL};

#define exit_nicely(code) exit(code)

int
main(int argc, char *argv[])
{
  static struct option long_options[] = {{"data-only", no_argument, NULL, 'a'}, {"clean", no_argument, NULL, 'c'}, {"encoding", required_argument, NULL, 'E'}, {"file", required_argument, NULL, 'f'}, {"globals-only", no_argument, NULL, 'g'}, {"host", required_argument, NULL, 'h'}, {"dbname", required_argument, NULL, 'd'}, {"database", required_argument, NULL, 'l'}, {"no-owner", no_argument, NULL, 'O'}, {"port", required_argument, NULL, 'p'}, {"roles-only", no_argument, NULL, 'r'}, {"schema-only", no_argument, NULL, 's'}, {"superuser", required_argument, NULL, 'S'}, {"tablespaces-only", no_argument, NULL, 't'}, {"username", required_argument, NULL, 'U'}, {"verbose", no_argument, NULL, 'v'}, {"no-password", no_argument, NULL, 'w'}, {"password", no_argument, NULL, 'W'}, {"no-privileges", no_argument, NULL, 'x'}, {"no-acl", no_argument, NULL, 'x'},

         
                                                                            
         
      {"attribute-inserts", no_argument, &column_inserts, 1}, {"binary-upgrade", no_argument, &binary_upgrade, 1}, {"column-inserts", no_argument, &column_inserts, 1}, {"disable-dollar-quoting", no_argument, &disable_dollar_quoting, 1}, {"disable-triggers", no_argument, &disable_triggers, 1}, {"exclude-database", required_argument, NULL, 6}, {"extra-float-digits", required_argument, NULL, 5}, {"if-exists", no_argument, &if_exists, 1}, {"inserts", no_argument, &inserts, 1}, {"lock-wait-timeout", required_argument, NULL, 2}, {"no-tablespaces", no_argument, &no_tablespaces, 1}, {"quote-all-identifiers", no_argument, &quote_all_identifiers, 1}, {"load-via-partition-root", no_argument, &load_via_partition_root, 1}, {"role", required_argument, NULL, 3}, {"use-set-session-authorization", no_argument, &use_setsessauth, 1}, {"no-comments", no_argument, &no_comments, 1}, {"no-publications", no_argument, &no_publications, 1}, {"no-role-passwords", no_argument, &no_role_passwords, 1},
      {"no-security-labels", no_argument, &no_security_labels, 1}, {"no-subscriptions", no_argument, &no_subscriptions, 1}, {"no-sync", no_argument, NULL, 4}, {"no-unlogged-table-data", no_argument, &no_unlogged_table_data, 1}, {"on-conflict-do-nothing", no_argument, &on_conflict_do_nothing, 1}, {"rows-per-insert", required_argument, NULL, 7},

      {NULL, 0, NULL, 0}};

  char *pghost = NULL;
  char *pgport = NULL;
  char *pguser = NULL;
  char *pgdb = NULL;
  char *use_role = NULL;
  const char *dumpencoding = NULL;
  trivalue prompt_password = TRI_DEFAULT;
  bool data_only = false;
  bool globals_only = false;
  bool roles_only = false;
  bool tablespaces_only = false;
  PGconn *conn;
  int encoding;
  const char *std_strings;
  int c, ret;
  int optindex;

  pg_logging_init(argv[0]);
  pg_logging_set_level(PG_LOG_WARNING);
  set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pg_dump"));
  progname = get_progname(argv[0]);

  if (argc > 1)
  {
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
    {
      help();
      exit_nicely(0);
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
    {
      puts("pg_dumpall (PostgreSQL) " PG_VERSION);
      exit_nicely(0);
    }
  }

  if ((ret = find_other_exec(argv[0], "pg_dump", PGDUMP_VERSIONSTR, pg_dump_bin)) < 0)
  {
    char full_path[MAXPGPATH];

    if (find_my_exec(argv[0], full_path) < 0)
    {
      strlcpy(full_path, progname, sizeof(full_path));
    }

    if (ret == -1)
    {
      pg_log_error("The program \"pg_dump\" is needed by %s but was not found in the\n"
                   "same directory as \"%s\".\n"
                   "Check your installation.",
          progname, full_path);
    }
    else
    {
      pg_log_error("The program \"pg_dump\" was found by \"%s\"\n"
                   "but was not the same version as %s.\n"
                   "Check your installation.",
          full_path, progname);
    }
    exit_nicely(1);
  }

  pgdumpopts = createPQExpBuffer();

  while ((c = getopt_long(argc, argv, "acd:E:f:gh:l:Op:rsS:tU:vwWx", long_options, &optindex)) != -1)
  {
    switch (c)
    {
    case 'a':
      data_only = true;
      appendPQExpBufferStr(pgdumpopts, " -a");
      break;

    case 'c':
      output_clean = true;
      break;

    case 'd':
      connstr = pg_strdup(optarg);
      break;

    case 'E':
      dumpencoding = pg_strdup(optarg);
      appendPQExpBufferStr(pgdumpopts, " -E ");
      appendShellString(pgdumpopts, optarg);
      break;

    case 'f':
      filename = pg_strdup(optarg);
      appendPQExpBufferStr(pgdumpopts, " -f ");
      appendShellString(pgdumpopts, filename);
      break;

    case 'g':
      globals_only = true;
      break;

    case 'h':
      pghost = pg_strdup(optarg);
      break;

    case 'l':
      pgdb = pg_strdup(optarg);
      break;

    case 'O':
      appendPQExpBufferStr(pgdumpopts, " -O");
      break;

    case 'p':
      pgport = pg_strdup(optarg);
      break;

    case 'r':
      roles_only = true;
      break;

    case 's':
      appendPQExpBufferStr(pgdumpopts, " -s");
      break;

    case 'S':
      appendPQExpBufferStr(pgdumpopts, " -S ");
      appendShellString(pgdumpopts, optarg);
      break;

    case 't':
      tablespaces_only = true;
      break;

    case 'U':
      pguser = pg_strdup(optarg);
      break;

    case 'v':
      verbose = true;
      pg_logging_set_level(PG_LOG_INFO);
      appendPQExpBufferStr(pgdumpopts, " -v");
      break;

    case 'w':
      prompt_password = TRI_NO;
      appendPQExpBufferStr(pgdumpopts, " -w");
      break;

    case 'W':
      prompt_password = TRI_YES;
      appendPQExpBufferStr(pgdumpopts, " -W");
      break;

    case 'x':
      skip_acls = true;
      appendPQExpBufferStr(pgdumpopts, " -x");
      break;

    case 0:
      break;

    case 2:
      appendPQExpBufferStr(pgdumpopts, " --lock-wait-timeout ");
      appendShellString(pgdumpopts, optarg);
      break;

    case 3:
      use_role = pg_strdup(optarg);
      appendPQExpBufferStr(pgdumpopts, " --role ");
      appendShellString(pgdumpopts, use_role);
      break;

    case 4:
      dosync = false;
      appendPQExpBufferStr(pgdumpopts, " --no-sync");
      break;

    case 5:
      appendPQExpBufferStr(pgdumpopts, " --extra-float-digits ");
      appendShellString(pgdumpopts, optarg);
      break;

    case 6:
      simple_string_list_append(&database_exclude_patterns, optarg);
      break;

    case 7:
      appendPQExpBufferStr(pgdumpopts, " --rows-per-insert ");
      appendShellString(pgdumpopts, optarg);
      break;

    default:
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit_nicely(1);
    }
  }

                                        
  if (optind < argc)
  {
    pg_log_error("too many command-line arguments (first is \"%s\")", argv[optind]);
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit_nicely(1);
  }

  if (database_exclude_patterns.head != NULL && (globals_only || roles_only || tablespaces_only))
  {
    pg_log_error("option --exclude-database cannot be used together with -g/--globals-only, -r/--roles-only, or -t/--tablespaces-only");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit_nicely(1);
  }

                                                                         
  if (globals_only && roles_only)
  {
    pg_log_error("options -g/--globals-only and -r/--roles-only cannot be used together");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit_nicely(1);
  }

  if (globals_only && tablespaces_only)
  {
    pg_log_error("options -g/--globals-only and -t/--tablespaces-only cannot be used together");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit_nicely(1);
  }

  if (if_exists && !output_clean)
  {
    pg_log_error("option --if-exists requires option -c/--clean");
    exit_nicely(1);
  }

  if (roles_only && tablespaces_only)
  {
    pg_log_error("options -r/--roles-only and -t/--tablespaces-only cannot be used together");
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit_nicely(1);
  }

     
                                                                      
                                                                             
                            
     
  if (no_role_passwords)
  {
    sprintf(role_catalog, "%s", PG_ROLES);
  }
  else
  {
    sprintf(role_catalog, "%s", PG_AUTHID);
  }

                                                     
  if (binary_upgrade)
  {
    appendPQExpBufferStr(pgdumpopts, " --binary-upgrade");
  }
  if (column_inserts)
  {
    appendPQExpBufferStr(pgdumpopts, " --column-inserts");
  }
  if (disable_dollar_quoting)
  {
    appendPQExpBufferStr(pgdumpopts, " --disable-dollar-quoting");
  }
  if (disable_triggers)
  {
    appendPQExpBufferStr(pgdumpopts, " --disable-triggers");
  }
  if (inserts)
  {
    appendPQExpBufferStr(pgdumpopts, " --inserts");
  }
  if (no_tablespaces)
  {
    appendPQExpBufferStr(pgdumpopts, " --no-tablespaces");
  }
  if (quote_all_identifiers)
  {
    appendPQExpBufferStr(pgdumpopts, " --quote-all-identifiers");
  }
  if (load_via_partition_root)
  {
    appendPQExpBufferStr(pgdumpopts, " --load-via-partition-root");
  }
  if (use_setsessauth)
  {
    appendPQExpBufferStr(pgdumpopts, " --use-set-session-authorization");
  }
  if (no_comments)
  {
    appendPQExpBufferStr(pgdumpopts, " --no-comments");
  }
  if (no_publications)
  {
    appendPQExpBufferStr(pgdumpopts, " --no-publications");
  }
  if (no_security_labels)
  {
    appendPQExpBufferStr(pgdumpopts, " --no-security-labels");
  }
  if (no_subscriptions)
  {
    appendPQExpBufferStr(pgdumpopts, " --no-subscriptions");
  }
  if (no_unlogged_table_data)
  {
    appendPQExpBufferStr(pgdumpopts, " --no-unlogged-table-data");
  }
  if (on_conflict_do_nothing)
  {
    appendPQExpBufferStr(pgdumpopts, " --on-conflict-do-nothing");
  }

     
                                                                      
                                                                       
                                                                        
                                                           
     
  if (pgdb)
  {
    conn = connectDatabase(pgdb, connstr, pghost, pgport, pguser, prompt_password, false);

    if (!conn)
    {
      pg_log_error("could not connect to database \"%s\"", pgdb);
      exit_nicely(1);
    }
  }
  else
  {
    conn = connectDatabase("postgres", connstr, pghost, pgport, pguser, prompt_password, false);
    if (!conn)
    {
      conn = connectDatabase("template1", connstr, pghost, pgport, pguser, prompt_password, true);
    }

    if (!conn)
    {
      pg_log_error("could not connect to databases \"postgres\" or \"template1\"\n"
                   "Please specify an alternative database.");
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit_nicely(1);
    }
  }

     
                                                                  
     
  expand_dbname_patterns(conn, &database_exclude_patterns, &database_exclude_names);

     
                                                            
     
  if (filename)
  {
    OPF = fopen(filename, PG_BINARY_W);
    if (!OPF)
    {
      pg_log_error("could not open output file \"%s\": %m", filename);
      exit_nicely(1);
    }
  }
  else
  {
    OPF = stdout;
  }

     
                                           
     
  if (dumpencoding)
  {
    if (PQsetClientEncoding(conn, dumpencoding) < 0)
    {
      pg_log_error("invalid client encoding \"%s\" specified", dumpencoding);
      exit_nicely(1);
    }
  }

     
                                                                             
                                    
     
  encoding = PQclientEncoding(conn);
  std_strings = PQparameterStatus(conn, "standard_conforming_strings");
  if (!std_strings)
  {
    std_strings = "off";
  }

                                 
  if (use_role && server_version >= 80100)
  {
    PQExpBuffer query = createPQExpBuffer();

    appendPQExpBuffer(query, "SET ROLE %s", fmtId(use_role));
    executeCommand(conn, query->data);
    destroyPQExpBuffer(query);
  }

                                                      
  if (quote_all_identifiers && server_version >= 90100)
  {
    executeCommand(conn, "SET quote_all_identifiers = true");
  }

  fprintf(OPF, "--\n-- PostgreSQL database cluster dump\n--\n\n");
  if (verbose)
  {
    dumpTimestamp("Started on");
  }

     
                                                                        
                                                                     
                                                                          
                                                        
     

                                                        
  fprintf(OPF, "SET default_transaction_read_only = off;\n\n");

                                                    
  fprintf(OPF, "SET client_encoding = '%s';\n", pg_encoding_to_char(encoding));
  fprintf(OPF, "SET standard_conforming_strings = %s;\n", std_strings);
  if (strcmp(std_strings, "off") == 0)
  {
    fprintf(OPF, "SET escape_string_warning = off;\n");
  }
  fprintf(OPF, "\n");

  if (!data_only)
  {
       
                                                                  
                                                                         
                                                                     
                                                                         
       
    if (output_clean)
    {
      if (!globals_only && !roles_only && !tablespaces_only)
      {
        dropDBs(conn);
      }

      if (!roles_only && !no_tablespaces)
      {
        dropTablespaces(conn);
      }

      if (!tablespaces_only)
      {
        dropRoles(conn);
      }
    }

       
                                                                           
                                       
       
    if (!tablespaces_only)
    {
                              
      dumpRoles(conn);

                                                                       
      if (server_version >= 80100)
      {
        dumpRoleMembership(conn);
      }
      else
      {
        dumpGroups(conn);
      }
    }

                          
    if (!roles_only && !no_tablespaces)
    {
      dumpTablespaces(conn);
    }
  }

  if (!globals_only && !roles_only && !tablespaces_only)
  {
    dumpDatabases(conn);
  }

  PQfinish(conn);

  if (verbose)
  {
    dumpTimestamp("Completed on");
  }
  fprintf(OPF, "--\n-- PostgreSQL database cluster dump complete\n--\n\n");

  if (filename)
  {
    fclose(OPF);

                                                       
    if (dosync)
    {
      (void)fsync_fname(filename, false);
    }
  }

  exit_nicely(0);
}

static void
help(void)
{
  printf(_("%s extracts a PostgreSQL database cluster into an SQL script file.\n\n"), progname);
  printf(_("Usage:\n"));
  printf(_("  %s [OPTION]...\n"), progname);

  printf(_("\nGeneral options:\n"));
  printf(_("  -f, --file=FILENAME          output file name\n"));
  printf(_("  -v, --verbose                verbose mode\n"));
  printf(_("  -V, --version                output version information, then exit\n"));
  printf(_("  --lock-wait-timeout=TIMEOUT  fail after waiting TIMEOUT for a table lock\n"));
  printf(_("  -?, --help                   show this help, then exit\n"));
  printf(_("\nOptions controlling the output content:\n"));
  printf(_("  -a, --data-only              dump only the data, not the schema\n"));
  printf(_("  -c, --clean                  clean (drop) databases before recreating\n"));
  printf(_("  -E, --encoding=ENCODING      dump the data in encoding ENCODING\n"));
  printf(_("  -g, --globals-only           dump only global objects, no databases\n"));
  printf(_("  -O, --no-owner               skip restoration of object ownership\n"));
  printf(_("  -r, --roles-only             dump only roles, no databases or tablespaces\n"));
  printf(_("  -s, --schema-only            dump only the schema, no data\n"));
  printf(_("  -S, --superuser=NAME         superuser user name to use in the dump\n"));
  printf(_("  -t, --tablespaces-only       dump only tablespaces, no databases or roles\n"));
  printf(_("  -x, --no-privileges          do not dump privileges (grant/revoke)\n"));
  printf(_("  --binary-upgrade             for use by upgrade utilities only\n"));
  printf(_("  --column-inserts             dump data as INSERT commands with column names\n"));
  printf(_("  --disable-dollar-quoting     disable dollar quoting, use SQL standard quoting\n"));
  printf(_("  --disable-triggers           disable triggers during data-only restore\n"));
  printf(_("  --exclude-database=PATTERN   exclude databases whose name matches PATTERN\n"));
  printf(_("  --extra-float-digits=NUM     override default setting for extra_float_digits\n"));
  printf(_("  --if-exists                  use IF EXISTS when dropping objects\n"));
  printf(_("  --inserts                    dump data as INSERT commands, rather than COPY\n"));
  printf(_("  --load-via-partition-root    load partitions via the root table\n"));
  printf(_("  --no-comments                do not dump comments\n"));
  printf(_("  --no-publications            do not dump publications\n"));
  printf(_("  --no-role-passwords          do not dump passwords for roles\n"));
  printf(_("  --no-security-labels         do not dump security label assignments\n"));
  printf(_("  --no-subscriptions           do not dump subscriptions\n"));
  printf(_("  --no-sync                    do not wait for changes to be written safely to disk\n"));
  printf(_("  --no-tablespaces             do not dump tablespace assignments\n"));
  printf(_("  --no-unlogged-table-data     do not dump unlogged table data\n"));
  printf(_("  --on-conflict-do-nothing     add ON CONFLICT DO NOTHING to INSERT commands\n"));
  printf(_("  --quote-all-identifiers      quote all identifiers, even if not key words\n"));
  printf(_("  --rows-per-insert=NROWS      number of rows per INSERT; implies --inserts\n"));
  printf(_("  --use-set-session-authorization\n"
           "                               use SET SESSION AUTHORIZATION commands instead of\n"
           "                               ALTER OWNER commands to set ownership\n"));

  printf(_("\nConnection options:\n"));
  printf(_("  -d, --dbname=CONNSTR     connect using connection string\n"));
  printf(_("  -h, --host=HOSTNAME      database server host or socket directory\n"));
  printf(_("  -l, --database=DBNAME    alternative default database\n"));
  printf(_("  -p, --port=PORT          database server port number\n"));
  printf(_("  -U, --username=NAME      connect as specified database user\n"));
  printf(_("  -w, --no-password        never prompt for password\n"));
  printf(_("  -W, --password           force password prompt (should happen automatically)\n"));
  printf(_("  --role=ROLENAME          do SET ROLE before dump\n"));

  printf(_("\nIf -f/--file is not used, then the SQL script will be written to the standard\n"
           "output.\n\n"));
  printf(_("Report bugs to <pgsql-bugs@lists.postgresql.org>.\n"));
}

   
              
   
static void
dropRoles(PGconn *conn)
{
  PQExpBuffer buf = createPQExpBuffer();
  PGresult *res;
  int i_rolname;
  int i;

  if (server_version >= 90600)
  {
    printfPQExpBuffer(buf,
        "SELECT rolname "
        "FROM %s "
        "WHERE rolname !~ '^pg_' "
        "ORDER BY 1",
        role_catalog);
  }
  else if (server_version >= 80100)
  {
    printfPQExpBuffer(buf,
        "SELECT rolname "
        "FROM %s "
        "ORDER BY 1",
        role_catalog);
  }
  else
  {
    printfPQExpBuffer(buf, "SELECT usename as rolname "
                           "FROM pg_shadow "
                           "UNION "
                           "SELECT groname as rolname "
                           "FROM pg_group "
                           "ORDER BY 1");
  }

  res = executeQuery(conn, buf->data);

  i_rolname = PQfnumber(res, "rolname");

  if (PQntuples(res) > 0)
  {
    fprintf(OPF, "--\n-- Drop roles\n--\n\n");
  }

  for (i = 0; i < PQntuples(res); i++)
  {
    const char *rolename;

    rolename = PQgetvalue(res, i, i_rolname);

    fprintf(OPF, "DROP ROLE %s%s;\n", if_exists ? "IF EXISTS " : "", fmtId(rolename));
  }

  PQclear(res);
  destroyPQExpBuffer(buf);

  fprintf(OPF, "\n\n");
}

   
              
   
static void
dumpRoles(PGconn *conn)
{
  PQExpBuffer buf = createPQExpBuffer();
  PGresult *res;
  int i_oid, i_rolname, i_rolsuper, i_rolinherit, i_rolcreaterole, i_rolcreatedb, i_rolcanlogin, i_rolconnlimit, i_rolpassword, i_rolvaliduntil, i_rolreplication, i_rolbypassrls, i_rolcomment, i_is_current_user;
  int i;

                                       
  if (server_version >= 90600)
  {
    printfPQExpBuffer(buf,
        "SELECT oid, rolname, rolsuper, rolinherit, "
        "rolcreaterole, rolcreatedb, "
        "rolcanlogin, rolconnlimit, rolpassword, "
        "rolvaliduntil, rolreplication, rolbypassrls, "
        "pg_catalog.shobj_description(oid, '%s') as rolcomment, "
        "rolname = current_user AS is_current_user "
        "FROM %s "
        "WHERE rolname !~ '^pg_' "
        "ORDER BY 2",
        role_catalog, role_catalog);
  }
  else if (server_version >= 90500)
  {
    printfPQExpBuffer(buf,
        "SELECT oid, rolname, rolsuper, rolinherit, "
        "rolcreaterole, rolcreatedb, "
        "rolcanlogin, rolconnlimit, rolpassword, "
        "rolvaliduntil, rolreplication, rolbypassrls, "
        "pg_catalog.shobj_description(oid, '%s') as rolcomment, "
        "rolname = current_user AS is_current_user "
        "FROM %s "
        "ORDER BY 2",
        role_catalog, role_catalog);
  }
  else if (server_version >= 90100)
  {
    printfPQExpBuffer(buf,
        "SELECT oid, rolname, rolsuper, rolinherit, "
        "rolcreaterole, rolcreatedb, "
        "rolcanlogin, rolconnlimit, rolpassword, "
        "rolvaliduntil, rolreplication, "
        "false as rolbypassrls, "
        "pg_catalog.shobj_description(oid, '%s') as rolcomment, "
        "rolname = current_user AS is_current_user "
        "FROM %s "
        "ORDER BY 2",
        role_catalog, role_catalog);
  }
  else if (server_version >= 80200)
  {
    printfPQExpBuffer(buf,
        "SELECT oid, rolname, rolsuper, rolinherit, "
        "rolcreaterole, rolcreatedb, "
        "rolcanlogin, rolconnlimit, rolpassword, "
        "rolvaliduntil, false as rolreplication, "
        "false as rolbypassrls, "
        "pg_catalog.shobj_description(oid, '%s') as rolcomment, "
        "rolname = current_user AS is_current_user "
        "FROM %s "
        "ORDER BY 2",
        role_catalog, role_catalog);
  }
  else if (server_version >= 80100)
  {
    printfPQExpBuffer(buf,
        "SELECT oid, rolname, rolsuper, rolinherit, "
        "rolcreaterole, rolcreatedb, "
        "rolcanlogin, rolconnlimit, rolpassword, "
        "rolvaliduntil, false as rolreplication, "
        "false as rolbypassrls, "
        "null as rolcomment, "
        "rolname = current_user AS is_current_user "
        "FROM %s "
        "ORDER BY 2",
        role_catalog);
  }
  else
  {
    printfPQExpBuffer(buf, "SELECT 0 as oid, usename as rolname, "
                           "usesuper as rolsuper, "
                           "true as rolinherit, "
                           "usesuper as rolcreaterole, "
                           "usecreatedb as rolcreatedb, "
                           "true as rolcanlogin, "
                           "-1 as rolconnlimit, "
                           "passwd as rolpassword, "
                           "valuntil as rolvaliduntil, "
                           "false as rolreplication, "
                           "false as rolbypassrls, "
                           "null as rolcomment, "
                           "usename = current_user AS is_current_user "
                           "FROM pg_shadow "
                           "UNION ALL "
                           "SELECT 0 as oid, groname as rolname, "
                           "false as rolsuper, "
                           "true as rolinherit, "
                           "false as rolcreaterole, "
                           "false as rolcreatedb, "
                           "false as rolcanlogin, "
                           "-1 as rolconnlimit, "
                           "null::text as rolpassword, "
                           "null::timestamptz as rolvaliduntil, "
                           "false as rolreplication, "
                           "false as rolbypassrls, "
                           "null as rolcomment, "
                           "false AS is_current_user "
                           "FROM pg_group "
                           "WHERE NOT EXISTS (SELECT 1 FROM pg_shadow "
                           " WHERE usename = groname) "
                           "ORDER BY 2");
  }

  res = executeQuery(conn, buf->data);

  i_oid = PQfnumber(res, "oid");
  i_rolname = PQfnumber(res, "rolname");
  i_rolsuper = PQfnumber(res, "rolsuper");
  i_rolinherit = PQfnumber(res, "rolinherit");
  i_rolcreaterole = PQfnumber(res, "rolcreaterole");
  i_rolcreatedb = PQfnumber(res, "rolcreatedb");
  i_rolcanlogin = PQfnumber(res, "rolcanlogin");
  i_rolconnlimit = PQfnumber(res, "rolconnlimit");
  i_rolpassword = PQfnumber(res, "rolpassword");
  i_rolvaliduntil = PQfnumber(res, "rolvaliduntil");
  i_rolreplication = PQfnumber(res, "rolreplication");
  i_rolbypassrls = PQfnumber(res, "rolbypassrls");
  i_rolcomment = PQfnumber(res, "rolcomment");
  i_is_current_user = PQfnumber(res, "is_current_user");

  if (PQntuples(res) > 0)
  {
    fprintf(OPF, "--\n-- Roles\n--\n\n");
  }

  for (i = 0; i < PQntuples(res); i++)
  {
    const char *rolename;
    Oid auth_oid;

    auth_oid = atooid(PQgetvalue(res, i, i_oid));
    rolename = PQgetvalue(res, i, i_rolname);

    if (strncmp(rolename, "pg_", 3) == 0)
    {
      pg_log_warning("role name starting with \"pg_\" skipped (%s)", rolename);
      continue;
    }

    resetPQExpBuffer(buf);

    if (binary_upgrade)
    {
      appendPQExpBufferStr(buf, "\n-- For binary upgrade, must preserve pg_authid.oid\n");
      appendPQExpBuffer(buf, "SELECT pg_catalog.binary_upgrade_set_next_pg_authid_oid('%u'::pg_catalog.oid);\n\n", auth_oid);
    }

       
                                                                          
                                                                           
                                                                           
                                                                         
                                                                           
                                                         
       
    if (!binary_upgrade || strcmp(PQgetvalue(res, i, i_is_current_user), "f") == 0)
    {
      appendPQExpBuffer(buf, "CREATE ROLE %s;\n", fmtId(rolename));
    }
    appendPQExpBuffer(buf, "ALTER ROLE %s WITH", fmtId(rolename));

    if (strcmp(PQgetvalue(res, i, i_rolsuper), "t") == 0)
    {
      appendPQExpBufferStr(buf, " SUPERUSER");
    }
    else
    {
      appendPQExpBufferStr(buf, " NOSUPERUSER");
    }

    if (strcmp(PQgetvalue(res, i, i_rolinherit), "t") == 0)
    {
      appendPQExpBufferStr(buf, " INHERIT");
    }
    else
    {
      appendPQExpBufferStr(buf, " NOINHERIT");
    }

    if (strcmp(PQgetvalue(res, i, i_rolcreaterole), "t") == 0)
    {
      appendPQExpBufferStr(buf, " CREATEROLE");
    }
    else
    {
      appendPQExpBufferStr(buf, " NOCREATEROLE");
    }

    if (strcmp(PQgetvalue(res, i, i_rolcreatedb), "t") == 0)
    {
      appendPQExpBufferStr(buf, " CREATEDB");
    }
    else
    {
      appendPQExpBufferStr(buf, " NOCREATEDB");
    }

    if (strcmp(PQgetvalue(res, i, i_rolcanlogin), "t") == 0)
    {
      appendPQExpBufferStr(buf, " LOGIN");
    }
    else
    {
      appendPQExpBufferStr(buf, " NOLOGIN");
    }

    if (strcmp(PQgetvalue(res, i, i_rolreplication), "t") == 0)
    {
      appendPQExpBufferStr(buf, " REPLICATION");
    }
    else
    {
      appendPQExpBufferStr(buf, " NOREPLICATION");
    }

    if (strcmp(PQgetvalue(res, i, i_rolbypassrls), "t") == 0)
    {
      appendPQExpBufferStr(buf, " BYPASSRLS");
    }
    else
    {
      appendPQExpBufferStr(buf, " NOBYPASSRLS");
    }

    if (strcmp(PQgetvalue(res, i, i_rolconnlimit), "-1") != 0)
    {
      appendPQExpBuffer(buf, " CONNECTION LIMIT %s", PQgetvalue(res, i, i_rolconnlimit));
    }

    if (!PQgetisnull(res, i, i_rolpassword) && !no_role_passwords)
    {
      appendPQExpBufferStr(buf, " PASSWORD ");
      appendStringLiteralConn(buf, PQgetvalue(res, i, i_rolpassword), conn);
    }

    if (!PQgetisnull(res, i, i_rolvaliduntil))
    {
      appendPQExpBuffer(buf, " VALID UNTIL '%s'", PQgetvalue(res, i, i_rolvaliduntil));
    }

    appendPQExpBufferStr(buf, ";\n");

    if (!no_comments && !PQgetisnull(res, i, i_rolcomment))
    {
      appendPQExpBuffer(buf, "COMMENT ON ROLE %s IS ", fmtId(rolename));
      appendStringLiteralConn(buf, PQgetvalue(res, i, i_rolcomment), conn);
      appendPQExpBufferStr(buf, ";\n");
    }

    if (!no_security_labels && server_version >= 90200)
    {
      buildShSecLabels(conn, "pg_authid", auth_oid, "ROLE", rolename, buf);
    }

    fprintf(OPF, "%s", buf->data);
  }

     
                                                                             
                                                                           
                           
     
  for (i = 0; i < PQntuples(res); i++)
  {
    dumpUserConfig(conn, PQgetvalue(res, i, i_rolname));
  }

  PQclear(res);

  fprintf(OPF, "\n\n");

  destroyPQExpBuffer(buf);
}

   
                                                                        
   
                                                                         
                      
   
static void
dumpRoleMembership(PGconn *conn)
{
  PQExpBuffer buf = createPQExpBuffer();
  PGresult *res;
  int i;

  printfPQExpBuffer(buf,
      "SELECT ur.rolname AS roleid, "
      "um.rolname AS member, "
      "a.admin_option, "
      "ug.rolname AS grantor "
      "FROM pg_auth_members a "
      "LEFT JOIN %s ur on ur.oid = a.roleid "
      "LEFT JOIN %s um on um.oid = a.member "
      "LEFT JOIN %s ug on ug.oid = a.grantor "
      "WHERE NOT (ur.rolname ~ '^pg_' AND um.rolname ~ '^pg_')"
      "ORDER BY 1,2,3",
      role_catalog, role_catalog, role_catalog);
  res = executeQuery(conn, buf->data);

  if (PQntuples(res) > 0)
  {
    fprintf(OPF, "--\n-- Role memberships\n--\n\n");
  }

  for (i = 0; i < PQntuples(res); i++)
  {
    char *roleid = PQgetvalue(res, i, 0);
    char *member = PQgetvalue(res, i, 1);
    char *option = PQgetvalue(res, i, 2);

    fprintf(OPF, "GRANT %s", fmtId(roleid));
    fprintf(OPF, " TO %s", fmtId(member));
    if (*option == 't')
    {
      fprintf(OPF, " WITH ADMIN OPTION");
    }

       
                                                                         
                                                      
       
    if (!PQgetisnull(res, i, 3))
    {
      char *grantor = PQgetvalue(res, i, 3);

      fprintf(OPF, " GRANTED BY %s", fmtId(grantor));
    }
    fprintf(OPF, ";\n");
  }

  PQclear(res);
  destroyPQExpBuffer(buf);

  fprintf(OPF, "\n\n");
}

   
                                                                        
                                                                     
                                                  
   
                                                                         
                      
   
static void
dumpGroups(PGconn *conn)
{
  PQExpBuffer buf = createPQExpBuffer();
  PGresult *res;
  int i;

  res = executeQuery(conn, "SELECT groname, grolist FROM pg_group ORDER BY 1");

  if (PQntuples(res) > 0)
  {
    fprintf(OPF, "--\n-- Role memberships\n--\n\n");
  }

  for (i = 0; i < PQntuples(res); i++)
  {
    char *groname = PQgetvalue(res, i, 0);
    char *grolist = PQgetvalue(res, i, 1);
    PGresult *res2;
    int j;

       
                                                              
       
    if (strlen(grolist) < 3)
    {
      continue;
    }

    grolist = pg_strdup(grolist);
    grolist[0] = '(';
    grolist[strlen(grolist) - 1] = ')';
    printfPQExpBuffer(buf,
        "SELECT usename FROM pg_shadow "
        "WHERE usesysid IN %s ORDER BY 1",
        grolist);
    free(grolist);

    res2 = executeQuery(conn, buf->data);

    for (j = 0; j < PQntuples(res2); j++)
    {
      char *usename = PQgetvalue(res2, j, 0);

         
                                                                
                                                            
         
      if (strcmp(groname, usename) == 0)
      {
        continue;
      }

      fprintf(OPF, "GRANT %s", fmtId(groname));
      fprintf(OPF, " TO %s;\n", fmtId(usename));
    }

    PQclear(res2);
  }

  PQclear(res);
  destroyPQExpBuffer(buf);

  fprintf(OPF, "\n\n");
}

   
                     
   
static void
dropTablespaces(PGconn *conn)
{
  PGresult *res;
  int i;

     
                                                                         
             
     
  res = executeQuery(conn, "SELECT spcname "
                           "FROM pg_catalog.pg_tablespace "
                           "WHERE spcname !~ '^pg_' "
                           "ORDER BY 1");

  if (PQntuples(res) > 0)
  {
    fprintf(OPF, "--\n-- Drop tablespaces\n--\n\n");
  }

  for (i = 0; i < PQntuples(res); i++)
  {
    char *spcname = PQgetvalue(res, i, 0);

    fprintf(OPF, "DROP TABLESPACE %s%s;\n", if_exists ? "IF EXISTS " : "", fmtId(spcname));
  }

  PQclear(res);

  fprintf(OPF, "\n\n");
}

   
                     
   
static void
dumpTablespaces(PGconn *conn)
{
  PGresult *res;
  int i;

     
                                                                         
             
     
                                                                          
                                                                             
                                                                     
     
                                                   
     
                                                                         
                                                                             
                                                                         
                                            
     
                                                                       
                                                                      
     
  if (server_version >= 90600)
  {
    res = executeQuery(conn, "SELECT oid, spcname, "
                             "pg_catalog.pg_get_userbyid(spcowner) AS spcowner, "
                             "pg_catalog.pg_tablespace_location(oid), "
                             "(SELECT array_agg(acl ORDER BY row_n) FROM "
                             "  (SELECT acl, row_n FROM "
                             "     unnest(coalesce(spcacl,acldefault('t',spcowner))) "
                             "     WITH ORDINALITY AS perm(acl,row_n) "
                             "   WHERE NOT EXISTS ( "
                             "     SELECT 1 "
                             "     FROM unnest(acldefault('t',spcowner)) "
                             "       AS init(init_acl) "
                             "     WHERE acl = init_acl)) AS spcacls) "
                             " AS spcacl, "
                             "(SELECT array_agg(acl ORDER BY row_n) FROM "
                             "  (SELECT acl, row_n FROM "
                             "     unnest(acldefault('t',spcowner)) "
                             "     WITH ORDINALITY AS initp(acl,row_n) "
                             "   WHERE NOT EXISTS ( "
                             "     SELECT 1 "
                             "     FROM unnest(coalesce(spcacl,acldefault('t',spcowner))) "
                             "       AS permp(orig_acl) "
                             "     WHERE acl = orig_acl)) AS rspcacls) "
                             " AS rspcacl, "
                             "array_to_string(spcoptions, ', '),"
                             "pg_catalog.shobj_description(oid, 'pg_tablespace') "
                             "FROM pg_catalog.pg_tablespace "
                             "WHERE spcname !~ '^pg_' "
                             "ORDER BY 1");
  }
  else if (server_version >= 90200)
  {
    res = executeQuery(conn, "SELECT oid, spcname, "
                             "pg_catalog.pg_get_userbyid(spcowner) AS spcowner, "
                             "pg_catalog.pg_tablespace_location(oid), "
                             "spcacl, '' as rspcacl, "
                             "array_to_string(spcoptions, ', '),"
                             "pg_catalog.shobj_description(oid, 'pg_tablespace') "
                             "FROM pg_catalog.pg_tablespace "
                             "WHERE spcname !~ '^pg_' "
                             "ORDER BY 1");
  }
  else if (server_version >= 90000)
  {
    res = executeQuery(conn, "SELECT oid, spcname, "
                             "pg_catalog.pg_get_userbyid(spcowner) AS spcowner, "
                             "spclocation, spcacl, '' as rspcacl, "
                             "array_to_string(spcoptions, ', '),"
                             "pg_catalog.shobj_description(oid, 'pg_tablespace') "
                             "FROM pg_catalog.pg_tablespace "
                             "WHERE spcname !~ '^pg_' "
                             "ORDER BY 1");
  }
  else if (server_version >= 80200)
  {
    res = executeQuery(conn, "SELECT oid, spcname, "
                             "pg_catalog.pg_get_userbyid(spcowner) AS spcowner, "
                             "spclocation, spcacl, '' as rspcacl, null, "
                             "pg_catalog.shobj_description(oid, 'pg_tablespace') "
                             "FROM pg_catalog.pg_tablespace "
                             "WHERE spcname !~ '^pg_' "
                             "ORDER BY 1");
  }
  else
  {
    res = executeQuery(conn, "SELECT oid, spcname, "
                             "pg_catalog.pg_get_userbyid(spcowner) AS spcowner, "
                             "spclocation, spcacl, '' as rspcacl, "
                             "null, null "
                             "FROM pg_catalog.pg_tablespace "
                             "WHERE spcname !~ '^pg_' "
                             "ORDER BY 1");
  }

  if (PQntuples(res) > 0)
  {
    fprintf(OPF, "--\n-- Tablespaces\n--\n\n");
  }

  for (i = 0; i < PQntuples(res); i++)
  {
    PQExpBuffer buf = createPQExpBuffer();
    Oid spcoid = atooid(PQgetvalue(res, i, 0));
    char *spcname = PQgetvalue(res, i, 1);
    char *spcowner = PQgetvalue(res, i, 2);
    char *spclocation = PQgetvalue(res, i, 3);
    char *spcacl = PQgetvalue(res, i, 4);
    char *rspcacl = PQgetvalue(res, i, 5);
    char *spcoptions = PQgetvalue(res, i, 6);
    char *spccomment = PQgetvalue(res, i, 7);
    char *fspcname;

                                       
    fspcname = pg_strdup(fmtId(spcname));

    appendPQExpBuffer(buf, "CREATE TABLESPACE %s", fspcname);
    appendPQExpBuffer(buf, " OWNER %s", fmtId(spcowner));

    appendPQExpBufferStr(buf, " LOCATION ");
    appendStringLiteralConn(buf, spclocation, conn);
    appendPQExpBufferStr(buf, ";\n");

    if (spcoptions && spcoptions[0] != '\0')
    {
      appendPQExpBuffer(buf, "ALTER TABLESPACE %s SET (%s);\n", fspcname, spcoptions);
    }

    if (!skip_acls && !buildACLCommands(fspcname, NULL, NULL, "TABLESPACE", spcacl, rspcacl, spcowner, "", server_version, buf))
    {
      pg_log_error("could not parse ACL list (%s) for tablespace \"%s\"", spcacl, spcname);
      PQfinish(conn);
      exit_nicely(1);
    }

    if (!no_comments && spccomment && spccomment[0] != '\0')
    {
      appendPQExpBuffer(buf, "COMMENT ON TABLESPACE %s IS ", fspcname);
      appendStringLiteralConn(buf, spccomment, conn);
      appendPQExpBufferStr(buf, ";\n");
    }

    if (!no_security_labels && server_version >= 90200)
    {
      buildShSecLabels(conn, "pg_tablespace", spcoid, "TABLESPACE", spcname, buf);
    }

    fprintf(OPF, "%s", buf->data);

    free(fspcname);
    destroyPQExpBuffer(buf);
  }

  PQclear(res);
  fprintf(OPF, "\n\n");
}

   
                                        
   
static void
dropDBs(PGconn *conn)
{
  PGresult *res;
  int i;

     
                                                                             
                                                            
     
  res = executeQuery(conn, "SELECT datname "
                           "FROM pg_database d "
                           "WHERE datallowconn "
                           "ORDER BY datname");

  if (PQntuples(res) > 0)
  {
    fprintf(OPF, "--\n-- Drop databases (except postgres and template1)\n--\n\n");
  }

  for (i = 0; i < PQntuples(res); i++)
  {
    char *dbname = PQgetvalue(res, i, 0);

       
                                                                       
                                                                       
                                                  
       
    if (strcmp(dbname, "template1") != 0 && strcmp(dbname, "template0") != 0 && strcmp(dbname, "postgres") != 0)
    {
      fprintf(OPF, "DROP DATABASE %s%s;\n", if_exists ? "IF EXISTS " : "", fmtId(dbname));
    }
  }

  PQclear(res);

  fprintf(OPF, "\n\n");
}

   
                                    
   
static void
dumpUserConfig(PGconn *conn, const char *username)
{
  PQExpBuffer buf = createPQExpBuffer();
  int count = 1;
  bool first = true;

  for (;;)
  {
    PGresult *res;

    if (server_version >= 90000)
    {
      printfPQExpBuffer(buf,
          "SELECT setconfig[%d] FROM pg_db_role_setting WHERE "
          "setdatabase = 0 AND setrole = "
          "(SELECT oid FROM %s WHERE rolname = ",
          count, role_catalog);
    }
    else if (server_version >= 80100)
    {
      printfPQExpBuffer(buf, "SELECT rolconfig[%d] FROM %s WHERE rolname = ", count, role_catalog);
    }
    else
    {
      printfPQExpBuffer(buf, "SELECT useconfig[%d] FROM pg_shadow WHERE usename = ", count);
    }
    appendStringLiteralConn(buf, username, conn);
    if (server_version >= 90000)
    {
      appendPQExpBufferChar(buf, ')');
    }

    res = executeQuery(conn, buf->data);
    if (PQntuples(res) == 1 && !PQgetisnull(res, 0, 0))
    {
                                                    
      if (first)
      {
        fprintf(OPF, "--\n-- User Configurations\n--\n\n");
        first = false;
      }

      fprintf(OPF, "--\n-- User Config \"%s\"\n--\n\n", username);
      resetPQExpBuffer(buf);
      makeAlterConfigCommand(conn, PQgetvalue(res, 0, 0), "ROLE", username, NULL, NULL, buf);
      fprintf(OPF, "%s", buf->data);
      PQclear(res);
      count++;
    }
    else
    {
      PQclear(res);
      break;
    }
  }

  destroyPQExpBuffer(buf);
}

   
                                                                
                                                      
   
static void
expand_dbname_patterns(PGconn *conn, SimpleStringList *patterns, SimpleStringList *names)
{
  PQExpBuffer query;
  PGresult *res;

  if (patterns->head == NULL)
  {
    return;                    
  }

  query = createPQExpBuffer();

     
                                                                           
                                                                            
                                                 
     

  for (SimpleStringListCell *cell = patterns->head; cell; cell = cell->next)
  {
    appendPQExpBuffer(query, "SELECT datname FROM pg_catalog.pg_database n\n");
    processSQLNamePattern(conn, query, cell->val, false, false, NULL, "datname", NULL, NULL);

    res = executeQuery(conn, query->data);
    for (int i = 0; i < PQntuples(res); i++)
    {
      simple_string_list_append(names, PQgetvalue(res, i, 0));
    }

    PQclear(res);
    resetPQExpBuffer(query);
  }

  destroyPQExpBuffer(query);
}

   
                               
   
static void
dumpDatabases(PGconn *conn)
{
  PGresult *res;
  int i;

     
                                                                             
                                                      
     
                                                                           
                                                                            
                                                                             
                                                                         
                                                                          
                                                  
     
  res = executeQuery(conn, "SELECT datname "
                           "FROM pg_database d "
                           "WHERE datallowconn "
                           "ORDER BY (datname <> 'template1'), datname");

  if (PQntuples(res) > 0)
  {
    fprintf(OPF, "--\n-- Databases\n--\n\n");
  }

  for (i = 0; i < PQntuples(res); i++)
  {
    char *dbname = PQgetvalue(res, i, 0);
    const char *create_opts;
    int ret;

                                                                
    if (strcmp(dbname, "template0") == 0)
    {
      continue;
    }

                                               
    if (simple_string_list_member(&database_exclude_names, dbname))
    {
      pg_log_info("excluding database \"%s\"", dbname);
      continue;
    }

    pg_log_info("dumping database \"%s\"", dbname);

    fprintf(OPF, "--\n-- Database \"%s\" dump\n--\n\n", dbname);

       
                                                                      
                                                                         
                                                                           
                                                                      
                                                                       
                                 
       
    if (strcmp(dbname, "template1") == 0 || strcmp(dbname, "postgres") == 0)
    {
      if (output_clean)
      {
        create_opts = "--clean --create";
      }
      else
      {
        create_opts = "";
                                                                  
        fprintf(OPF, "\\connect %s\n\n", dbname);
      }
    }
    else
    {
      create_opts = "--create";
    }

    if (filename)
    {
      fclose(OPF);
    }

    ret = runPgDump(dbname, create_opts);
    if (ret != 0)
    {
      pg_log_error("pg_dump failed on database \"%s\", exiting", dbname);
      exit_nicely(1);
    }

    if (filename)
    {
      OPF = fopen(filename, PG_BINARY_A);
      if (!OPF)
      {
        pg_log_error("could not re-open the output file \"%s\": %m", filename);
        exit_nicely(1);
      }
    }
  }

  PQclear(res);
}

   
                                                  
   
static int
runPgDump(const char *dbname, const char *create_opts)
{
  PQExpBuffer connstrbuf = createPQExpBuffer();
  PQExpBuffer cmd = createPQExpBuffer();
  int ret;

  appendPQExpBuffer(cmd, "\"%s\" %s %s", pg_dump_bin, pgdumpopts->data, create_opts);

     
                                                                      
             
     
  if (filename)
  {
    appendPQExpBufferStr(cmd, " -Fa ");
  }
  else
  {
    appendPQExpBufferStr(cmd, " -Fp ");
  }

     
                                                                            
             
     
  appendPQExpBuffer(connstrbuf, "%s dbname=", connstr);
  appendConnStrVal(connstrbuf, dbname);

  appendShellString(cmd, connstrbuf->data);

  pg_log_info("running \"%s\"", cmd->data);

  fflush(stdout);
  fflush(stderr);

  ret = system(cmd->data);

  destroyPQExpBuffer(cmd);
  destroyPQExpBuffer(connstrbuf);

  return ret;
}

   
                    
   
                                                       
   
                                                                               
                                                                 
                                                                  
   
                                            
   
static void
buildShSecLabels(PGconn *conn, const char *catalog_name, Oid objectId, const char *objtype, const char *objname, PQExpBuffer buffer)
{
  PQExpBuffer sql = createPQExpBuffer();
  PGresult *res;

  buildShSecLabelQuery(conn, catalog_name, objectId, sql);
  res = executeQuery(conn, sql->data);
  emitShSecLabels(conn, res, buffer, objtype, objname);

  PQclear(res);
  destroyPQExpBuffer(sql);
}

   
                                                             
                                                                    
   
                                                                          
                                                                    
   
                                                                           
                                
   
static PGconn *
connectDatabase(const char *dbname, const char *connection_string, const char *pghost, const char *pgport, const char *pguser, trivalue prompt_password, bool fail_on_error)
{
  PGconn *conn;
  bool new_pass;
  const char *remoteversion_str;
  int my_version;
  const char **keywords = NULL;
  const char **values = NULL;
  PQconninfoOption *conn_opts = NULL;
  static bool have_password = false;
  static char password[100];

  if (prompt_password == TRI_YES && !have_password)
  {
    simple_prompt("Password: ", password, sizeof(password), false);
    have_password = true;
  }

     
                                                                          
              
     
  do
  {
    int argcount = 6;
    PQconninfoOption *conn_opt;
    char *err_msg = NULL;
    int i = 0;

    if (keywords)
    {
      free(keywords);
    }
    if (values)
    {
      free(values);
    }
    if (conn_opts)
    {
      PQconninfoFree(conn_opts);
    }

       
                                                                           
                                                                      
                                                                         
                                                       
       
    if (connection_string)
    {
      conn_opts = PQconninfoParse(connection_string, &err_msg);
      if (conn_opts == NULL)
      {
        pg_log_error("%s", err_msg);
        exit_nicely(1);
      }

      for (conn_opt = conn_opts; conn_opt->keyword != NULL; conn_opt++)
      {
        if (conn_opt->val != NULL && conn_opt->val[0] != '\0' && strcmp(conn_opt->keyword, "dbname") != 0)
        {
          argcount++;
        }
      }

      keywords = pg_malloc0((argcount + 1) * sizeof(*keywords));
      values = pg_malloc0((argcount + 1) * sizeof(*values));

      for (conn_opt = conn_opts; conn_opt->keyword != NULL; conn_opt++)
      {
        if (conn_opt->val != NULL && conn_opt->val[0] != '\0' && strcmp(conn_opt->keyword, "dbname") != 0)
        {
          keywords[i] = conn_opt->keyword;
          values[i] = conn_opt->val;
          i++;
        }
      }
    }
    else
    {
      keywords = pg_malloc0((argcount + 1) * sizeof(*keywords));
      values = pg_malloc0((argcount + 1) * sizeof(*values));
    }

    if (pghost)
    {
      keywords[i] = "host";
      values[i] = pghost;
      i++;
    }
    if (pgport)
    {
      keywords[i] = "port";
      values[i] = pgport;
      i++;
    }
    if (pguser)
    {
      keywords[i] = "user";
      values[i] = pguser;
      i++;
    }
    if (have_password)
    {
      keywords[i] = "password";
      values[i] = password;
      i++;
    }
    if (dbname)
    {
      keywords[i] = "dbname";
      values[i] = dbname;
      i++;
    }
    keywords[i] = "fallback_application_name";
    values[i] = progname;
    i++;

    new_pass = false;
    conn = PQconnectdbParams(keywords, values, true);

    if (!conn)
    {
      pg_log_error("could not connect to database \"%s\"", dbname);
      exit_nicely(1);
    }

    if (PQstatus(conn) == CONNECTION_BAD && PQconnectionNeedsPassword(conn) && !have_password && prompt_password != TRI_NO)
    {
      PQfinish(conn);
      simple_prompt("Password: ", password, sizeof(password), false);
      have_password = true;
      new_pass = true;
    }
  } while (new_pass);

                                                                      
  if (PQstatus(conn) == CONNECTION_BAD)
  {
    if (fail_on_error)
    {
      pg_log_error("could not connect to database \"%s\": %s", PQdb(conn) ? PQdb(conn) : "", PQerrorMessage(conn));
      exit_nicely(1);
    }
    else
    {
      PQfinish(conn);

      free(keywords);
      free(values);
      PQconninfoFree(conn_opts);

      return NULL;
    }
  }

     
                                                                             
                        
     
  connstr = constructConnStr(keywords, values);

  free(keywords);
  free(values);
  PQconninfoFree(conn_opts);

                     
  remoteversion_str = PQparameterStatus(conn, "server_version");
  if (!remoteversion_str)
  {
    pg_log_error("could not get server version");
    exit_nicely(1);
  }
  server_version = PQserverVersion(conn);
  if (server_version == 0)
  {
    pg_log_error("could not parse server version \"%s\"", remoteversion_str);
    exit_nicely(1);
  }

  my_version = PG_VERSION_NUM;

     
                                                                           
                                                                    
     
  if (my_version != server_version && (server_version < 80000 || (server_version / 100) > (my_version / 100)))
  {
    pg_log_error("server version: %s; %s version: %s", remoteversion_str, progname, PG_VERSION);
    pg_log_error("aborting because of server version mismatch");
    exit_nicely(1);
  }

  PQclear(executeQuery(conn, ALWAYS_SECURE_SEARCH_PATH_SQL));

  return conn;
}

              
                                                                           
                                                                  
   
                                          
                                               
                                                                     
                                                        
              
   
static char *
constructConnStr(const char **keywords, const char **values)
{
  PQExpBuffer buf = createPQExpBuffer();
  char *connstr;
  int i;
  bool firstkeyword = true;

                                                                
  for (i = 0; keywords[i] != NULL; i++)
  {
    if (strcmp(keywords[i], "dbname") == 0 || strcmp(keywords[i], "password") == 0 || strcmp(keywords[i], "fallback_application_name") == 0)
    {
      continue;
    }

    if (!firstkeyword)
    {
      appendPQExpBufferChar(buf, ' ');
    }
    firstkeyword = false;
    appendPQExpBuffer(buf, "%s=", keywords[i]);
    appendConnStrVal(buf, values[i]);
  }

  connstr = pg_strdup(buf->data);
  destroyPQExpBuffer(buf);
  return connstr;
}

   
                                                             
   
static PGresult *
executeQuery(PGconn *conn, const char *query)
{
  PGresult *res;

  pg_log_info("executing %s", query);

  res = PQexec(conn, query);
  if (!res || PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    pg_log_error("query failed: %s", PQerrorMessage(conn));
    pg_log_error("query was: %s", query);
    PQfinish(conn);
    exit_nicely(1);
  }

  return res;
}

   
                                                       
   
static void
executeCommand(PGconn *conn, const char *query)
{
  PGresult *res;

  pg_log_info("executing %s", query);

  res = PQexec(conn, query);
  if (!res || PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    pg_log_error("query failed: %s", PQerrorMessage(conn));
    pg_log_error("query was: %s", query);
    PQfinish(conn);
    exit_nicely(1);
  }

  PQclear(res);
}

   
                 
   
static void
dumpTimestamp(const char *msg)
{
  char buf[64];
  time_t now = time(NULL);

  if (strftime(buf, sizeof(buf), PGDUMP_STRFTIME_FMT, localtime(&now)) != 0)
  {
    fprintf(OPF, "-- %s %s\n\n", msg, buf);
  }
}
