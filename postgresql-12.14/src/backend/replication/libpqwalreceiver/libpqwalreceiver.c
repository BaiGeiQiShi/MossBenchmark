                                                                            
   
                      
   
                                                                    
                                                                           
          
   
                                                                         
   
   
                  
                                                                 
   
                                                                            
   
#include "postgres.h"

#include <unistd.h>
#include <sys/time.h>

#include "libpq-fe.h"
#include "pqexpbuffer.h"
#include "access/xlog.h"
#include "catalog/pg_type.h"
#include "common/connect.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "replication/walreceiver.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/pg_lsn.h"
#include "utils/tuplestore.h"

PG_MODULE_MAGIC;

void
_PG_init(void);

struct WalReceiverConn
{
                                                 
  PGconn *streamConn;
                                                                 
  bool logical;
                                         
  char *recvBuf;
};

                                        
static WalReceiverConn *
libpqrcv_connect(const char *conninfo, bool logical, const char *appname, char **err);
static void
libpqrcv_check_conninfo(const char *conninfo);
static char *
libpqrcv_get_conninfo(WalReceiverConn *conn);
static void
libpqrcv_get_senderinfo(WalReceiverConn *conn, char **sender_host, int *sender_port);
static char *
libpqrcv_identify_system(WalReceiverConn *conn, TimeLineID *primary_tli);
static int
libpqrcv_server_version(WalReceiverConn *conn);
static void
libpqrcv_readtimelinehistoryfile(WalReceiverConn *conn, TimeLineID tli, char **filename, char **content, int *len);
static bool
libpqrcv_startstreaming(WalReceiverConn *conn, const WalRcvStreamOptions *options);
static void
libpqrcv_endstreaming(WalReceiverConn *conn, TimeLineID *next_tli);
static int
libpqrcv_receive(WalReceiverConn *conn, char **buffer, pgsocket *wait_fd);
static void
libpqrcv_send(WalReceiverConn *conn, const char *buffer, int nbytes);
static char *
libpqrcv_create_slot(WalReceiverConn *conn, const char *slotname, bool temporary, CRSSnapshotAction snapshot_action, XLogRecPtr *lsn);
static WalRcvExecResult *
libpqrcv_exec(WalReceiverConn *conn, const char *query, const int nRetTypes, const Oid *retTypes);
static void
libpqrcv_disconnect(WalReceiverConn *conn);

static WalReceiverFunctionsType PQWalReceiverFunctions = {libpqrcv_connect, libpqrcv_check_conninfo, libpqrcv_get_conninfo, libpqrcv_get_senderinfo, libpqrcv_identify_system, libpqrcv_server_version, libpqrcv_readtimelinehistoryfile, libpqrcv_startstreaming, libpqrcv_endstreaming, libpqrcv_receive, libpqrcv_send, libpqrcv_create_slot, libpqrcv_exec, libpqrcv_disconnect};

                                      
static PGresult *
libpqrcv_PQexec(PGconn *streamConn, const char *query);
static PGresult *
libpqrcv_PQgetResult(PGconn *streamConn);
static char *
stringlist_to_identifierstr(PGconn *conn, List *strings);

   
                                  
   
void
_PG_init(void)
{
  if (WalReceiverFunctions != NULL)
  {
    elog(ERROR, "libpqwalreceiver already loaded");
  }
  WalReceiverFunctions = &PQWalReceiverFunctions;
}

   
                                                                     
   
                                                                         
   
static WalReceiverConn *
libpqrcv_connect(const char *conninfo, bool logical, const char *appname, char **err)
{
  WalReceiverConn *conn;
  PostgresPollingStatusType status;
  const char *keys[5];
  const char *vals[5];
  int i = 0;

     
                                                                             
                                        
     
  keys[i] = "dbname";
  vals[i] = conninfo;
  keys[++i] = "replication";
  vals[i] = logical ? "database" : "true";
  if (!logical)
  {
       
                                                                           
                                                 
       
    keys[++i] = "dbname";
    vals[i] = "replication";
  }
  keys[++i] = "fallback_application_name";
  vals[i] = appname;
  if (logical)
  {
    keys[++i] = "client_encoding";
    vals[i] = GetDatabaseEncodingName();
  }
  keys[++i] = NULL;
  vals[i] = NULL;

  Assert(i < sizeof(keys));

  conn = palloc0(sizeof(WalReceiverConn));
  conn->streamConn = PQconnectStartParams(keys, vals,
                            true);
  if (PQstatus(conn->streamConn) == CONNECTION_BAD)
  {
    goto bad_connection_errmsg;
  }

     
                                                        
     
                                                                        
     
  status = PGRES_POLLING_WRITING;
  do
  {
    int io_flag;
    int rc;

    if (status == PGRES_POLLING_READING)
    {
      io_flag = WL_SOCKET_READABLE;
    }
#ifdef WIN32
                                                                          
    else if (PQstatus(conn->streamConn) == CONNECTION_STARTED)
    {
      io_flag = WL_SOCKET_CONNECTED;
    }
#endif
    else
    {
      io_flag = WL_SOCKET_WRITEABLE;
    }

    rc = WaitLatchOrSocket(MyLatch, WL_EXIT_ON_PM_DEATH | WL_LATCH_SET | io_flag, PQsocket(conn->streamConn), 0, WAIT_EVENT_LIBPQWALRECEIVER_CONNECT);

                      
    if (rc & WL_LATCH_SET)
    {
      ResetLatch(MyLatch);
      ProcessWalRcvInterrupts();
    }

                                                             
    if (rc & io_flag)
    {
      status = PQconnectPoll(conn->streamConn);
    }
  } while (status != PGRES_POLLING_OK && status != PGRES_POLLING_FAILED);

  if (PQstatus(conn->streamConn) != CONNECTION_OK)
  {
    goto bad_connection_errmsg;
  }

  if (logical)
  {
    PGresult *res;

    res = libpqrcv_PQexec(conn->streamConn, ALWAYS_SECURE_SEARCH_PATH_SQL);
    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
      PQclear(res);
      *err = psprintf(_("could not clear search path: %s"), pchomp(PQerrorMessage(conn->streamConn)));
      goto bad_connection;
    }
    PQclear(res);
  }

  conn->logical = logical;

  return conn;

                                               
bad_connection_errmsg:
  *err = pchomp(PQerrorMessage(conn->streamConn));

                                     
bad_connection:
  PQfinish(conn->streamConn);
  pfree(conn);
  return NULL;
}

   
                                                          
   
static void
libpqrcv_check_conninfo(const char *conninfo)
{
  PQconninfoOption *opts = NULL;
  char *err = NULL;

  opts = PQconninfoParse(conninfo, &err);
  if (opts == NULL)
  {
                                                                     
    char *errcopy = err ? pstrdup(err) : "out of memory";

    PQfreemem(err);
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("invalid connection string syntax: %s", errcopy)));
  }

  PQconninfoFree(opts);
}

   
                                                                             
                   
   
static char *
libpqrcv_get_conninfo(WalReceiverConn *conn)
{
  PQconninfoOption *conn_opts;
  PQconninfoOption *conn_opt;
  PQExpBufferData buf;
  char *retval;

  Assert(conn->streamConn != NULL);

  initPQExpBuffer(&buf);
  conn_opts = PQconninfo(conn->streamConn);

  if (conn_opts == NULL)
  {
    ereport(ERROR, (errmsg("could not parse connection string: %s", _("out of memory"))));
  }

                                                   
  for (conn_opt = conn_opts; conn_opt->keyword != NULL; conn_opt++)
  {
    bool obfuscate;

                                      
    if (strchr(conn_opt->dispchar, 'D') || conn_opt->val == NULL || conn_opt->val[0] == '\0')
    {
      continue;
    }

                                              
    obfuscate = strchr(conn_opt->dispchar, '*') != NULL;

    appendPQExpBuffer(&buf, "%s%s=%s", buf.len == 0 ? "" : " ", conn_opt->keyword, obfuscate ? "********" : conn_opt->val);
  }

  PQconninfoFree(conn_opts);

  retval = PQExpBufferDataBroken(buf) ? NULL : pstrdup(buf.data);
  termPQExpBuffer(&buf);
  return retval;
}

   
                                                                     
   
static void
libpqrcv_get_senderinfo(WalReceiverConn *conn, char **sender_host, int *sender_port)
{
  char *ret = NULL;

  *sender_host = NULL;
  *sender_port = 0;

  Assert(conn->streamConn != NULL);

  ret = PQhost(conn->streamConn);
  if (ret && strlen(ret) != 0)
  {
    *sender_host = pstrdup(ret);
  }

  ret = PQport(conn->streamConn);
  if (ret && strlen(ret) != 0)
  {
    *sender_port = atoi(ret);
  }
}

   
                                                                              
                               
   
static char *
libpqrcv_identify_system(WalReceiverConn *conn, TimeLineID *primary_tli)
{
  PGresult *res;
  char *primary_sysid;

     
                                                                             
                     
     
  res = libpqrcv_PQexec(conn->streamConn, "IDENTIFY_SYSTEM");
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    PQclear(res);
    ereport(ERROR, (errmsg("could not receive database system identifier and timeline ID from "
                           "the primary server: %s",
                       pchomp(PQerrorMessage(conn->streamConn)))));
  }
  if (PQnfields(res) < 3 || PQntuples(res) != 1)
  {
    int ntuples = PQntuples(res);
    int nfields = PQnfields(res);

    PQclear(res);
    ereport(ERROR, (errmsg("invalid response from primary server"), errdetail("Could not identify system: got %d rows and %d fields, expected %d rows and %d or more fields.", ntuples, nfields, 3, 1)));
  }
  primary_sysid = pstrdup(PQgetvalue(res, 0, 0));
  *primary_tli = pg_strtoint32(PQgetvalue(res, 0, 1));
  PQclear(res);

  return primary_sysid;
}

   
                                                       
   
static int
libpqrcv_server_version(WalReceiverConn *conn)
{
  return PQserverVersion(conn->streamConn);
}

   
                                                          
   
                                                                     
                                                                           
                                                                        
                                                                         
                                                                           
                    
   
static bool
libpqrcv_startstreaming(WalReceiverConn *conn, const WalRcvStreamOptions *options)
{
  StringInfoData cmd;
  PGresult *res;

  Assert(options->logical == conn->logical);
  Assert(options->slotname || !options->logical);

  initStringInfo(&cmd);

                          
  appendStringInfoString(&cmd, "START_REPLICATION");
  if (options->slotname != NULL)
  {
    appendStringInfo(&cmd, " SLOT \"%s\"", options->slotname);
  }

  if (options->logical)
  {
    appendStringInfoString(&cmd, " LOGICAL");
  }

  appendStringInfo(&cmd, " %X/%X", (uint32)(options->startpoint >> 32), (uint32)options->startpoint);

     
                                                                           
                              
     
  if (options->logical)
  {
    char *pubnames_str;
    List *pubnames;
    char *pubnames_literal;

    appendStringInfoString(&cmd, " (");

    appendStringInfo(&cmd, "proto_version '%u'", options->proto.logical.proto_version);

    pubnames = options->proto.logical.publication_names;
    pubnames_str = stringlist_to_identifierstr(conn->streamConn, pubnames);
    if (!pubnames_str)
    {
      ereport(ERROR, (errmsg("could not start WAL streaming: %s", pchomp(PQerrorMessage(conn->streamConn)))));
    }
    pubnames_literal = PQescapeLiteral(conn->streamConn, pubnames_str, strlen(pubnames_str));
    if (!pubnames_literal)
    {
      ereport(ERROR, (errmsg("could not start WAL streaming: %s", pchomp(PQerrorMessage(conn->streamConn)))));
    }
    appendStringInfo(&cmd, ", publication_names %s", pubnames_literal);
    PQfreemem(pubnames_literal);
    pfree(pubnames_str);

    appendStringInfoChar(&cmd, ')');
  }
  else
  {
    appendStringInfo(&cmd, " TIMELINE %u", options->proto.physical.startpointTLI);
  }

                        
  res = libpqrcv_PQexec(conn->streamConn, cmd.data);
  pfree(cmd.data);

  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return false;
  }
  else if (PQresultStatus(res) != PGRES_COPY_BOTH)
  {
    PQclear(res);
    ereport(ERROR, (errmsg("could not start WAL streaming: %s", pchomp(PQerrorMessage(conn->streamConn)))));
  }
  PQclear(res);
  return true;
}

   
                                                                            
                                                         
   
static void
libpqrcv_endstreaming(WalReceiverConn *conn, TimeLineID *next_tli)
{
  PGresult *res;

     
                                                                             
                                      
     
  if (PQputCopyEnd(conn->streamConn, NULL) <= 0 || PQflush(conn->streamConn))
  {
    ereport(ERROR, (errmsg("could not send end-of-streaming message to primary: %s", pchomp(PQerrorMessage(conn->streamConn)))));
  }

  *next_tli = 0;

     
                                                                           
                                                                        
           
     
                                                                             
                                                              
     
  res = libpqrcv_PQgetResult(conn->streamConn);
  if (PQresultStatus(res) == PGRES_TUPLES_OK)
  {
       
                                                                         
                                          
       
    if (PQnfields(res) < 2 || PQntuples(res) != 1)
    {
      ereport(ERROR, (errmsg("unexpected result set after end-of-streaming")));
    }
    *next_tli = pg_strtoint32(PQgetvalue(res, 0, 0));
    PQclear(res);

                                                              
    res = libpqrcv_PQgetResult(conn->streamConn);
  }
  else if (PQresultStatus(res) == PGRES_COPY_OUT)
  {
    PQclear(res);

                      
    if (PQendcopy(conn->streamConn))
    {
      ereport(ERROR, (errmsg("error while shutting down streaming COPY: %s", pchomp(PQerrorMessage(conn->streamConn)))));
    }

                                       
    res = libpqrcv_PQgetResult(conn->streamConn);
  }

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    ereport(ERROR, (errmsg("error reading result of streaming command: %s", pchomp(PQerrorMessage(conn->streamConn)))));
  }
  PQclear(res);

                                             
  res = libpqrcv_PQgetResult(conn->streamConn);
  if (res != NULL)
  {
    ereport(ERROR, (errmsg("unexpected result after CommandComplete: %s", pchomp(PQerrorMessage(conn->streamConn)))));
  }
}

   
                                                           
   
static void
libpqrcv_readtimelinehistoryfile(WalReceiverConn *conn, TimeLineID tli, char **filename, char **content, int *len)
{
  PGresult *res;
  char cmd[64];

  Assert(!conn->logical);

     
                                                                           
     
  snprintf(cmd, sizeof(cmd), "TIMELINE_HISTORY %u", tli);
  res = libpqrcv_PQexec(conn->streamConn, cmd);
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    PQclear(res);
    ereport(ERROR, (errmsg("could not receive timeline history file from "
                           "the primary server: %s",
                       pchomp(PQerrorMessage(conn->streamConn)))));
  }
  if (PQnfields(res) != 2 || PQntuples(res) != 1)
  {
    int ntuples = PQntuples(res);
    int nfields = PQnfields(res);

    PQclear(res);
    ereport(ERROR, (errmsg("invalid response from primary server"), errdetail("Expected 1 tuple with 2 fields, got %d tuples with %d fields.", ntuples, nfields)));
  }
  *filename = pstrdup(PQgetvalue(res, 0, 0));

  *len = PQgetlength(res, 0, 1);
  *content = palloc(*len);
  memcpy(*content, PQgetvalue(res, 0, 1), *len);
  PQclear(res);
}

   
                                                                         
                                          
   
                                                                      
                                                                        
            
   
                                                                     
                                                       
   
                                                             
   
static PGresult *
libpqrcv_PQexec(PGconn *streamConn, const char *query)
{
  PGresult *lastResult = NULL;

     
                                                                           
                                                                             
                                                                             
                                                     
     

     
                                                                         
                                                                            
                                         
     
  if (!PQsendQuery(streamConn, query))
  {
    return NULL;
  }

  for (;;)
  {
                                                   
    PGresult *result;

    result = libpqrcv_PQgetResult(streamConn);
    if (result == NULL)
    {
      break;                                    
    }

       
                                                                           
                                                                      
       
    PQclear(lastResult);
    lastResult = result;

    if (PQresultStatus(lastResult) == PGRES_COPY_IN || PQresultStatus(lastResult) == PGRES_COPY_OUT || PQresultStatus(lastResult) == PGRES_COPY_BOTH || PQstatus(streamConn) == CONNECTION_BAD)
    {
      break;
    }
  }

  return lastResult;
}

   
                                                                      
   
static PGresult *
libpqrcv_PQgetResult(PGconn *streamConn)
{
     
                                                                       
               
     
  while (PQisBusy(streamConn))
  {
    int rc;

       
                                                                      
                                                                 
                        
       
    rc = WaitLatchOrSocket(MyLatch, WL_EXIT_ON_PM_DEATH | WL_SOCKET_READABLE | WL_LATCH_SET, PQsocket(streamConn), 0, WAIT_EVENT_LIBPQWALRECEIVER_RECEIVE);

                      
    if (rc & WL_LATCH_SET)
    {
      ResetLatch(MyLatch);
      ProcessWalRcvInterrupts();
    }

                                                            
    if (PQconsumeInput(streamConn) == 0)
    {
                                
      return NULL;
    }
  }

                                                       
  return PQgetResult(streamConn);
}

   
                                             
   
static void
libpqrcv_disconnect(WalReceiverConn *conn)
{
  PQfinish(conn->streamConn);
  if (conn->recvBuf != NULL)
  {
    PQfreemem(conn->recvBuf);
  }
  pfree(conn);
}

   
                                                 
   
            
   
                                                                            
                                                                             
                                    
   
                                                                              
                                                                  
   
                                     
   
                      
   
static int
libpqrcv_receive(WalReceiverConn *conn, char **buffer, pgsocket *wait_fd)
{
  int rawlen;

  if (conn->recvBuf != NULL)
  {
    PQfreemem(conn->recvBuf);
  }
  conn->recvBuf = NULL;

                                         
  rawlen = PQgetCopyData(conn->streamConn, &conn->recvBuf, 1);
  if (rawlen == 0)
  {
                                  
    if (PQconsumeInput(conn->streamConn) == 0)
    {
      ereport(ERROR, (errmsg("could not receive data from WAL stream: %s", pchomp(PQerrorMessage(conn->streamConn)))));
    }

                                                       
    rawlen = PQgetCopyData(conn->streamConn, &conn->recvBuf, 1);
    if (rawlen == 0)
    {
                                                              
      *wait_fd = PQsocket(conn->streamConn);
      return 0;
    }
  }
  if (rawlen == -1)                                
  {
    PGresult *res;

    res = libpqrcv_PQgetResult(conn->streamConn);
    if (PQresultStatus(res) == PGRES_COMMAND_OK)
    {
      PQclear(res);

                                                  
      res = libpqrcv_PQgetResult(conn->streamConn);
      if (res != NULL)
      {
        PQclear(res);

           
                                                                      
                                                                       
                                               
           
        if (PQstatus(conn->streamConn) == CONNECTION_BAD)
        {
          return -1;
        }

        ereport(ERROR, (errmsg("unexpected result after CommandComplete: %s", PQerrorMessage(conn->streamConn))));
      }

      return -1;
    }
    else if (PQresultStatus(res) == PGRES_COPY_IN)
    {
      PQclear(res);
      return -1;
    }
    else
    {
      PQclear(res);
      ereport(ERROR, (errmsg("could not receive data from WAL stream: %s", pchomp(PQerrorMessage(conn->streamConn)))));
    }
  }
  if (rawlen < -1)
  {
    ereport(ERROR, (errmsg("could not receive data from WAL stream: %s", pchomp(PQerrorMessage(conn->streamConn)))));
  }

                                          
  *buffer = conn->recvBuf;
  return rawlen;
}

   
                                  
   
                      
   
static void
libpqrcv_send(WalReceiverConn *conn, const char *buffer, int nbytes)
{
  if (PQputCopyData(conn->streamConn, buffer, nbytes) <= 0 || PQflush(conn->streamConn))
  {
    ereport(ERROR, (errmsg("could not send data to WAL stream: %s", pchomp(PQerrorMessage(conn->streamConn)))));
  }
}

   
                                
                                                                          
                  
   
static char *
libpqrcv_create_slot(WalReceiverConn *conn, const char *slotname, bool temporary, CRSSnapshotAction snapshot_action, XLogRecPtr *lsn)
{
  PGresult *res;
  StringInfoData cmd;
  char *snapshot;

  initStringInfo(&cmd);

  appendStringInfo(&cmd, "CREATE_REPLICATION_SLOT \"%s\"", slotname);

  if (temporary)
  {
    appendStringInfoString(&cmd, " TEMPORARY");
  }

  if (conn->logical)
  {
    appendStringInfoString(&cmd, " LOGICAL pgoutput");
    switch (snapshot_action)
    {
    case CRS_EXPORT_SNAPSHOT:
      appendStringInfoString(&cmd, " EXPORT_SNAPSHOT");
      break;
    case CRS_NOEXPORT_SNAPSHOT:
      appendStringInfoString(&cmd, " NOEXPORT_SNAPSHOT");
      break;
    case CRS_USE_SNAPSHOT:
      appendStringInfoString(&cmd, " USE_SNAPSHOT");
      break;
    }
  }

  res = libpqrcv_PQexec(conn->streamConn, cmd.data);
  pfree(cmd.data);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    PQclear(res);
    ereport(ERROR, (errmsg("could not create replication slot \"%s\": %s", slotname, pchomp(PQerrorMessage(conn->streamConn)))));
  }

  *lsn = DatumGetLSN(DirectFunctionCall1Coll(pg_lsn_in, InvalidOid, CStringGetDatum(PQgetvalue(res, 0, 1))));
  if (!PQgetisnull(res, 0, 2))
  {
    snapshot = pstrdup(PQgetvalue(res, 0, 2));
  }
  else
  {
    snapshot = NULL;
  }

  PQclear(res);

  return snapshot;
}

   
                                             
   
static void
libpqrcv_processTuples(PGresult *pgres, WalRcvExecResult *walres, const int nRetTypes, const Oid *retTypes)
{
  int tupn;
  int coln;
  int nfields = PQnfields(pgres);
  HeapTuple tuple;
  AttInMetadata *attinmeta;
  MemoryContext rowcontext;
  MemoryContext oldcontext;

                                                   
  if (nfields != nRetTypes)
  {
    ereport(ERROR, (errmsg("invalid query response"), errdetail("Expected %d fields, got %d fields.", nRetTypes, nfields)));
  }

  walres->tuplestore = tuplestore_begin_heap(true, false, work_mem);

                                                                 
  walres->tupledesc = CreateTemplateTupleDesc(nRetTypes);
  for (coln = 0; coln < nRetTypes; coln++)
  {
    TupleDescInitEntry(walres->tupledesc, (AttrNumber)coln + 1, PQfname(pgres, coln), retTypes[coln], -1, 0);
  }
  attinmeta = TupleDescGetAttInMetadata(walres->tupledesc);

                                                                     
  if (PQntuples(pgres) == 0)
  {
    return;
  }

                                                       
  rowcontext = AllocSetContextCreate(CurrentMemoryContext, "libpqrcv query result context", ALLOCSET_DEFAULT_SIZES);

                              
  for (tupn = 0; tupn < PQntuples(pgres); tupn++)
  {
    char *cstrs[MaxTupleAttributeNumber];

    ProcessWalRcvInterrupts();

                                                  
    oldcontext = MemoryContextSwitchTo(rowcontext);

       
                                                                 
       
    for (coln = 0; coln < nfields; coln++)
    {
      if (PQgetisnull(pgres, tupn, coln))
      {
        cstrs[coln] = NULL;
      }
      else
      {
        cstrs[coln] = PQgetvalue(pgres, tupn, coln);
      }
    }

                                                              
    tuple = BuildTupleFromCStrings(attinmeta, cstrs);
    tuplestore_puttuple(walres->tuplestore, tuple);

                  
    MemoryContextSwitchTo(oldcontext);
    MemoryContextReset(rowcontext);
  }

  MemoryContextDelete(rowcontext);
}

   
                                                                
   
                                                               
   
static WalRcvExecResult *
libpqrcv_exec(WalReceiverConn *conn, const char *query, const int nRetTypes, const Oid *retTypes)
{
  PGresult *pgres = NULL;
  WalRcvExecResult *walres = palloc0(sizeof(WalRcvExecResult));

  if (MyDatabaseId == InvalidOid)
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("the query interface requires a database connection")));
  }

  pgres = libpqrcv_PQexec(conn->streamConn, query);

  switch (PQresultStatus(pgres))
  {
  case PGRES_SINGLE_TUPLE:
  case PGRES_TUPLES_OK:
    walres->status = WALRCV_OK_TUPLES;
    libpqrcv_processTuples(pgres, walres, nRetTypes, retTypes);
    break;

  case PGRES_COPY_IN:
    walres->status = WALRCV_OK_COPY_IN;
    break;

  case PGRES_COPY_OUT:
    walres->status = WALRCV_OK_COPY_OUT;
    break;

  case PGRES_COPY_BOTH:
    walres->status = WALRCV_OK_COPY_BOTH;
    break;

  case PGRES_COMMAND_OK:
    walres->status = WALRCV_OK_COMMAND;
    break;

                                          
  case PGRES_EMPTY_QUERY:
    walres->status = WALRCV_ERROR;
    walres->err = _("empty query");
    break;

  case PGRES_NONFATAL_ERROR:
  case PGRES_FATAL_ERROR:
  case PGRES_BAD_RESPONSE:
    walres->status = WALRCV_ERROR;
    walres->err = pchomp(PQerrorMessage(conn->streamConn));
    break;
  }

  PQclear(pgres);

  return walres;
}

   
                                                                
                                          
   
                                                             
   
                                      
   
static char *
stringlist_to_identifierstr(PGconn *conn, List *strings)
{
  ListCell *lc;
  StringInfoData res;
  bool first = true;

  initStringInfo(&res);

  foreach (lc, strings)
  {
    char *val = strVal(lfirst(lc));
    char *val_escaped;

    if (first)
    {
      first = false;
    }
    else
    {
      appendStringInfoChar(&res, ',');
    }

    val_escaped = PQescapeIdentifier(conn, val, strlen(val));
    if (!val_escaped)
    {
      free(res.data);
      return NULL;
    }
    appendStringInfoString(&res, val_escaped);
    PQfreemem(val_escaped);
  }

  return res.data;
}
