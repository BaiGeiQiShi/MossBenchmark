                                                                            
   
            
                                             
   
   
                                                                         
                                                                        
   
                            
   
                                                                            
   

#include "postgres_fe.h"

#include <signal.h>
#include <unistd.h>

#include "common.h"
#include "common/logging.h"
#include "fe_utils/connect.h"
#include "fe_utils/string_utils.h"

   
                                                                         
                                                                             
                                                           
   
#define write_stderr(str)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    const char *str_ = (str);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    int rc_;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    rc_ = write(fileno(stderr), str_, strlen(str_));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    (void)rc_;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

#define PQmblenBounded(s, e) strnlen(s, PQmblen(s, e))

static PGcancel *volatile cancelConn = NULL;
bool CancelRequested = false;

#ifdef WIN32
static CRITICAL_SECTION cancelConnLock;
#endif

   
                                                                
            
   
void
handle_help_version_opts(int argc, char *argv[], const char *fixed_progname, help_handler hlp)
{
  if (argc > 1)
  {
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
    {
      hlp(get_progname(argv[0]));
      exit(0);
    }
    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
    {
      printf("%s (PostgreSQL) " PG_VERSION "\n", fixed_progname);
      exit(0);
    }
  }
}

   
                                                         
   
                                                                        
                                        
   
                                                                       
                                                                          
                                                                           
                                                               
   
PGconn *
connectDatabase(const ConnParams *cparams, const char *progname, bool echo, bool fail_ok, bool allow_password_reuse)
{
  PGconn *conn;
  bool new_pass;
  static bool have_password = false;
  static char password[100];

                                                                     
  Assert(cparams->dbname);

  if (!allow_password_reuse)
  {
    have_password = false;
  }

  if (cparams->prompt_password == TRI_YES && !have_password)
  {
    simple_prompt("Password: ", password, sizeof(password), false);
    have_password = true;
  }

     
                                                                          
              
     
  do
  {
    const char *keywords[8];
    const char *values[8];
    int i = 0;

       
                                                                     
                                                                      
                                            
       
    keywords[i] = "host";
    values[i++] = cparams->pghost;
    keywords[i] = "port";
    values[i++] = cparams->pgport;
    keywords[i] = "user";
    values[i++] = cparams->pguser;
    keywords[i] = "password";
    values[i++] = have_password ? password : NULL;
    keywords[i] = "dbname";
    values[i++] = cparams->dbname;
    if (cparams->override_dbname)
    {
      keywords[i] = "dbname";
      values[i++] = cparams->override_dbname;
    }
    keywords[i] = "fallback_application_name";
    values[i++] = progname;
    keywords[i] = NULL;
    values[i++] = NULL;
    Assert(i <= lengthof(keywords));

    new_pass = false;
    conn = PQconnectdbParams(keywords, values, true);

    if (!conn)
    {
      pg_log_error("could not connect to database %s: out of memory", cparams->dbname);
      exit(1);
    }

       
                                                       
       
    if (PQstatus(conn) == CONNECTION_BAD && PQconnectionNeedsPassword(conn) && cparams->prompt_password != TRI_NO)
    {
      PQfinish(conn);
      simple_prompt("Password: ", password, sizeof(password), false);
      have_password = true;
      new_pass = true;
    }
  } while (new_pass);

                                                                      
  if (PQstatus(conn) == CONNECTION_BAD)
  {
    if (fail_ok)
    {
      PQfinish(conn);
      return NULL;
    }
    pg_log_error("could not connect to database %s: %s", cparams->dbname, PQerrorMessage(conn));
    exit(1);
  }

                                                
  PQclear(executeQuery(conn, ALWAYS_SECURE_SEARCH_PATH_SQL, progname, echo));

  return conn;
}

   
                                                           
   
                                                                    
                                                                        
                                                                        
                                              
   
PGconn *
connectMaintenanceDatabase(ConnParams *cparams, const char *progname, bool echo)
{
  PGconn *conn;

                                                                         
  if (cparams->dbname)
  {
    return connectDatabase(cparams, progname, echo, false, false);
  }

                                                         
  cparams->dbname = "postgres";
  conn = connectDatabase(cparams, progname, echo, true, false);
  if (!conn)
  {
    cparams->dbname = "template1";
    conn = connectDatabase(cparams, progname, echo, false, false);
  }
  return conn;
}

   
                                                             
   
PGresult *
executeQuery(PGconn *conn, const char *query, const char *progname, bool echo)
{
  PGresult *res;

  if (echo)
  {
    printf("%s\n", query);
  }

  res = PQexec(conn, query);
  if (!res || PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    pg_log_error("query failed: %s", PQerrorMessage(conn));
    pg_log_info("query was: %s", query);
    PQfinish(conn);
    exit(1);
  }

  return res;
}

   
                                                       
   
void
executeCommand(PGconn *conn, const char *query, const char *progname, bool echo)
{
  PGresult *res;

  if (echo)
  {
    printf("%s\n", query);
  }

  res = PQexec(conn, query);
  if (!res || PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    pg_log_error("query failed: %s", PQerrorMessage(conn));
    pg_log_info("query was: %s", query);
    PQfinish(conn);
    exit(1);
  }

  PQclear(res);
}

   
                                                                     
                                                                
                 
   
bool
executeMaintenanceCommand(PGconn *conn, const char *query, bool echo)
{
  PGresult *res;
  bool r;

  if (echo)
  {
    printf("%s\n", query);
  }

  SetCancelConn(conn);
  res = PQexec(conn, query);
  ResetCancelConn();

  r = (res && PQresultStatus(res) == PGRES_COMMAND_OK);

  if (res)
  {
    PQclear(res);
  }

  return r;
}

   
                                                                         
                                                                           
                                   
   
void
splitTableColumnsSpec(const char *spec, int encoding, char **table, const char **columns)
{
  bool inquotes = false;
  const char *cp = spec;

     
                                                         
                                    
     
  while (*cp && (*cp != '(' || inquotes))
  {
    if (*cp == '"')
    {
      if (inquotes && cp[1] == '"')
      {
        cp++;                                   
      }
      else
      {
        inquotes = !inquotes;
      }
      cp++;
    }
    else
    {
      cp += PQmblenBounded(cp, encoding);
    }
  }
  *table = pg_strdup(spec);
  (*table)[cp - spec] = '\0';                 
  *columns = cp;
}

   
                                                                              
                                                                              
                                                                             
                                                                              
                                                                           
   
void
appendQualifiedRelation(PQExpBuffer buf, const char *spec, PGconn *conn, const char *progname, bool echo)
{
  char *table;
  const char *columns;
  PQExpBufferData sql;
  PGresult *res;
  int ntups;

  splitTableColumnsSpec(spec, PQclientEncoding(conn), &table, &columns);

     
                                                                           
                                                                      
               
     
  initPQExpBuffer(&sql);
  appendPQExpBufferStr(&sql, "SELECT c.relname, ns.nspname\n"
                             " FROM pg_catalog.pg_class c,"
                             " pg_catalog.pg_namespace ns\n"
                             " WHERE c.relnamespace OPERATOR(pg_catalog.=) ns.oid\n"
                             "  AND c.oid OPERATOR(pg_catalog.=) ");
  appendStringLiteralConn(&sql, table, conn);
  appendPQExpBufferStr(&sql, "::pg_catalog.regclass;");

  executeCommand(conn, "RESET search_path;", progname, echo);

     
                                                                      
                                                                            
                                                                            
                                    
     
  res = executeQuery(conn, sql.data, progname, echo);
  ntups = PQntuples(res);
  if (ntups != 1)
  {
    pg_log_error(ngettext("query returned %d row instead of one: %s", "query returned %d rows instead of one: %s", ntups), ntups, sql.data);
    PQfinish(conn);
    exit(1);
  }
  appendPQExpBufferStr(buf, fmtQualifiedId(PQgetvalue(res, 0, 1), PQgetvalue(res, 0, 0)));
  appendPQExpBufferStr(buf, columns);
  PQclear(res);
  termPQExpBuffer(&sql);
  pg_free(table);

  PQclear(executeQuery(conn, ALWAYS_SECURE_SEARCH_PATH_SQL, progname, echo));
}

   
                                                                     
   

                                        
#define PG_YESLETTER gettext_noop("y")
                                       
#define PG_NOLETTER gettext_noop("n")

bool
yesno_prompt(const char *question)
{
  char prompt[256];

           
                                                                           
                       
  snprintf(prompt, sizeof(prompt), _("%s (%s/%s) "), _(question), _(PG_YESLETTER), _(PG_NOLETTER));

  for (;;)
  {
    char resp[10];

    simple_prompt(prompt, resp, sizeof(resp), true);

    if (strcmp(resp, _(PG_YESLETTER)) == 0)
    {
      return true;
    }
    if (strcmp(resp, _(PG_NOLETTER)) == 0)
    {
      return false;
    }

    printf(_("Please answer \"%s\" or \"%s\".\n"), _(PG_YESLETTER), _(PG_NOLETTER));
  }
}

   
                 
   
                                                               
   
void
SetCancelConn(PGconn *conn)
{
  PGcancel *oldCancelConn;

#ifdef WIN32
  EnterCriticalSection(&cancelConnLock);
#endif

                                       
  oldCancelConn = cancelConn;

                                                               
  cancelConn = NULL;

  if (oldCancelConn != NULL)
  {
    PQfreeCancel(oldCancelConn);
  }

  cancelConn = PQgetCancel(conn);

#ifdef WIN32
  LeaveCriticalSection(&cancelConnLock);
#endif
}

   
                   
   
                                                                
   
void
ResetCancelConn(void)
{
  PGcancel *oldCancelConn;

#ifdef WIN32
  EnterCriticalSection(&cancelConnLock);
#endif

  oldCancelConn = cancelConn;

                                                               
  cancelConn = NULL;

  if (oldCancelConn != NULL)
  {
    PQfreeCancel(oldCancelConn);
  }

#ifdef WIN32
  LeaveCriticalSection(&cancelConnLock);
#endif
}

#ifndef WIN32
   
                                                                              
           
   
static void
handle_sigint(SIGNAL_ARGS)
{
  int save_errno = errno;
  char errbuf[256];

                                                              
  if (cancelConn != NULL)
  {
    if (PQcancel(cancelConn, errbuf, sizeof(errbuf)))
    {
      CancelRequested = true;
      write_stderr("Cancel request sent\n");
    }
    else
    {
      write_stderr("Could not send cancel request: ");
      write_stderr(errbuf);
    }
  }
  else
  {
    CancelRequested = true;
  }

  errno = save_errno;                                        
}

void
setup_cancel_handler(void)
{
  pqsignal(SIGINT, handle_sigint);
}
#else            

   
                                                                         
                                                                       
                                           
   
static BOOL WINAPI
consoleHandler(DWORD dwCtrlType)
{
  char errbuf[256];

  if (dwCtrlType == CTRL_C_EVENT || dwCtrlType == CTRL_BREAK_EVENT)
  {
                                                                
    EnterCriticalSection(&cancelConnLock);
    if (cancelConn != NULL)
    {
      if (PQcancel(cancelConn, errbuf, sizeof(errbuf)))
      {
        CancelRequested = true;
        write_stderr("Cancel request sent\n");
      }
      else
      {
        write_stderr("Could not send cancel request: ");
        write_stderr(errbuf);
      }
    }
    else
    {
      CancelRequested = true;
    }

    LeaveCriticalSection(&cancelConnLock);

    return TRUE;
  }
  else
  {
                                                        
    return FALSE;
  }
}

void
setup_cancel_handler(void)
{
  InitializeCriticalSection(&cancelConnLock);

  SetConsoleCtrlHandler(consoleHandler, TRUE);
}

#endif            
