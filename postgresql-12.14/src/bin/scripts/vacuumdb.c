                                                                            
   
            
   
                                                                         
                                                                        
   
                              
   
                                                                            
   

#ifdef WIN32
#define FD_SETSIZE 1024                                             
#endif

#include "postgres_fe.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "catalog/pg_class_d.h"

#include "common.h"
#include "common/logging.h"
#include "fe_utils/connect.h"
#include "fe_utils/simple_list.h"
#include "fe_utils/string_utils.h"

#define ERRCODE_UNDEFINED_TABLE "42P01"

                              
typedef struct ParallelSlot
{
  PGconn *connection;                     
  bool isFree;                                     
} ParallelSlot;

                                             
typedef struct vacuumingOptions
{
  bool analyze_only;
  bool verbose;
  bool and_analyze;
  bool full;
  bool freeze;
  bool disable_page_skipping;
  bool skip_locked;
  int min_xid_age;
  int min_mxid_age;
} vacuumingOptions;

static void
vacuum_one_database(const ConnParams *cparams, vacuumingOptions *vacopts, int stage, SimpleStringList *tables, int concurrentCons, const char *progname, bool echo, bool quiet);

static void
vacuum_all_databases(ConnParams *cparams, vacuumingOptions *vacopts, bool analyze_in_stages, int concurrentCons, const char *progname, bool echo, bool quiet);

static void
prepare_vacuum_command(PQExpBuffer sql, int serverVersion, vacuumingOptions *vacopts, const char *table);

static void
run_vacuum_command(PGconn *conn, const char *sql, bool echo, const char *table, const char *progname, bool async);

static ParallelSlot *
GetIdleSlot(ParallelSlot slots[], int numslots, const char *progname);

static bool
ProcessQueryResult(PGconn *conn, PGresult *result, const char *progname);

static bool
GetQueryResult(PGconn *conn, const char *progname);

static void
DisconnectDatabase(ParallelSlot *slot);

static int
select_loop(int maxFd, fd_set *workerset, bool *aborting);

static void
init_slot(ParallelSlot *slot, PGconn *conn);

static void
help(const char *progname);

                                
#define ANALYZE_NO_STAGE -1
#define ANALYZE_NUM_STAGES 3

int
main(int argc, char *argv[])
{
  static struct option long_options[] = {{"host", required_argument, NULL, 'h'}, {"port", required_argument, NULL, 'p'}, {"username", required_argument, NULL, 'U'}, {"no-password", no_argument, NULL, 'w'}, {"password", no_argument, NULL, 'W'}, {"echo", no_argument, NULL, 'e'}, {"quiet", no_argument, NULL, 'q'}, {"dbname", required_argument, NULL, 'd'}, {"analyze", no_argument, NULL, 'z'}, {"analyze-only", no_argument, NULL, 'Z'}, {"freeze", no_argument, NULL, 'F'}, {"all", no_argument, NULL, 'a'}, {"table", required_argument, NULL, 't'}, {"full", no_argument, NULL, 'f'}, {"verbose", no_argument, NULL, 'v'}, {"jobs", required_argument, NULL, 'j'}, {"maintenance-db", required_argument, NULL, 2}, {"analyze-in-stages", no_argument, NULL, 3}, {"disable-page-skipping", no_argument, NULL, 4}, {"skip-locked", no_argument, NULL, 5}, {"min-xid-age", required_argument, NULL, 6}, {"min-mxid-age", required_argument, NULL, 7}, {NULL, 0, NULL, 0}};

  const char *progname;
  int optindex;
  int c;
  const char *dbname = NULL;
  const char *maintenance_db = NULL;
  char *host = NULL;
  char *port = NULL;
  char *username = NULL;
  enum trivalue prompt_password = TRI_DEFAULT;
  ConnParams cparams;
  bool echo = false;
  bool quiet = false;
  vacuumingOptions vacopts;
  bool analyze_in_stages = false;
  bool alldb = false;
  SimpleStringList tables = {NULL, NULL};
  int concurrentCons = 1;
  int tbl_count = 0;

                                       
  memset(&vacopts, 0, sizeof(vacopts));

  pg_logging_init(argv[0]);
  progname = get_progname(argv[0]);
  set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pgscripts"));

  handle_help_version_opts(argc, argv, "vacuumdb", help);

  while ((c = getopt_long(argc, argv, "h:p:U:wWeqd:zZFat:fvj:", long_options, &optindex)) != -1)
  {
    switch (c)
    {
    case 'h':
      host = pg_strdup(optarg);
      break;
    case 'p':
      port = pg_strdup(optarg);
      break;
    case 'U':
      username = pg_strdup(optarg);
      break;
    case 'w':
      prompt_password = TRI_NO;
      break;
    case 'W':
      prompt_password = TRI_YES;
      break;
    case 'e':
      echo = true;
      break;
    case 'q':
      quiet = true;
      break;
    case 'd':
      dbname = pg_strdup(optarg);
      break;
    case 'z':
      vacopts.and_analyze = true;
      break;
    case 'Z':
      vacopts.analyze_only = true;
      break;
    case 'F':
      vacopts.freeze = true;
      break;
    case 'a':
      alldb = true;
      break;
    case 't':
    {
      simple_string_list_append(&tables, optarg);
      tbl_count++;
      break;
    }
    case 'f':
      vacopts.full = true;
      break;
    case 'v':
      vacopts.verbose = true;
      break;
    case 'j':
      concurrentCons = atoi(optarg);
      if (concurrentCons <= 0)
      {
        pg_log_error("number of parallel jobs must be at least 1");
        exit(1);
      }
      break;
    case 2:
      maintenance_db = pg_strdup(optarg);
      break;
    case 3:
      analyze_in_stages = vacopts.analyze_only = true;
      break;
    case 4:
      vacopts.disable_page_skipping = true;
      break;
    case 5:
      vacopts.skip_locked = true;
      break;
    case 6:
      vacopts.min_xid_age = atoi(optarg);
      if (vacopts.min_xid_age <= 0)
      {
        pg_log_error("minimum transaction ID age must be at least 1");
        exit(1);
      }
      break;
    case 7:
      vacopts.min_mxid_age = atoi(optarg);
      if (vacopts.min_mxid_age <= 0)
      {
        pg_log_error("minimum multixact ID age must be at least 1");
        exit(1);
      }
      break;
    default:
      fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
      exit(1);
    }
  }

     
                                                                      
                                          
     
  if (optind < argc && dbname == NULL)
  {
    dbname = argv[optind];
    optind++;
  }

  if (optind < argc)
  {
    pg_log_error("too many command-line arguments (first is \"%s\")", argv[optind]);
    fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
    exit(1);
  }

  if (vacopts.analyze_only)
  {
    if (vacopts.full)
    {
      pg_log_error("cannot use the \"%s\" option when performing only analyze", "full");
      exit(1);
    }
    if (vacopts.freeze)
    {
      pg_log_error("cannot use the \"%s\" option when performing only analyze", "freeze");
      exit(1);
    }
    if (vacopts.disable_page_skipping)
    {
      pg_log_error("cannot use the \"%s\" option when performing only analyze", "disable-page-skipping");
      exit(1);
    }
                                                 
  }

                                                          
  cparams.pghost = host;
  cparams.pgport = port;
  cparams.pguser = username;
  cparams.prompt_password = prompt_password;
  cparams.override_dbname = NULL;

  setup_cancel_handler();

                                        
  if (tbl_count && (concurrentCons > tbl_count))
  {
    concurrentCons = tbl_count;
  }

  if (alldb)
  {
    if (dbname)
    {
      pg_log_error("cannot vacuum all databases and a specific one at the same time");
      exit(1);
    }
    if (tables.head != NULL)
    {
      pg_log_error("cannot vacuum specific table(s) in all databases");
      exit(1);
    }

    cparams.dbname = maintenance_db;

    vacuum_all_databases(&cparams, &vacopts, analyze_in_stages, concurrentCons, progname, echo, quiet);
  }
  else
  {
    if (dbname == NULL)
    {
      if (getenv("PGDATABASE"))
      {
        dbname = getenv("PGDATABASE");
      }
      else if (getenv("PGUSER"))
      {
        dbname = getenv("PGUSER");
      }
      else
      {
        dbname = get_user_name_or_exit(progname);
      }
    }

    cparams.dbname = dbname;

    if (analyze_in_stages)
    {
      int stage;

      for (stage = 0; stage < ANALYZE_NUM_STAGES; stage++)
      {
        vacuum_one_database(&cparams, &vacopts, stage, &tables, concurrentCons, progname, echo, quiet);
      }
    }
    else
    {
      vacuum_one_database(&cparams, &vacopts, ANALYZE_NO_STAGE, &tables, concurrentCons, progname, echo, quiet);
    }
  }

  exit(0);
}

   
                       
   
                                                                         
                                       
   
                                                                            
                                                                           
   
                                                                            
                                                                              
                                       
   
static void
vacuum_one_database(const ConnParams *cparams, vacuumingOptions *vacopts, int stage, SimpleStringList *tables, int concurrentCons, const char *progname, bool echo, bool quiet)
{
  PQExpBufferData sql;
  PQExpBufferData buf;
  PQExpBufferData catalog_query;
  PGresult *res;
  PGconn *conn;
  SimpleStringListCell *cell;
  ParallelSlot *slots;
  SimpleStringList dbtables = {NULL, NULL};
  int i;
  int ntups;
  bool failed = false;
  bool parallel = concurrentCons > 1;
  bool tables_listed = false;
  bool has_where = false;
  const char *stage_commands[] = {"SET default_statistics_target=1; SET vacuum_cost_delay=0;", "SET default_statistics_target=10; RESET vacuum_cost_delay;", "RESET default_statistics_target;"};
  const char *stage_messages[] = {gettext_noop("Generating minimal optimizer statistics (1 target)"), gettext_noop("Generating medium optimizer statistics (10 targets)"), gettext_noop("Generating default (full) optimizer statistics")};

  Assert(stage == ANALYZE_NO_STAGE || (stage >= 0 && stage < ANALYZE_NUM_STAGES));

  conn = connectDatabase(cparams, progname, echo, false, true);

  if (vacopts->disable_page_skipping && PQserverVersion(conn) < 90600)
  {
    PQfinish(conn);
    pg_log_error("cannot use the \"%s\" option on server versions older than PostgreSQL %s", "disable-page-skipping", "9.6");
    exit(1);
  }

  if (vacopts->skip_locked && PQserverVersion(conn) < 120000)
  {
    PQfinish(conn);
    pg_log_error("cannot use the \"%s\" option on server versions older than PostgreSQL %s", "skip-locked", "12");
    exit(1);
  }

  if (vacopts->min_xid_age != 0 && PQserverVersion(conn) < 90600)
  {
    pg_log_error("cannot use the \"%s\" option on server versions older than PostgreSQL %s", "--min-xid-age", "9.6");
    exit(1);
  }

  if (vacopts->min_mxid_age != 0 && PQserverVersion(conn) < 90600)
  {
    pg_log_error("cannot use the \"%s\" option on server versions older than PostgreSQL %s", "--min-mxid-age", "9.6");
    exit(1);
  }

  if (!quiet)
  {
    if (stage != ANALYZE_NO_STAGE)
    {
      printf(_("%s: processing database \"%s\": %s\n"), progname, PQdb(conn), _(stage_messages[stage]));
    }
    else
    {
      printf(_("%s: vacuuming database \"%s\"\n"), progname, PQdb(conn));
    }
    fflush(stdout);
  }

     
                                                                     
     
                                                                         
                                                                     
                
     
                                                                         
                                                                      
                                                                         
                                                                          
                                                                         
                              
     
  initPQExpBuffer(&catalog_query);
  for (cell = tables ? tables->head : NULL; cell; cell = cell->next)
  {
    char *just_table;
    const char *just_columns;

       
                                                                          
                                                                        
                                                                        
       
    splitTableColumnsSpec(cell->val, PQclientEncoding(conn), &just_table, &just_columns);

    if (!tables_listed)
    {
      appendPQExpBuffer(&catalog_query, "WITH listed_tables (table_oid, column_list) "
                                        "AS (\n  VALUES (");
      tables_listed = true;
    }
    else
    {
      appendPQExpBuffer(&catalog_query, ",\n  (");
    }

    appendStringLiteralConn(&catalog_query, just_table, conn);
    appendPQExpBuffer(&catalog_query, "::pg_catalog.regclass, ");

    if (just_columns && just_columns[0] != '\0')
    {
      appendStringLiteralConn(&catalog_query, just_columns, conn);
    }
    else
    {
      appendPQExpBufferStr(&catalog_query, "NULL");
    }

    appendPQExpBufferStr(&catalog_query, "::pg_catalog.text)");

    pg_free(just_table);
  }

                                 
  if (tables_listed)
  {
    appendPQExpBuffer(&catalog_query, "\n)\n");
  }

  appendPQExpBuffer(&catalog_query, "SELECT c.relname, ns.nspname");

  if (tables_listed)
  {
    appendPQExpBuffer(&catalog_query, ", listed_tables.column_list");
  }

  appendPQExpBuffer(&catalog_query, " FROM pg_catalog.pg_class c\n"
                                    " JOIN pg_catalog.pg_namespace ns"
                                    " ON c.relnamespace OPERATOR(pg_catalog.=) ns.oid\n"
                                    " LEFT JOIN pg_catalog.pg_class t"
                                    " ON c.reltoastrelid OPERATOR(pg_catalog.=) t.oid\n");

                                                   
  if (tables_listed)
  {
    appendPQExpBuffer(&catalog_query, " JOIN listed_tables"
                                      " ON listed_tables.table_oid OPERATOR(pg_catalog.=) c.oid\n");
  }

     
                                                                           
                                                                             
                                                                    
                                                          
     
  if (!tables_listed)
  {
    appendPQExpBuffer(&catalog_query, " WHERE c.relkind OPERATOR(pg_catalog.=) ANY (array[" CppAsString2(RELKIND_RELATION) ", " CppAsString2(RELKIND_MATVIEW) "])\n");
    has_where = true;
  }

     
                                                                          
                                                                        
                                                                            
                                                                    
                            
     
  if (vacopts->min_xid_age != 0)
  {
    appendPQExpBuffer(&catalog_query,
        " %s GREATEST(pg_catalog.age(c.relfrozenxid),"
        " pg_catalog.age(t.relfrozenxid)) "
        " OPERATOR(pg_catalog.>=) '%d'::pg_catalog.int4\n"
        " AND c.relfrozenxid OPERATOR(pg_catalog.!=)"
        " '0'::pg_catalog.xid\n",
        has_where ? "AND" : "WHERE", vacopts->min_xid_age);
    has_where = true;
  }

  if (vacopts->min_mxid_age != 0)
  {
    appendPQExpBuffer(&catalog_query,
        " %s GREATEST(pg_catalog.mxid_age(c.relminmxid),"
        " pg_catalog.mxid_age(t.relminmxid)) OPERATOR(pg_catalog.>=)"
        " '%d'::pg_catalog.int4\n"
        " AND c.relminmxid OPERATOR(pg_catalog.!=)"
        " '0'::pg_catalog.xid\n",
        has_where ? "AND" : "WHERE", vacopts->min_mxid_age);
    has_where = true;
  }

     
                                                                         
                                                                          
     
  appendPQExpBuffer(&catalog_query, " ORDER BY c.relpages DESC;");
  executeCommand(conn, "RESET search_path;", progname, echo);
  res = executeQuery(conn, catalog_query.data, progname, echo);
  termPQExpBuffer(&catalog_query);
  PQclear(executeQuery(conn, ALWAYS_SECURE_SEARCH_PATH_SQL, progname, echo));

     
                                                                            
     
  ntups = PQntuples(res);
  if (ntups == 0)
  {
    PQclear(res);
    PQfinish(conn);
    return;
  }

     
                                                                           
               
     
  initPQExpBuffer(&buf);
  for (i = 0; i < ntups; i++)
  {
    appendPQExpBufferStr(&buf, fmtQualifiedId(PQgetvalue(res, i, 1), PQgetvalue(res, i, 0)));

    if (tables_listed && !PQgetisnull(res, i, 2))
    {
      appendPQExpBufferStr(&buf, PQgetvalue(res, i, 2));
    }

    simple_string_list_append(&dbtables, buf.data);
    resetPQExpBuffer(&buf);
  }
  termPQExpBuffer(&buf);
  PQclear(res);

     
                                                                            
                      
     
  if (parallel)
  {
    if (concurrentCons > ntups)
    {
      concurrentCons = ntups;
    }
    if (concurrentCons <= 1)
    {
      parallel = false;
    }
  }

     
                                                                             
                                                                         
                                    
     
  if (concurrentCons <= 0)
  {
    concurrentCons = 1;
  }
  slots = (ParallelSlot *)pg_malloc(sizeof(ParallelSlot) * concurrentCons);
  init_slot(slots, conn);
  if (parallel)
  {
    for (i = 1; i < concurrentCons; i++)
    {
      conn = connectDatabase(cparams, progname, echo, false, true);

         
                                                                   
                                                                      
                                                                   
         
      if (PQsocket(conn) >= FD_SETSIZE)
      {
        pg_log_fatal("too many jobs for this platform -- try %d", i);
        exit(1);
      }

      init_slot(slots + i, conn);
    }
  }

     
                                                                          
                                 
     
  if (stage != ANALYZE_NO_STAGE)
  {
    int j;

                                              

    for (j = 0; j < concurrentCons; j++)
    {
      executeCommand((slots + j)->connection, stage_commands[stage], progname, echo);
    }
  }

  initPQExpBuffer(&sql);

  cell = dbtables.head;
  do
  {
    const char *tabname = cell->val;
    ParallelSlot *free_slot;

    if (CancelRequested)
    {
      failed = true;
      goto finish;
    }

       
                                                                          
                                                                      
                                                                       
                        
       
    if (parallel)
    {
         
                                                                 
                       
         
      free_slot = GetIdleSlot(slots, concurrentCons, progname);
      if (!free_slot)
      {
        failed = true;
        goto finish;
      }

      free_slot->isFree = false;
    }
    else
    {
      free_slot = slots;
    }

    prepare_vacuum_command(&sql, PQserverVersion(free_slot->connection), vacopts, tabname);

       
                                                                         
                                                                      
                                                          
       
    run_vacuum_command(free_slot->connection, sql.data, echo, tabname, progname, parallel);

    cell = cell->next;
  } while (cell != NULL);

  if (parallel)
  {
    int j;

                                            
    for (j = 0; j < concurrentCons; j++)
    {
      if (!GetQueryResult((slots + j)->connection, progname))
      {
        failed = true;
        goto finish;
      }
    }
  }

finish:
  for (i = 0; i < concurrentCons; i++)
  {
    DisconnectDatabase(slots + i);
  }
  pfree(slots);

  termPQExpBuffer(&sql);

  if (failed)
  {
    exit(1);
  }
}

   
                                             
   
                                                                           
                                                                         
                                                            
   
static void
vacuum_all_databases(ConnParams *cparams, vacuumingOptions *vacopts, bool analyze_in_stages, int concurrentCons, const char *progname, bool echo, bool quiet)
{
  PGconn *conn;
  PGresult *result;
  int stage;
  int i;

  conn = connectMaintenanceDatabase(cparams, progname, echo);
  result = executeQuery(conn, "SELECT datname FROM pg_database WHERE datallowconn ORDER BY 1;", progname, echo);
  PQfinish(conn);

  if (analyze_in_stages)
  {
       
                                                                          
                                                                        
                                            
       
                                                                      
                                         
       
    for (stage = 0; stage < ANALYZE_NUM_STAGES; stage++)
    {
      for (i = 0; i < PQntuples(result); i++)
      {
        cparams->override_dbname = PQgetvalue(result, i, 0);

        vacuum_one_database(cparams, vacopts, stage, NULL, concurrentCons, progname, echo, quiet);
      }
    }
  }
  else
  {
    for (i = 0; i < PQntuples(result); i++)
    {
      cparams->override_dbname = PQgetvalue(result, i, 0);

      vacuum_one_database(cparams, vacopts, ANALYZE_NO_STAGE, NULL, concurrentCons, progname, echo, quiet);
    }
  }

  PQclear(result);
}

   
                                                                                
                                                            
   
                                                                               
                                                                          
   
static void
prepare_vacuum_command(PQExpBuffer sql, int serverVersion, vacuumingOptions *vacopts, const char *table)
{
  const char *paren = " (";
  const char *comma = ", ";
  const char *sep = paren;

  resetPQExpBuffer(sql);

  if (vacopts->analyze_only)
  {
    appendPQExpBufferStr(sql, "ANALYZE");

                                                                 
    if (serverVersion >= 110000)
    {
      if (vacopts->skip_locked)
      {
                                                
        Assert(serverVersion >= 120000);
        appendPQExpBuffer(sql, "%sSKIP_LOCKED", sep);
        sep = comma;
      }
      if (vacopts->verbose)
      {
        appendPQExpBuffer(sql, "%sVERBOSE", sep);
        sep = comma;
      }
      if (sep != paren)
      {
        appendPQExpBufferChar(sql, ')');
      }
    }
    else
    {
      if (vacopts->verbose)
      {
        appendPQExpBufferStr(sql, " VERBOSE");
      }
    }
  }
  else
  {
    appendPQExpBufferStr(sql, "VACUUM");

                                                                 
    if (serverVersion >= 90000)
    {
      if (vacopts->disable_page_skipping)
      {
                                                           
        Assert(serverVersion >= 90600);
        appendPQExpBuffer(sql, "%sDISABLE_PAGE_SKIPPING", sep);
        sep = comma;
      }
      if (vacopts->skip_locked)
      {
                                                
        Assert(serverVersion >= 120000);
        appendPQExpBuffer(sql, "%sSKIP_LOCKED", sep);
        sep = comma;
      }
      if (vacopts->full)
      {
        appendPQExpBuffer(sql, "%sFULL", sep);
        sep = comma;
      }
      if (vacopts->freeze)
      {
        appendPQExpBuffer(sql, "%sFREEZE", sep);
        sep = comma;
      }
      if (vacopts->verbose)
      {
        appendPQExpBuffer(sql, "%sVERBOSE", sep);
        sep = comma;
      }
      if (vacopts->and_analyze)
      {
        appendPQExpBuffer(sql, "%sANALYZE", sep);
        sep = comma;
      }
      if (sep != paren)
      {
        appendPQExpBufferChar(sql, ')');
      }
    }
    else
    {
      if (vacopts->full)
      {
        appendPQExpBufferStr(sql, " FULL");
      }
      if (vacopts->freeze)
      {
        appendPQExpBufferStr(sql, " FREEZE");
      }
      if (vacopts->verbose)
      {
        appendPQExpBufferStr(sql, " VERBOSE");
      }
      if (vacopts->and_analyze)
      {
        appendPQExpBufferStr(sql, " ANALYZE");
      }
    }
  }

  appendPQExpBuffer(sql, " %s;", table);
}

   
                                                                             
                                                     
   
                                                                            
                                                                     
   
static void
run_vacuum_command(PGconn *conn, const char *sql, bool echo, const char *table, const char *progname, bool async)
{
  bool status;

  if (async)
  {
    if (echo)
    {
      printf("%s\n", sql);
    }

    status = PQsendQuery(conn, sql) == 1;
  }
  else
  {
    status = executeMaintenanceCommand(conn, sql, echo);
  }

  if (!status)
  {
    if (table)
    {
      pg_log_error("vacuuming of table \"%s\" in database \"%s\" failed: %s", table, PQdb(conn), PQerrorMessage(conn));
    }
    else
    {
      pg_log_error("vacuuming of database \"%s\" failed: %s", PQdb(conn), PQerrorMessage(conn));
    }

    if (!async)
    {
      PQfinish(conn);
      exit(1);
    }
  }
}

   
               
                                                                 
   
                                                                      
                                                                            
                                                                                
              
   
                                         
   
static ParallelSlot *
GetIdleSlot(ParallelSlot slots[], int numslots, const char *progname)
{
  int i;
  int firstFree = -1;

                                          
  for (i = 0; i < numslots; i++)
  {
    if (slots[i].isFree)
    {
      return slots + i;
    }
  }

     
                                                                           
                                             
     
  while (firstFree < 0)
  {
    fd_set slotset;
    int maxFd = 0;
    bool aborting;

                                                                     
    FD_ZERO(&slotset);

    for (i = 0; i < numslots; i++)
    {
      int sock = PQsocket(slots[i].connection);

         
                                                                      
                                                                 
         
      if (sock < 0)
      {
        continue;
      }

      FD_SET(sock, &slotset);
      if (sock > maxFd)
      {
        maxFd = sock;
      }
    }

    SetCancelConn(slots->connection);
    i = select_loop(maxFd, &slotset, &aborting);
    ResetCancelConn();

    if (aborting)
    {
         
                                                                         
                                                    
         
      GetQueryResult(slots->connection, progname);
      return NULL;
    }
    Assert(i != 0);

    for (i = 0; i < numslots; i++)
    {
      int sock = PQsocket(slots[i].connection);

      if (sock >= 0 && FD_ISSET(sock, &slotset))
      {
                                                             
        PQconsumeInput(slots[i].connection);
      }

                                                          
      while (!PQisBusy(slots[i].connection))
      {
        PGresult *result = PQgetResult(slots[i].connection);

        if (result != NULL)
        {
                                                    
          if (!ProcessQueryResult(slots[i].connection, result, progname))
          {
            return NULL;
          }
        }
        else
        {
                                               
          slots[i].isFree = true;
          if (firstFree < 0)
          {
            firstFree = i;
          }
          break;
        }
      }
    }
  }

  return slots + firstFree;
}

   
                      
   
                                                                           
                                                                           
                                          
   
static bool
ProcessQueryResult(PGconn *conn, PGresult *result, const char *progname)
{
     
                                                                             
                                                          
     
  if (PQresultStatus(result) != PGRES_COMMAND_OK)
  {
    char *sqlState = PQresultErrorField(result, PG_DIAG_SQLSTATE);

    pg_log_error("vacuuming of database \"%s\" failed: %s", PQdb(conn), PQerrorMessage(conn));

    if (sqlState && strcmp(sqlState, ERRCODE_UNDEFINED_TABLE) != 0)
    {
      PQclear(result);
      return false;
    }
  }

  PQclear(result);
  return true;
}

   
                  
   
                                                                           
                                                  
   
static bool
GetQueryResult(PGconn *conn, const char *progname)
{
  bool ok = true;
  PGresult *result;

  SetCancelConn(conn);
  while ((result = PQgetResult(conn)) != NULL)
  {
    if (!ProcessQueryResult(conn, result, progname))
    {
      ok = false;
    }
  }
  ResetCancelConn();
  return ok;
}

   
                      
                                                             
   
static void
DisconnectDatabase(ParallelSlot *slot)
{
  char errbuf[256];

  if (!slot->connection)
  {
    return;
  }

  if (PQtransactionStatus(slot->connection) == PQTRANS_ACTIVE)
  {
    PGcancel *cancel;

    if ((cancel = PQgetCancel(slot->connection)))
    {
      (void)PQcancel(cancel, errbuf, sizeof(errbuf));
      PQfreeCancel(cancel);
    }
  }

  PQfinish(slot->connection);
  slot->connection = NULL;
}

   
                                                                            
   
                                                                         
                                                                            
                                                                
   
static int
select_loop(int maxFd, fd_set *workerset, bool *aborting)
{
  int i;
  fd_set saveSet = *workerset;

  if (CancelRequested)
  {
    *aborting = true;
    return -1;
  }
  else
  {
    *aborting = false;
  }

  for (;;)
  {
       
                                                                         
                                                                          
       
    struct timeval *tvp;
#ifdef WIN32
    struct timeval tv = {0, 1000000};

    tvp = &tv;
#else
    tvp = NULL;
#endif

    *workerset = saveSet;
    i = select(maxFd + 1, workerset, NULL, NULL, tvp);

#ifdef WIN32
    if (i == SOCKET_ERROR)
    {
      i = -1;

      if (WSAGetLastError() == WSAEINTR)
      {
        errno = EINTR;
      }
    }
#endif

    if (i < 0 && errno == EINTR)
    {
      continue;                  
    }
    if (i < 0 || CancelRequested)
    {
      *aborting = true;                   
    }
    if (i == 0)
    {
      continue;                           
    }
    break;
  }

  return i;
}

static void
init_slot(ParallelSlot *slot, PGconn *conn)
{
  slot->connection = conn;
                                           
  slot->isFree = true;
}

static void
help(const char *progname)
{
  printf(_("%s cleans and analyzes a PostgreSQL database.\n\n"), progname);
  printf(_("Usage:\n"));
  printf(_("  %s [OPTION]... [DBNAME]\n"), progname);
  printf(_("\nOptions:\n"));
  printf(_("  -a, --all                       vacuum all databases\n"));
  printf(_("  -d, --dbname=DBNAME             database to vacuum\n"));
  printf(_("      --disable-page-skipping     disable all page-skipping behavior\n"));
  printf(_("  -e, --echo                      show the commands being sent to the server\n"));
  printf(_("  -f, --full                      do full vacuuming\n"));
  printf(_("  -F, --freeze                    freeze row transaction information\n"));
  printf(_("  -j, --jobs=NUM                  use this many concurrent connections to vacuum\n"));
  printf(_("      --min-mxid-age=MXID_AGE     minimum multixact ID age of tables to vacuum\n"));
  printf(_("      --min-xid-age=XID_AGE       minimum transaction ID age of tables to vacuum\n"));
  printf(_("  -q, --quiet                     don't write any messages\n"));
  printf(_("      --skip-locked               skip relations that cannot be immediately locked\n"));
  printf(_("  -t, --table='TABLE[(COLUMNS)]'  vacuum specific table(s) only\n"));
  printf(_("  -v, --verbose                   write a lot of output\n"));
  printf(_("  -V, --version                   output version information, then exit\n"));
  printf(_("  -z, --analyze                   update optimizer statistics\n"));
  printf(_("  -Z, --analyze-only              only update optimizer statistics; no vacuum\n"));
  printf(_("      --analyze-in-stages         only update optimizer statistics, in multiple\n"
           "                                  stages for faster results; no vacuum\n"));
  printf(_("  -?, --help                      show this help, then exit\n"));
  printf(_("\nConnection options:\n"));
  printf(_("  -h, --host=HOSTNAME       database server host or socket directory\n"));
  printf(_("  -p, --port=PORT           database server port\n"));
  printf(_("  -U, --username=USERNAME   user name to connect as\n"));
  printf(_("  -w, --no-password         never prompt for password\n"));
  printf(_("  -W, --password            force password prompt\n"));
  printf(_("  --maintenance-db=DBNAME   alternate maintenance database\n"));
  printf(_("\nRead the description of the SQL command VACUUM for details.\n"));
  printf(_("\nReport bugs to <pgsql-bugs@lists.postgresql.org>.\n"));
}
