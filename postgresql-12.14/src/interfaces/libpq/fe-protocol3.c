                                                                            
   
                  
                                                                        
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres_fe.h"

#include <ctype.h>
#include <fcntl.h>

#include "libpq-fe.h"
#include "libpq-int.h"

#include "mb/pg_wchar.h"
#include "port/pg_bswap.h"

#ifdef WIN32
#include "win32.h"
#else
#include <unistd.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#endif

   
                                                                         
                                
   
#define VALID_LONG_MESSAGE_TYPE(id) ((id) == 'T' || (id) == 'D' || (id) == 'd' || (id) == 'V' || (id) == 'E' || (id) == 'N' || (id) == 'A')

#define PQmblenBounded(s, e) strnlen(s, PQmblen(s, e))

static void
handleSyncLoss(PGconn *conn, char id, int msgLength);
static int
getRowDescriptions(PGconn *conn, int msgLength);
static int
getParamDescriptions(PGconn *conn, int msgLength);
static int
getAnotherTuple(PGconn *conn, int msgLength);
static int
getParameterStatus(PGconn *conn);
static int
getNotify(PGconn *conn);
static int
getCopyStart(PGconn *conn, ExecStatusType copytype);
static int
getReadyForQuery(PGconn *conn);
static void
reportErrorPosition(PQExpBuffer msg, const char *query, int loc, int encoding);
static int
build_startup_packet(const PGconn *conn, char *packet, const PQEnvironmentOption *options);

   
                                                             
                                                            
                                                                                
   
void
pqParseInput3(PGconn *conn)
{
  char id;
  int msgLength;
  int avail;

     
                                                                         
     
  for (;;)
  {
       
                                                                          
                           
       
    conn->inCursor = conn->inStart;
    if (pqGetc(&id, conn))
    {
      return;
    }
    if (pqGetInt(&msgLength, 4, conn))
    {
      return;
    }

       
                                                                          
                                                                           
                      
       
    if (msgLength < 4)
    {
      handleSyncLoss(conn, id, msgLength);
      return;
    }
    if (msgLength > 30000 && !VALID_LONG_MESSAGE_TYPE(id))
    {
      handleSyncLoss(conn, id, msgLength);
      return;
    }

       
                                                         
       
    msgLength -= 4;
    avail = conn->inEnd - conn->inCursor;
    if (avail < msgLength)
    {
         
                                                                      
                                                               
                                                                      
                                                                        
                                                                   
                 
         
      if (pqCheckInBufferSpace(conn->inCursor + (size_t)msgLength, conn))
      {
           
                                                                     
                                                                       
                                                                      
                      
           
        handleSyncLoss(conn, id, msgLength);
      }
      return;
    }

       
                                                                          
                        
       
                                                                         
                                                                        
                                                       
       
                                                                          
                                            
       
                                                                           
                                                                           
                                                                        
                   
       
    if (id == 'A')
    {
      if (getNotify(conn))
      {
        return;
      }
    }
    else if (id == 'N')
    {
      if (pqGetErrorNotice3(conn, false))
      {
        return;
      }
    }
    else if (conn->asyncStatus != PGASYNC_BUSY)
    {
                                            
      if (conn->asyncStatus != PGASYNC_IDLE)
      {
        return;
      }

         
                                                                    
                                                                
                                                                    
                                                                  
                                                                        
                                                                       
                        
         
      if (id == 'E')
      {
        if (pqGetErrorNotice3(conn, false                      ))
        {
          return;
        }
      }
      else if (id == 'S')
      {
        if (getParameterStatus(conn))
        {
          return;
        }
      }
      else
      {
        pqInternalNotice(&conn->noticeHooks, "message type 0x%02x arrived from server while idle", id);
                                            
        conn->inCursor += msgLength;
      }
    }
    else
    {
         
                                                   
         
      switch (id)
      {
      case 'C':                       
        if (pqGets(&conn->workBuffer, conn))
        {
          return;
        }
        if (conn->result == NULL)
        {
          conn->result = PQmakeEmptyPGresult(conn, PGRES_COMMAND_OK);
          if (!conn->result)
          {
            printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory"));
            pqSaveErrorResult(conn);
          }
        }
        if (conn->result)
        {
          strlcpy(conn->result->cmdStatus, conn->workBuffer.data, CMDSTATUS_LEN);
        }
        conn->asyncStatus = PGASYNC_READY;
        break;
      case 'E':                   
        if (pqGetErrorNotice3(conn, true))
        {
          return;
        }
        conn->asyncStatus = PGASYNC_READY;
        break;
      case 'Z':                                     
        if (getReadyForQuery(conn))
        {
          return;
        }
        conn->asyncStatus = PGASYNC_IDLE;
        break;
      case 'I':                  
        if (conn->result == NULL)
        {
          conn->result = PQmakeEmptyPGresult(conn, PGRES_EMPTY_QUERY);
          if (!conn->result)
          {
            printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory"));
            pqSaveErrorResult(conn);
          }
        }
        conn->asyncStatus = PGASYNC_READY;
        break;
      case '1':                     
                                                               
        if (conn->queryclass == PGQUERY_PREPARE)
        {
          if (conn->result == NULL)
          {
            conn->result = PQmakeEmptyPGresult(conn, PGRES_COMMAND_OK);
            if (!conn->result)
            {
              printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory"));
              pqSaveErrorResult(conn);
            }
          }
          conn->asyncStatus = PGASYNC_READY;
        }
        break;
      case '2':                    
      case '3':                     
                                                   
        break;
      case 'S':                       
        if (getParameterStatus(conn))
        {
          return;
        }
        break;
      case 'K':                                       

           
                                                                  
                                                               
                                                  
           
        if (pqGetInt(&(conn->be_pid), 4, conn))
        {
          return;
        }
        if (pqGetInt(&(conn->be_key), 4, conn))
        {
          return;
        }
        break;
      case 'T':                      
        if (conn->result != NULL && conn->result->resultStatus == PGRES_FATAL_ERROR)
        {
             
                                                                 
                                                           
             
          conn->inCursor += msgLength;
        }
        else if (conn->result == NULL || conn->queryclass == PGQUERY_DESCRIBE)
        {
                                             
          if (getRowDescriptions(conn, msgLength))
          {
            return;
          }
        }
        else
        {
             
                                                          
                                                              
                                                                
                                                               
                     
             
          conn->asyncStatus = PGASYNC_READY;
          return;
        }
        break;
      case 'n':              

           
                                                         
                                                                  
                                               
           
                                                                
                                                              
                                                               
                         
           
        if (conn->queryclass == PGQUERY_DESCRIBE)
        {
          if (conn->result == NULL)
          {
            conn->result = PQmakeEmptyPGresult(conn, PGRES_COMMAND_OK);
            if (!conn->result)
            {
              printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory"));
              pqSaveErrorResult(conn);
            }
          }
          conn->asyncStatus = PGASYNC_READY;
        }
        break;
      case 't':                            
        if (getParamDescriptions(conn, msgLength))
        {
          return;
        }
        break;
      case 'D':               
        if (conn->result != NULL && conn->result->resultStatus == PGRES_TUPLES_OK)
        {
                                                             
          if (getAnotherTuple(conn, msgLength))
          {
            return;
          }
        }
        else if (conn->result != NULL && conn->result->resultStatus == PGRES_FATAL_ERROR)
        {
             
                                                                 
                                                         
             
          conn->inCursor += msgLength;
        }
        else
        {
                                                      
          printfPQExpBuffer(&conn->errorMessage, libpq_gettext("server sent data (\"D\" message) without prior row description (\"T\" message)\n"));
          pqSaveErrorResult(conn);
                                              
          conn->inCursor += msgLength;
        }
        break;
      case 'G':                    
        if (getCopyStart(conn, PGRES_COPY_IN))
        {
          return;
        }
        conn->asyncStatus = PGASYNC_COPY_IN;
        break;
      case 'H':                     
        if (getCopyStart(conn, PGRES_COPY_OUT))
        {
          return;
        }
        conn->asyncStatus = PGASYNC_COPY_OUT;
        conn->copy_already_done = 0;
        break;
      case 'W':                      
        if (getCopyStart(conn, PGRES_COPY_BOTH))
        {
          return;
        }
        conn->asyncStatus = PGASYNC_COPY_BOTH;
        conn->copy_already_done = 0;
        break;
      case 'd':                

           
                                                                   
                                                             
                  
           
        conn->inCursor += msgLength;
        break;
      case 'c':                

           
                                                                
                                                           
                                                                  
                             
           
        break;
      default:
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("unexpected response from server; first received character was \"%c\"\n"), id);
                                                             
        pqSaveErrorResult(conn);
                                                                
        conn->asyncStatus = PGASYNC_READY;
                                            
        conn->inCursor += msgLength;
        break;
      }                                   
    }
                                            
    if (conn->inCursor == conn->inStart + 5 + msgLength)
    {
                                                             
      conn->inStart = conn->inCursor;
    }
    else
    {
                                 
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("message contents do not agree with length in message type \"%c\"\n"), id);
                                                           
      pqSaveErrorResult(conn);
      conn->asyncStatus = PGASYNC_READY;
                                                              
      conn->inStart += 5 + msgLength;
    }
  }
}

   
                                                                
   
                                                                          
   
static void
handleSyncLoss(PGconn *conn, char id, int msgLength)
{
  printfPQExpBuffer(&conn->errorMessage, libpq_gettext("lost synchronization with server: got message type \"%c\", length %d\n"), id, msgLength);
                                                       
  pqSaveErrorResult(conn);
  conn->asyncStatus = PGASYNC_READY;                                      
                                                               
  pqDropConnection(conn, true);
  conn->status = CONNECTION_BAD;                                    
}

   
                                                                   
                                                                      
                                                                    
                                                                        
                                                     
   
static int
getRowDescriptions(PGconn *conn, int msgLength)
{
  PGresult *result;
  int nfields;
  const char *errmsg;
  int i;

     
                                                                         
                                                                            
                                                     
     
  if (conn->queryclass == PGQUERY_DESCRIBE)
  {
    if (conn->result)
    {
      result = conn->result;
    }
    else
    {
      result = PQmakeEmptyPGresult(conn, PGRES_COMMAND_OK);
    }
  }
  else
  {
    result = PQmakeEmptyPGresult(conn, PGRES_TUPLES_OK);
  }
  if (!result)
  {
    errmsg = NULL;                                       
    goto advance_and_error;
  }

                                                                 
                                                   
  if (pqGetInt(&(result->numAttributes), 2, conn))
  {
                                                         
    errmsg = libpq_gettext("insufficient data in \"T\" message");
    goto advance_and_error;
  }
  nfields = result->numAttributes;

                                                    
  if (nfields > 0)
  {
    result->attDescs = (PGresAttDesc *)pqResultAlloc(result, nfields * sizeof(PGresAttDesc), true);
    if (!result->attDescs)
    {
      errmsg = NULL;                                       
      goto advance_and_error;
    }
    MemSet(result->attDescs, 0, nfields * sizeof(PGresAttDesc));
  }

                                                             
  result->binary = (nfields > 0) ? 1 : 0;

                     
  for (i = 0; i < nfields; i++)
  {
    int tableid;
    int columnid;
    int typid;
    int typlen;
    int atttypmod;
    int format;

    if (pqGets(&conn->workBuffer, conn) || pqGetInt(&tableid, 4, conn) || pqGetInt(&columnid, 2, conn) || pqGetInt(&typid, 4, conn) || pqGetInt(&typlen, 2, conn) || pqGetInt(&atttypmod, 4, conn) || pqGetInt(&format, 2, conn))
    {
                                                           
      errmsg = libpq_gettext("insufficient data in \"T\" message");
      goto advance_and_error;
    }

       
                                                                     
                                            
       
    columnid = (int)((int16)columnid);
    typlen = (int)((int16)typlen);
    format = (int)((int16)format);

    result->attDescs[i].name = pqResultStrdup(result, conn->workBuffer.data);
    if (!result->attDescs[i].name)
    {
      errmsg = NULL;                                       
      goto advance_and_error;
    }
    result->attDescs[i].tableid = tableid;
    result->attDescs[i].columnid = columnid;
    result->attDescs[i].format = format;
    result->attDescs[i].typid = typid;
    result->attDescs[i].typlen = typlen;
    result->attDescs[i].atttypmod = atttypmod;

    if (format != 1)
    {
      result->binary = 0;
    }
  }

                
  conn->result = result;

     
                                                                         
                         
     
  if (conn->queryclass == PGQUERY_DESCRIBE)
  {
    conn->asyncStatus = PGASYNC_READY;
    return 0;
  }

     
                                                                            
                                     
     

                       
  return 0;

advance_and_error:
                                      
  if (result && result != conn->result)
  {
    PQclear(result);
  }

     
                                                                      
                                                            
     
  pqClearAsyncResult(conn);

     
                                                                       
                                                                           
                                                                           
                                              
     
  if (!errmsg)
  {
    errmsg = libpq_gettext("out of memory for query result");
  }

  printfPQExpBuffer(&conn->errorMessage, "%s\n", errmsg);
  pqSaveErrorResult(conn);

     
                                                                           
                                            
     
  conn->inCursor = conn->inStart + 5 + msgLength;

     
                                                                     
                                                                          
                               
     
  return 0;
}

   
                                                                       
                                                                       
                                                                        
                                                     
   
static int
getParamDescriptions(PGconn *conn, int msgLength)
{
  PGresult *result;
  const char *errmsg = NULL;                                       
  int nparams;
  int i;

  result = PQmakeEmptyPGresult(conn, PGRES_COMMAND_OK);
  if (!result)
  {
    goto advance_and_error;
  }

                                                                 
                                                       
  if (pqGetInt(&(result->numParameters), 2, conn))
  {
    goto not_enough_data;
  }
  nparams = result->numParameters;

                                                    
  if (nparams > 0)
  {
    result->paramDescs = (PGresParamDesc *)pqResultAlloc(result, nparams * sizeof(PGresParamDesc), true);
    if (!result->paramDescs)
    {
      goto advance_and_error;
    }
    MemSet(result->paramDescs, 0, nparams * sizeof(PGresParamDesc));
  }

                          
  for (i = 0; i < nparams; i++)
  {
    int typid;

    if (pqGetInt(&typid, 4, conn))
    {
      goto not_enough_data;
    }
    result->paramDescs[i].typid = typid;
  }

                
  conn->result = result;

  return 0;

not_enough_data:
  errmsg = libpq_gettext("insufficient data in \"t\" message");

advance_and_error:
                                      
  if (result && result != conn->result)
  {
    PQclear(result);
  }

     
                                                                      
                                                            
     
  pqClearAsyncResult(conn);

     
                                                                       
                                                                           
                                                                           
                                              
     
  if (!errmsg)
  {
    errmsg = libpq_gettext("out of memory");
  }
  printfPQExpBuffer(&conn->errorMessage, "%s\n", errmsg);
  pqSaveErrorResult(conn);

     
                                                                           
                                            
     
  conn->inCursor = conn->inStart + 5 + msgLength;

     
                                                                         
                                                                         
                                                   
     
  return 0;
}

   
                                                           
                                                                        
                                                                        
                                                     
   
static int
getAnotherTuple(PGconn *conn, int msgLength)
{
  PGresult *result = conn->result;
  int nfields = result->numAttributes;
  const char *errmsg;
  PGdataValue *rowbuf;
  int tupnfields;                          
  int vlen;                                              
  int i;

                                                             
  if (pqGetInt(&tupnfields, 2, conn))
  {
                                                         
    errmsg = libpq_gettext("insufficient data in \"D\" message");
    goto advance_and_error;
  }

  if (tupnfields != nfields)
  {
    errmsg = libpq_gettext("unexpected field count in \"D\" message");
    goto advance_and_error;
  }

                                   
  rowbuf = conn->rowBuf;
  if (nfields > conn->rowBufLen)
  {
    rowbuf = (PGdataValue *)realloc(rowbuf, nfields * sizeof(PGdataValue));
    if (!rowbuf)
    {
      errmsg = NULL;                                       
      goto advance_and_error;
    }
    conn->rowBuf = rowbuf;
    conn->rowBufLen = nfields;
  }

                       
  for (i = 0; i < nfields; i++)
  {
                              
    if (pqGetInt(&vlen, 4, conn))
    {
                                                           
      errmsg = libpq_gettext("insufficient data in \"D\" message");
      goto advance_and_error;
    }
    rowbuf[i].len = vlen;

       
                                                                     
                                                                        
                                        
       
    rowbuf[i].value = conn->inBuffer + conn->inCursor;

                                  
    if (vlen > 0)
    {
      if (pqSkipnchar(vlen, conn))
      {
                                                             
        errmsg = libpq_gettext("insufficient data in \"D\" message");
        goto advance_and_error;
      }
    }
  }

                                 
  errmsg = NULL;
  if (pqRowProcessor(conn, &errmsg))
  {
    return 0;                              
  }

                                                        

advance_and_error:

     
                                                                      
                                                            
     
  pqClearAsyncResult(conn);

     
                                                                       
                                                                           
                                                                           
                                              
     
  if (!errmsg)
  {
    errmsg = libpq_gettext("out of memory for query result");
  }

  printfPQExpBuffer(&conn->errorMessage, "%s\n", errmsg);
  pqSaveErrorResult(conn);

     
                                                                           
                                            
     
  conn->inCursor = conn->inStart + 5 + msgLength;

     
                                                                     
                                                                          
                               
     
  return 0;
}

   
                                                        
                                                                           
                                                                         
                                                     
                                     
   
int
pqGetErrorNotice3(PGconn *conn, bool isError)
{
  PGresult *res = NULL;
  bool have_position = false;
  PQExpBufferData workBuf;
  char id;

     
                                                                           
                                                                    
                                                                         
     
  if (isError)
  {
    pqClearAsyncResult(conn);
  }

     
                                                                  
                                                                             
                                                               
                                                                   
     
  initPQExpBuffer(&workBuf);

     
                                                                         
                                                                            
                              
     
                                                                             
                                                                         
                                                
     
  res = PQmakeEmptyPGresult(conn, PGRES_EMPTY_QUERY);
  if (res)
  {
    res->resultStatus = isError ? PGRES_FATAL_ERROR : PGRES_NONFATAL_ERROR;
  }

     
                                        
     
                                                                             
                                                
     
  for (;;)
  {
    if (pqGetc(&id, conn))
    {
      goto fail;
    }
    if (id == '\0')
    {
      break;                       
    }
    if (pqGets(&workBuf, conn))
    {
      goto fail;
    }
    pqSaveMessageField(res, id, workBuf.data);
    if (id == PG_DIAG_SQLSTATE)
    {
      strlcpy(conn->last_sqlstate, workBuf.data, sizeof(conn->last_sqlstate));
    }
    else if (id == PG_DIAG_STATEMENT_POSITION)
    {
      have_position = true;
    }
  }

     
                                                                          
                                                                            
                                            
     
  if (have_position && conn->last_query && res)
  {
    res->errQuery = pqResultStrdup(res, conn->last_query);
  }

     
                                                                     
     
  resetPQExpBuffer(&workBuf);
  pqBuildErrorMessage3(&workBuf, res, conn->verbosity, conn->show_context);

     
                                                                         
     
  if (isError)
  {
    if (res)
    {
      res->errMsg = pqResultStrdup(res, workBuf.data);
    }
    pqClearAsyncResult(conn);                             
    conn->result = res;
    if (PQExpBufferDataBroken(workBuf))
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory"));
    }
    else
    {
      appendPQExpBufferStr(&conn->errorMessage, workBuf.data);
    }
  }
  else
  {
                                                                         
    if (res)
    {
                                                                
      res->errMsg = workBuf.data;
      if (res->noticeHooks.noticeRec != NULL)
      {
        res->noticeHooks.noticeRec(res->noticeHooks.noticeRecArg, res);
      }
      PQclear(res);
    }
  }

  termPQExpBuffer(&workBuf);
  return 0;

fail:
  PQclear(res);
  termPQExpBuffer(&workBuf);
  return EOF;
}

   
                                                                     
                                          
   
void
pqBuildErrorMessage3(PQExpBuffer msg, const PGresult *res, PGVerbosity verbosity, PGContextVisibility show_context)
{
  const char *val;
  const char *querytext = NULL;
  int querypos = 0;

                                                                    
  if (res == NULL)
  {
    appendPQExpBuffer(msg, libpq_gettext("out of memory\n"));
    return;
  }

     
                                                                            
                                                                        
     
  if (res->errFields == NULL)
  {
    if (res->errMsg && res->errMsg[0])
    {
      appendPQExpBufferStr(msg, res->errMsg);
    }
    else
    {
      appendPQExpBuffer(msg, libpq_gettext("no error message available\n"));
    }
    return;
  }

                                                     
  val = PQresultErrorField(res, PG_DIAG_SEVERITY);
  if (val)
  {
    appendPQExpBuffer(msg, "%s:  ", val);
  }

  if (verbosity == PQERRORS_SQLSTATE)
  {
       
                                                                          
                                                                        
                                                                       
                                                       
       
    val = PQresultErrorField(res, PG_DIAG_SQLSTATE);
    if (val)
    {
      appendPQExpBuffer(msg, "%s\n", val);
      return;
    }
    verbosity = PQERRORS_TERSE;
  }

  if (verbosity == PQERRORS_VERBOSE)
  {
    val = PQresultErrorField(res, PG_DIAG_SQLSTATE);
    if (val)
    {
      appendPQExpBuffer(msg, "%s: ", val);
    }
  }
  val = PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY);
  if (val)
  {
    appendPQExpBufferStr(msg, val);
  }
  val = PQresultErrorField(res, PG_DIAG_STATEMENT_POSITION);
  if (val)
  {
    if (verbosity != PQERRORS_TERSE && res->errQuery != NULL)
    {
                                                    
      querytext = res->errQuery;
      querypos = atoi(val);
    }
    else
    {
                                                             
                                                    
      appendPQExpBuffer(msg, libpq_gettext(" at character %s"), val);
    }
  }
  else
  {
    val = PQresultErrorField(res, PG_DIAG_INTERNAL_POSITION);
    if (val)
    {
      querytext = PQresultErrorField(res, PG_DIAG_INTERNAL_QUERY);
      if (verbosity != PQERRORS_TERSE && querytext != NULL)
      {
                                                      
        querypos = atoi(val);
      }
      else
      {
                                                               
                                                      
        appendPQExpBuffer(msg, libpq_gettext(" at character %s"), val);
      }
    }
  }
  appendPQExpBufferChar(msg, '\n');
  if (verbosity != PQERRORS_TERSE)
  {
    if (querytext && querypos > 0)
    {
      reportErrorPosition(msg, querytext, querypos, res->client_encoding);
    }
    val = PQresultErrorField(res, PG_DIAG_MESSAGE_DETAIL);
    if (val)
    {
      appendPQExpBuffer(msg, libpq_gettext("DETAIL:  %s\n"), val);
    }
    val = PQresultErrorField(res, PG_DIAG_MESSAGE_HINT);
    if (val)
    {
      appendPQExpBuffer(msg, libpq_gettext("HINT:  %s\n"), val);
    }
    val = PQresultErrorField(res, PG_DIAG_INTERNAL_QUERY);
    if (val)
    {
      appendPQExpBuffer(msg, libpq_gettext("QUERY:  %s\n"), val);
    }
    if (show_context == PQSHOW_CONTEXT_ALWAYS || (show_context == PQSHOW_CONTEXT_ERRORS && res->resultStatus == PGRES_FATAL_ERROR))
    {
      val = PQresultErrorField(res, PG_DIAG_CONTEXT);
      if (val)
      {
        appendPQExpBuffer(msg, libpq_gettext("CONTEXT:  %s\n"), val);
      }
    }
  }
  if (verbosity == PQERRORS_VERBOSE)
  {
    val = PQresultErrorField(res, PG_DIAG_SCHEMA_NAME);
    if (val)
    {
      appendPQExpBuffer(msg, libpq_gettext("SCHEMA NAME:  %s\n"), val);
    }
    val = PQresultErrorField(res, PG_DIAG_TABLE_NAME);
    if (val)
    {
      appendPQExpBuffer(msg, libpq_gettext("TABLE NAME:  %s\n"), val);
    }
    val = PQresultErrorField(res, PG_DIAG_COLUMN_NAME);
    if (val)
    {
      appendPQExpBuffer(msg, libpq_gettext("COLUMN NAME:  %s\n"), val);
    }
    val = PQresultErrorField(res, PG_DIAG_DATATYPE_NAME);
    if (val)
    {
      appendPQExpBuffer(msg, libpq_gettext("DATATYPE NAME:  %s\n"), val);
    }
    val = PQresultErrorField(res, PG_DIAG_CONSTRAINT_NAME);
    if (val)
    {
      appendPQExpBuffer(msg, libpq_gettext("CONSTRAINT NAME:  %s\n"), val);
    }
  }
  if (verbosity == PQERRORS_VERBOSE)
  {
    const char *valf;
    const char *vall;

    valf = PQresultErrorField(res, PG_DIAG_SOURCE_FILE);
    vall = PQresultErrorField(res, PG_DIAG_SOURCE_LINE);
    val = PQresultErrorField(res, PG_DIAG_SOURCE_FUNCTION);
    if (val || valf || vall)
    {
      appendPQExpBufferStr(msg, libpq_gettext("LOCATION:  "));
      if (val)
      {
        appendPQExpBuffer(msg, libpq_gettext("%s, "), val);
      }
      if (valf && vall)                                  
      {
        appendPQExpBuffer(msg, libpq_gettext("%s:%s"), valf, vall);
      }
      appendPQExpBufferChar(msg, '\n');
    }
  }
}

   
                                                                          
   
                                                                           
                                                
   
static void
reportErrorPosition(PQExpBuffer msg, const char *query, int loc, int encoding)
{
#define DISPLAY_SIZE 60                                          
#define MIN_RIGHT_CUT 10                                         

  char *wquery;
  int slen, cno, i, *qidx, *scridx, qoffset, scroffset, ibeg, iend, loc_line;
  bool mb_encoding, beg_trunc, end_trunc;

                                                                  
  loc--;
  if (loc < 0)
  {
    return;
  }

                                         
  wquery = strdup(query);
  if (wquery == NULL)
  {
    return;                                     
  }

     
                                                                            
                                                                           
                                                                       
                                                                            
                            
     

                                         
  slen = strlen(wquery) + 1;

  qidx = (int *)malloc(slen * sizeof(int));
  if (qidx == NULL)
  {
    free(wquery);
    return;
  }
  scridx = (int *)malloc(slen * sizeof(int));
  if (scridx == NULL)
  {
    free(qidx);
    free(wquery);
    return;
  }

                                                            
  mb_encoding = (pg_encoding_max_length(encoding) != 1);

     
                                                                      
                                                                            
                                                                       
                                                                            
                                                                             
                                                                          
                                                              
     
  qoffset = 0;
  scroffset = 0;
  loc_line = 1;
  ibeg = 0;
  iend = -1;                           

  for (cno = 0; wquery[qoffset] != '\0'; cno++)
  {
    char ch = wquery[qoffset];

    qidx[cno] = qoffset;
    scridx[cno] = scroffset;

       
                                                                       
                                                                        
                   
       
    if (ch == '\t')
    {
      wquery[qoffset] = ' ';
    }

       
                                                                     
                                                           
       
    else if (ch == '\r' || ch == '\n')
    {
      if (cno < loc)
      {
        if (ch == '\r' || cno == 0 || wquery[qidx[cno - 1]] != '\r')
        {
          loc_line++;
        }
                                                             
        ibeg = cno + 1;
      }
      else
      {
                              
        iend = cno;
                            
        break;
      }
    }

                 
    if (mb_encoding)
    {
      int w;

      w = pg_encoding_dsplen(encoding, &wquery[qoffset]);
                                                      
      if (w <= 0)
      {
        w = 1;
      }
      scroffset += w;
      qoffset += PQmblenBounded(&wquery[qoffset], encoding);
    }
    else
    {
                                                                  
      scroffset++;
      qoffset++;
    }
  }
                                                         
  if (iend < 0)
  {
    iend = cno;                                
    qidx[iend] = qoffset;
    scridx[iend] = scroffset;
  }

                                                         
  if (loc <= cno)
  {
                                                            
    beg_trunc = false;
    end_trunc = false;
    if (scridx[iend] - scridx[ibeg] > DISPLAY_SIZE)
    {
         
                                                                      
                                                                        
                                                         
         
      if (scridx[ibeg] + DISPLAY_SIZE >= scridx[loc] + MIN_RIGHT_CUT)
      {
        while (scridx[iend] - scridx[ibeg] > DISPLAY_SIZE)
        {
          iend--;
        }
        end_trunc = true;
      }
      else
      {
                                                     
        while (scridx[loc] + MIN_RIGHT_CUT < scridx[iend])
        {
          iend--;
          end_trunc = true;
        }

                                              
        while (scridx[iend] - scridx[ibeg] > DISPLAY_SIZE)
        {
          ibeg++;
          beg_trunc = true;
        }
      }
    }

                                                   
    wquery[qidx[iend]] = '\0';

                                              
    i = msg->len;
    appendPQExpBuffer(msg, libpq_gettext("LINE %d: "), loc_line);
    if (beg_trunc)
    {
      appendPQExpBufferStr(msg, "...");
    }

       
                                                                      
              
       
    scroffset = 0;
    for (; i < msg->len; i += PQmblenBounded(&msg->data[i], encoding))
    {
      int w = pg_encoding_dsplen(encoding, &msg->data[i]);

      if (w <= 0)
      {
        w = 1;
      }
      scroffset += w;
    }

                                          
    appendPQExpBufferStr(msg, &wquery[qidx[ibeg]]);
    if (end_trunc)
    {
      appendPQExpBufferStr(msg, "...");
    }
    appendPQExpBufferChar(msg, '\n');

                                          
    scroffset += scridx[loc] - scridx[ibeg];
    for (i = 0; i < scroffset; i++)
    {
      appendPQExpBufferChar(msg, ' ');
    }
    appendPQExpBufferChar(msg, '^');
    appendPQExpBufferChar(msg, '\n');
  }

                 
  free(scridx);
  free(qidx);
  free(wquery);
}

   
                                              
                                                                           
                                                                  
                                                     
                                     
   
static int
getParameterStatus(PGconn *conn)
{
  PQExpBufferData valueBuf;

                              
  if (pqGets(&conn->workBuffer, conn))
  {
    return EOF;
  }
                                                
  initPQExpBuffer(&valueBuf);
  if (pqGets(&valueBuf, conn))
  {
    termPQExpBuffer(&valueBuf);
    return EOF;
  }
                   
  pqSaveParameterStatus(conn, conn->workBuffer.data, valueBuf.data);
  termPQExpBuffer(&valueBuf);
  return 0;
}

   
                                              
                                                                           
                                                                  
                                                            
                                     
   
static int
getNotify(PGconn *conn)
{
  int be_pid;
  char *svname;
  int nmlen;
  int extralen;
  PGnotify *newNotify;

  if (pqGetInt(&be_pid, 4, conn))
  {
    return EOF;
  }
  if (pqGets(&conn->workBuffer, conn))
  {
    return EOF;
  }
                                                 
  svname = strdup(conn->workBuffer.data);
  if (!svname)
  {
    return EOF;
  }
  if (pqGets(&conn->workBuffer, conn))
  {
    free(svname);
    return EOF;
  }

     
                                                                           
                                                                           
                                                      
     
  nmlen = strlen(svname);
  extralen = strlen(conn->workBuffer.data);
  newNotify = (PGnotify *)malloc(sizeof(PGnotify) + nmlen + extralen + 2);
  if (newNotify)
  {
    newNotify->relname = (char *)newNotify + sizeof(PGnotify);
    strcpy(newNotify->relname, svname);
    newNotify->extra = newNotify->relname + nmlen + 1;
    strcpy(newNotify->extra, conn->workBuffer.data);
    newNotify->be_pid = be_pid;
    newNotify->next = NULL;
    if (conn->notifyTail)
    {
      conn->notifyTail->next = newNotify;
    }
    else
    {
      conn->notifyHead = newNotify;
    }
    conn->notifyTail = newNotify;
  }

  free(svname);
  return 0;
}

   
                                                             
                            
   
                                                        
   
static int
getCopyStart(PGconn *conn, ExecStatusType copytype)
{
  PGresult *result;
  int nfields;
  int i;

  result = PQmakeEmptyPGresult(conn, copytype);
  if (!result)
  {
    goto failure;
  }

  if (pqGetc(&conn->copy_is_binary, conn))
  {
    goto failure;
  }
  result->binary = conn->copy_is_binary;
                                                   
  if (pqGetInt(&(result->numAttributes), 2, conn))
  {
    goto failure;
  }
  nfields = result->numAttributes;

                                                    
  if (nfields > 0)
  {
    result->attDescs = (PGresAttDesc *)pqResultAlloc(result, nfields * sizeof(PGresAttDesc), true);
    if (!result->attDescs)
    {
      goto failure;
    }
    MemSet(result->attDescs, 0, nfields * sizeof(PGresAttDesc));
  }

  for (i = 0; i < nfields; i++)
  {
    int format;

    if (pqGetInt(&format, 2, conn))
    {
      goto failure;
    }

       
                                                                     
                                            
       
    format = (int)((int16)format);
    result->attDescs[i].format = format;
  }

                
  conn->result = result;
  return 0;

failure:
  PQclear(result);
  return EOF;
}

   
                                                    
   
static int
getReadyForQuery(PGconn *conn)
{
  char xact_status;

  if (pqGetc(&xact_status, conn))
  {
    return EOF;
  }
  switch (xact_status)
  {
  case 'I':
    conn->xactStatus = PQTRANS_IDLE;
    break;
  case 'T':
    conn->xactStatus = PQTRANS_INTRANS;
    break;
  case 'E':
    conn->xactStatus = PQTRANS_INERROR;
    break;
  default:
    conn->xactStatus = PQTRANS_UNKNOWN;
    break;
  }

  return 0;
}

   
                                                                            
   
                                                                      
                                                      
   
static int
getCopyDataMessage(PGconn *conn)
{
  char id;
  int msgLength;
  int avail;

  for (;;)
  {
       
                                                                          
                                                                    
                                               
       
    conn->inCursor = conn->inStart;
    if (pqGetc(&id, conn))
    {
      return 0;
    }
    if (pqGetInt(&msgLength, 4, conn))
    {
      return 0;
    }
    if (msgLength < 4)
    {
      handleSyncLoss(conn, id, msgLength);
      return -2;
    }
    avail = conn->inEnd - conn->inCursor;
    if (avail < msgLength - 4)
    {
         
                                                                      
                                                      
         
      if (pqCheckInBufferSpace(conn->inCursor + (size_t)msgLength - 4, conn))
      {
           
                                                                     
                                                                       
                                                                      
                      
           
        handleSyncLoss(conn, id, msgLength);
        return -2;
      }
      return 0;
    }

       
                                                                     
                                                                        
                                                                     
                           
       
    switch (id)
    {
    case 'A':             
      if (getNotify(conn))
      {
        return 0;
      }
      break;
    case 'N':             
      if (pqGetErrorNotice3(conn, false))
      {
        return 0;
      }
      break;
    case 'S':                      
      if (getParameterStatus(conn))
      {
        return 0;
      }
      break;
    case 'd':                                        
      return msgLength;
    case 'c':

         
                                                                   
                                                             
                                                 
         
      if (conn->asyncStatus == PGASYNC_COPY_BOTH)
      {
        conn->asyncStatus = PGASYNC_COPY_IN;
      }
      else
      {
        conn->asyncStatus = PGASYNC_BUSY;
      }
      return -1;
    default:                           

         
                                                                  
               
         
      conn->asyncStatus = PGASYNC_BUSY;
      return -1;
    }

                                                                
    conn->inStart = conn->inCursor;
  }
}

   
                                                                       
                
   
                                                                       
                                              
                                                                       
                                                                    
                    
   
int
pqGetCopyData3(PGconn *conn, char **buffer, int async)
{
  int msgLength;

  for (;;)
  {
       
                                                                       
                                                                    
                                               
       
    msgLength = getCopyDataMessage(conn);
    if (msgLength < 0)
    {
      return msgLength;                           
    }
    if (msgLength == 0)
    {
                                               
      if (async)
      {
        return 0;
      }
                                  
      if (pqWait(true, false, conn) || pqReadData(conn) < 0)
      {
        return -2;
      }
      continue;
    }

       
                                                                       
                                         
       
    msgLength -= 4;
    if (msgLength > 0)
    {
      *buffer = (char *)malloc(msgLength + 1);
      if (*buffer == NULL)
      {
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
        return -2;
      }
      memcpy(*buffer, &conn->inBuffer[conn->inCursor], msgLength);
      (*buffer)[msgLength] = '\0';                           

                                 
      conn->inStart = conn->inCursor + msgLength;

      return msgLength;
    }

                                                       
    conn->inStart = conn->inCursor;
  }
}

   
                                                                  
   
                                    
   
int
pqGetline3(PGconn *conn, char *s, int maxlen)
{
  int status;

  if (conn->sock == PGINVALID_SOCKET || (conn->asyncStatus != PGASYNC_COPY_OUT && conn->asyncStatus != PGASYNC_COPY_BOTH) || conn->copy_is_binary)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("PQgetline: not doing text COPY OUT\n"));
    *s = '\0';
    return EOF;
  }

  while ((status = PQgetlineAsync(conn, s, maxlen - 1)) == 0)
  {
                                
    if (pqWait(true, false, conn) || pqReadData(conn) < 0)
    {
      *s = '\0';
      return EOF;
    }
  }

  if (status < 0)
  {
                                                           
    strcpy(s, "\\.");
    return 0;
  }

                                                             
  if (s[status - 1] == '\n')
  {
    s[status - 1] = '\0';
    return 0;
  }
  else
  {
    s[status] = '\0';
    return 1;
  }
}

   
                                                           
   
                                    
   
int
pqGetlineAsync3(PGconn *conn, char *buffer, int bufsize)
{
  int msgLength;
  int avail;

  if (conn->asyncStatus != PGASYNC_COPY_OUT && conn->asyncStatus != PGASYNC_COPY_BOTH)
  {
    return -1;                                 
  }

     
                                                                       
                                                                            
                                                                             
                                                                       
     
  msgLength = getCopyDataMessage(conn);
  if (msgLength < 0)
  {
    return -1;                           
  }
  if (msgLength == 0)
  {
    return 0;                  
  }

     
                                                                         
                                                            
                                                                         
                             
     
  conn->inCursor += conn->copy_already_done;
  avail = msgLength - 4 - conn->copy_already_done;
  if (avail <= bufsize)
  {
                                           
    memcpy(buffer, &conn->inBuffer[conn->inCursor], avail);
                               
    conn->inStart = conn->inCursor + avail;
                                   
    conn->copy_already_done = 0;
    return avail;
  }
  else
  {
                                          
    memcpy(buffer, &conn->inBuffer[conn->inCursor], bufsize);
                                                         
    conn->copy_already_done += bufsize;
    return bufsize;
  }
}

   
             
   
                                    
   
int
pqEndcopy3(PGconn *conn)
{
  PGresult *result;

  if (conn->asyncStatus != PGASYNC_COPY_IN && conn->asyncStatus != PGASYNC_COPY_OUT && conn->asyncStatus != PGASYNC_COPY_BOTH)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("no COPY in progress\n"));
    return 1;
  }

                                           
  if (conn->asyncStatus == PGASYNC_COPY_IN || conn->asyncStatus == PGASYNC_COPY_BOTH)
  {
    if (pqPutMsgStart('c', false, conn) < 0 || pqPutMsgEnd(conn) < 0)
    {
      return 1;
    }

       
                                                                           
                     
       
    if (conn->queryclass != PGQUERY_SIMPLE)
    {
      if (pqPutMsgStart('S', false, conn) < 0 || pqPutMsgEnd(conn) < 0)
      {
        return 1;
      }
    }
  }

     
                                                                           
                         
     
  if (pqFlush(conn) && pqIsnonblocking(conn))
  {
    return 1;
  }

                             
  conn->asyncStatus = PGASYNC_BUSY;
  resetPQExpBuffer(&conn->errorMessage);

     
                                                                            
                                                                            
                                                                            
                                                                          
                                                                           
     
  if (pqIsnonblocking(conn) && PQisBusy(conn))
  {
    return 1;
  }

                                        
  result = PQgetResult(conn);

                                     
  if (result && result->resultStatus == PGRES_COMMAND_OK)
  {
    PQclear(result);
    return 0;
  }

     
                                                                      
                                                                      
                                                                    
                                                                          
                                    
     
  if (conn->errorMessage.len > 0)
  {
                                                                   
    char svLast = conn->errorMessage.data[conn->errorMessage.len - 1];

    if (svLast == '\n')
    {
      conn->errorMessage.data[conn->errorMessage.len - 1] = '\0';
    }
    pqInternalNotice(&conn->noticeHooks, "%s", conn->errorMessage.data);
    conn->errorMessage.data[conn->errorMessage.len - 1] = svLast;
  }

  PQclear(result);

  return 1;
}

   
                                                        
   
                                    
   
PGresult *
pqFunctionCall3(PGconn *conn, Oid fnid, int *result_buf, int *actual_result_len, int result_is_int, const PQArgBlock *args, int nargs)
{
  bool needInput = false;
  ExecStatusType status = PGRES_FATAL_ERROR;
  char id;
  int msgLength;
  int avail;
  int i;

                                               

  if (pqPutMsgStart('F', false, conn) < 0 ||                        
      pqPutInt(fnid, 4, conn) < 0 ||                          
      pqPutInt(1, 2, conn) < 0 ||                                   
      pqPutInt(1, 2, conn) < 0 ||                                     
      pqPutInt(nargs, 2, conn) < 0)                         
  {
                                                
    return NULL;
  }

  for (i = 0; i < nargs; ++i)
  {                             
    if (pqPutInt(args[i].len, 4, conn))
    {
      return NULL;
    }
    if (args[i].len == -1)
    {
      continue;                
    }

    if (args[i].isint)
    {
      if (pqPutInt(args[i].u.integer, args[i].len, conn))
      {
        return NULL;
      }
    }
    else
    {
      if (pqPutnchar((char *)args[i].u.ptr, args[i].len, conn))
      {
        return NULL;
      }
    }
  }

  if (pqPutInt(1, 2, conn) < 0)                                 
  {
    return NULL;
  }

  if (pqPutMsgEnd(conn) < 0 || pqFlush(conn))
  {
    return NULL;
  }

  for (;;)
  {
    if (needInput)
    {
                                                                      
      if (pqWait(true, false, conn) || pqReadData(conn) < 0)
      {
        break;
      }
    }

       
                                                                          
       
    needInput = true;

    conn->inCursor = conn->inStart;
    if (pqGetc(&id, conn))
    {
      continue;
    }
    if (pqGetInt(&msgLength, 4, conn))
    {
      continue;
    }

       
                                                                          
                                                                           
                      
       
    if (msgLength < 4)
    {
      handleSyncLoss(conn, id, msgLength);
      break;
    }
    if (msgLength > 30000 && !VALID_LONG_MESSAGE_TYPE(id))
    {
      handleSyncLoss(conn, id, msgLength);
      break;
    }

       
                                                         
       
    msgLength -= 4;
    avail = conn->inEnd - conn->inCursor;
    if (avail < msgLength)
    {
         
                                                                        
                                                  
         
      if (pqCheckInBufferSpace(conn->inCursor + (size_t)msgLength, conn))
      {
           
                                                                     
                                                                       
                                                                      
                      
           
        handleSyncLoss(conn, id, msgLength);
        break;
      }
      continue;
    }

       
                                                                     
                                                                          
                  
       
    switch (id)
    {
    case 'V':                      
      if (pqGetInt(actual_result_len, 4, conn))
      {
        continue;
      }
      if (*actual_result_len != -1)
      {
        if (result_is_int)
        {
          if (pqGetInt(result_buf, *actual_result_len, conn))
          {
            continue;
          }
        }
        else
        {
          if (pqGetnchar((char *)result_buf, *actual_result_len, conn))
          {
            continue;
          }
        }
      }
                                                      
      status = PGRES_COMMAND_OK;
      break;
    case 'E':                   
      if (pqGetErrorNotice3(conn, true))
      {
        continue;
      }
      status = PGRES_FATAL_ERROR;
      break;
    case 'A':                     
                                                                 
      if (getNotify(conn))
      {
        continue;
      }
      break;
    case 'N':             
                                                                 
      if (pqGetErrorNotice3(conn, false))
      {
        continue;
      }
      break;
    case 'Z':                                     
      if (getReadyForQuery(conn))
      {
        continue;
      }
                                        
      conn->inStart += 5 + msgLength;
                                                                   
      if (conn->result)
      {
        return pqPrepareAsyncResult(conn);
      }
      return PQmakeEmptyPGresult(conn, status);
    case 'S':                       
      if (getParameterStatus(conn))
      {
        continue;
      }
      break;
    default:
                                              
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("protocol error: id=0x%x\n"), id);
      pqSaveErrorResult(conn);
                                                              
      conn->inStart += 5 + msgLength;
      return pqPrepareAsyncResult(conn);
    }
                                            
                                                            
    conn->inStart += 5 + msgLength;
    needInput = false;
  }

     
                                                             
                                                                         
                                                      
     
  pqSaveErrorResult(conn);
  return pqPrepareAsyncResult(conn);
}

   
                            
   
                                                              
   
char *
pqBuildStartupPacket3(PGconn *conn, int *packetlen, const PQEnvironmentOption *options)
{
  char *startpacket;

  *packetlen = build_startup_packet(conn, NULL, options);
  startpacket = (char *)malloc(*packetlen);
  if (!startpacket)
  {
    return NULL;
  }
  *packetlen = build_startup_packet(conn, startpacket, options);
  return startpacket;
}

   
                                                              
   
                                                                    
                                                                          
                                                                       
                                                                             
                  
   
static int
build_startup_packet(const PGconn *conn, char *packet, const PQEnvironmentOption *options)
{
  int packet_len = 0;
  const PQEnvironmentOption *next_eo;
  const char *val;

                                     
  if (packet)
  {
    ProtocolVersion pv = pg_hton32(conn->pversion);

    memcpy(packet + packet_len, &pv, sizeof(ProtocolVersion));
  }
  packet_len += sizeof(ProtocolVersion);

                                             

#define ADD_STARTUP_OPTION(optname, optval)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (packet)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      strcpy(packet + packet_len, optname);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    packet_len += strlen(optname) + 1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
    if (packet)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      strcpy(packet + packet_len, optval);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    packet_len += strlen(optval) + 1;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  } while (0)

  if (conn->pguser && conn->pguser[0])
  {
    ADD_STARTUP_OPTION("user", conn->pguser);
  }
  if (conn->dbName && conn->dbName[0])
  {
    ADD_STARTUP_OPTION("database", conn->dbName);
  }
  if (conn->replication && conn->replication[0])
  {
    ADD_STARTUP_OPTION("replication", conn->replication);
  }
  if (conn->pgoptions && conn->pgoptions[0])
  {
    ADD_STARTUP_OPTION("options", conn->pgoptions);
  }
  if (conn->send_appname)
  {
                                                        
    val = conn->appname ? conn->appname : conn->fbappname;
    if (val && val[0])
    {
      ADD_STARTUP_OPTION("application_name", val);
    }
  }

  if (conn->client_encoding_initial && conn->client_encoding_initial[0])
  {
    ADD_STARTUP_OPTION("client_encoding", conn->client_encoding_initial);
  }

                                                      
  for (next_eo = options; next_eo->envName; next_eo++)
  {
    if ((val = getenv(next_eo->envName)) != NULL)
    {
      if (pg_strcasecmp(val, "default") != 0)
      {
        ADD_STARTUP_OPTION(next_eo->pgName, val);
      }
    }
  }

                               
  if (packet)
  {
    packet[packet_len] = '\0';
  }
  packet_len++;

  return packet_len;
}
