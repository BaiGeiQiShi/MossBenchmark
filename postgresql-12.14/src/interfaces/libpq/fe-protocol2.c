                                                                            
   
                  
                                                                        
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres_fe.h"

#include <ctype.h>
#include <fcntl.h>

#include "libpq-fe.h"
#include "libpq-int.h"
#include "port/pg_bswap.h"

#ifdef WIN32
#include "win32.h"
#else
#include <unistd.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#endif

static int
getRowDescriptions(PGconn *conn);
static int
getAnotherTuple(PGconn *conn, bool binary);
static int
pqGetErrorNotice2(PGconn *conn, bool isError);
static void
checkXactStatus(PGconn *conn, const char *cmdTag);
static int
getNotify(PGconn *conn);

   
                 
   
                                                                            
                             
   
PostgresPollingStatusType
pqSetenvPoll(PGconn *conn)
{
  PGresult *res;

  if (conn == NULL || conn->status == CONNECTION_BAD)
  {
    return PGRES_POLLING_FAILED;
  }

                                               
  switch (conn->setenv_state)
  {
                                  
  case SETENV_STATE_CLIENT_ENCODING_WAIT:
  case SETENV_STATE_OPTION_WAIT:
  case SETENV_STATE_QUERY1_WAIT:
  case SETENV_STATE_QUERY2_WAIT:
  {
                           
    int n = pqReadData(conn);

    if (n < 0)
    {
      goto error_return;
    }
    if (n == 0)
    {
      return PGRES_POLLING_READING;
    }

    break;
  }

                                                       
  case SETENV_STATE_CLIENT_ENCODING_SEND:
  case SETENV_STATE_OPTION_SEND:
  case SETENV_STATE_QUERY1_SEND:
  case SETENV_STATE_QUERY2_SEND:
    break;

                                                             
  case SETENV_STATE_IDLE:
    return PGRES_POLLING_OK;

  default:
    printfPQExpBuffer(&conn->errorMessage,
        libpq_gettext("invalid setenv state %c, "
                      "probably indicative of memory corruption\n"),
        conn->setenv_state);
    goto error_return;
  }

                                                                         
  for (;;)
  {
    switch (conn->setenv_state)
    {
         
                                                                   
                                                                     
                                  
         
    case SETENV_STATE_CLIENT_ENCODING_SEND:
    {
      char setQuery[100];                         
                                             
      const char *val = conn->client_encoding_initial;

      if (val)
      {
        if (pg_strcasecmp(val, "default") == 0)
        {
          sprintf(setQuery, "SET client_encoding = DEFAULT");
        }
        else
        {
          sprintf(setQuery, "SET client_encoding = '%.60s'", val);
        }
#ifdef CONNECTDEBUG
        fprintf(stderr, "Sending client_encoding with %s\n", setQuery);
#endif
        if (!PQsendQuery(conn, setQuery))
        {
          goto error_return;
        }

        conn->setenv_state = SETENV_STATE_CLIENT_ENCODING_WAIT;
      }
      else
      {
        conn->setenv_state = SETENV_STATE_OPTION_SEND;
      }
      break;
    }

    case SETENV_STATE_OPTION_SEND:
    {
         
                                                             
                                                                 
                                                       
                         
         
      char setQuery[100];                         
                                             

      if (conn->next_eo->envName)
      {
        const char *val;

        if ((val = getenv(conn->next_eo->envName)))
        {
          if (pg_strcasecmp(val, "default") == 0)
          {
            sprintf(setQuery, "SET %s = DEFAULT", conn->next_eo->pgName);
          }
          else
          {
            sprintf(setQuery, "SET %s = '%.60s'", conn->next_eo->pgName, val);
          }
#ifdef CONNECTDEBUG
          fprintf(stderr, "Use environment variable %s to send %s\n", conn->next_eo->envName, setQuery);
#endif
          if (!PQsendQuery(conn, setQuery))
          {
            goto error_return;
          }

          conn->setenv_state = SETENV_STATE_OPTION_WAIT;
        }
        else
        {
          conn->next_eo++;
        }
      }
      else
      {
                                                             
        conn->setenv_state = SETENV_STATE_QUERY1_SEND;
      }
      break;
    }

    case SETENV_STATE_CLIENT_ENCODING_WAIT:
    {
      if (PQisBusy(conn))
      {
        return PGRES_POLLING_READING;
      }

      res = PQgetResult(conn);

      if (res)
      {
        if (PQresultStatus(res) != PGRES_COMMAND_OK)
        {
          PQclear(res);
          goto error_return;
        }
        PQclear(res);
                                                         
      }
      else
      {
                                                     
        conn->setenv_state = SETENV_STATE_OPTION_SEND;
      }
      break;
    }

    case SETENV_STATE_OPTION_WAIT:
    {
      if (PQisBusy(conn))
      {
        return PGRES_POLLING_READING;
      }

      res = PQgetResult(conn);

      if (res)
      {
        if (PQresultStatus(res) != PGRES_COMMAND_OK)
        {
          PQclear(res);
          goto error_return;
        }
        PQclear(res);
                                                         
      }
      else
      {
                                                     
        conn->next_eo++;
        conn->setenv_state = SETENV_STATE_OPTION_SEND;
      }
      break;
    }

    case SETENV_STATE_QUERY1_SEND:
    {
         
                                                               
                                                               
                          
         
                                                               
                                                      
                                                            
                                             
         
      if (!PQsendQuery(conn, "begin; select version(); end"))
      {
        goto error_return;
      }

      conn->setenv_state = SETENV_STATE_QUERY1_WAIT;
      return PGRES_POLLING_READING;
    }

    case SETENV_STATE_QUERY1_WAIT:
    {
      if (PQisBusy(conn))
      {
        return PGRES_POLLING_READING;
      }

      res = PQgetResult(conn);

      if (res)
      {
        char *val;

        if (PQresultStatus(res) == PGRES_COMMAND_OK)
        {
                                                   
          PQclear(res);
          continue;
        }

        if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1)
        {
          PQclear(res);
          goto error_return;
        }

           
                                                 
                           
           
        val = PQgetvalue(res, 0, 0);
        if (val && strncmp(val, "PostgreSQL ", 11) == 0)
        {
          char *ptr;

                                         
          val += 11;

             
                                                           
                              
             
          ptr = strchr(val, ' ');
          if (ptr)
          {
            *ptr = '\0';
          }

          pqSaveParameterStatus(conn, "server_version", val);
        }

        PQclear(res);
                                                         
      }
      else
      {
                                          
        conn->setenv_state = SETENV_STATE_QUERY2_SEND;
      }
      break;
    }

    case SETENV_STATE_QUERY2_SEND:
    {
      const char *query;

         
                                                               
                                                                
                                                                
                                                           
                             
         
      if (conn->sversion >= 70300 && conn->sversion < 70400)
      {
        query = "begin; select pg_catalog.pg_client_encoding(); end";
      }
      else
      {
        query = "select pg_client_encoding()";
      }
      if (!PQsendQuery(conn, query))
      {
        goto error_return;
      }

      conn->setenv_state = SETENV_STATE_QUERY2_WAIT;
      return PGRES_POLLING_READING;
    }

    case SETENV_STATE_QUERY2_WAIT:
    {
      if (PQisBusy(conn))
      {
        return PGRES_POLLING_READING;
      }

      res = PQgetResult(conn);

      if (res)
      {
        const char *val;

        if (PQresultStatus(res) == PGRES_COMMAND_OK)
        {
                                                   
          PQclear(res);
          continue;
        }

        if (PQresultStatus(res) == PGRES_TUPLES_OK && PQntuples(res) == 1)
        {
                                                   
          val = PQgetvalue(res, 0, 0);
          if (val && *val)                                  
          {
            pqSaveParameterStatus(conn, "client_encoding", val);
          }
        }
        else
        {
             
                                                          
                                                      
                       
             
          val = getenv("PGCLIENTENCODING");
          if (val && *val)
          {
            pqSaveParameterStatus(conn, "client_encoding", val);
          }
          else
          {
            pqSaveParameterStatus(conn, "client_encoding", "SQL_ASCII");
          }
        }

        PQclear(res);
                                                         
      }
      else
      {
                                           
        conn->setenv_state = SETENV_STATE_IDLE;
        return PGRES_POLLING_OK;
      }
      break;
    }

    default:
      printfPQExpBuffer(&conn->errorMessage,
          libpq_gettext("invalid state %c, "
                        "probably indicative of memory corruption\n"),
          conn->setenv_state);
      goto error_return;
    }
  }

                   

error_return:
  conn->setenv_state = SETENV_STATE_IDLE;
  return PGRES_POLLING_FAILED;
}

   
                                                             
                                                            
                                                                                
   
void
pqParseInput2(PGconn *conn)
{
  char id;

     
                                                                         
     
  for (;;)
  {
       
                                                                           
                                                                           
                                                                          
                                                          
       
    if (conn->asyncStatus == PGASYNC_COPY_OUT)
    {
      return;
    }

       
                                              
       
    conn->inCursor = conn->inStart;
    if (pqGetc(&id, conn))
    {
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
      if (pqGetErrorNotice2(conn, false))
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
        if (pqGetErrorNotice2(conn, false                      ))
        {
          return;
        }
      }
      else
      {
        pqInternalNotice(&conn->noticeHooks, "message type 0x%02x arrived from server while idle", id);
                                                         
        conn->inStart = conn->inEnd;
        break;
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
        checkXactStatus(conn, conn->workBuffer.data);
        conn->asyncStatus = PGASYNC_READY;
        break;
      case 'E':                   
        if (pqGetErrorNotice2(conn, true))
        {
          return;
        }
        conn->asyncStatus = PGASYNC_READY;
        break;
      case 'Z':                                     
        conn->asyncStatus = PGASYNC_IDLE;
        break;
      case 'I':                  
                                                  
        if (pqGetc(&id, conn))
        {
          return;
        }
        if (id != '\0')
        {
          pqInternalNotice(&conn->noticeHooks, "unexpected character %c following empty query response (\"I\" message)", id);
        }
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
      case 'P':                                  
        if (pqGets(&conn->workBuffer, conn))
        {
          return;
        }
                                                        
        break;
      case 'T':                                                
        if (conn->result == NULL)
        {
                                             
          if (getRowDescriptions(conn))
          {
            return;
          }
                                                         
          continue;
        }
        else
        {
             
                                                          
                                                              
                                                                
                                                               
                     
             
          conn->asyncStatus = PGASYNC_READY;
          return;
        }
        break;
      case 'D':                       
        if (conn->result != NULL)
        {
                                                             
          if (getAnotherTuple(conn, false))
          {
            return;
          }
                                                      
          continue;
        }
        else
        {
          pqInternalNotice(&conn->noticeHooks, "server sent data (\"D\" message) without prior row description (\"T\" message)");
                                                           
          conn->inStart = conn->inEnd;
          return;
        }
        break;
      case 'B':                        
        if (conn->result != NULL)
        {
                                                             
          if (getAnotherTuple(conn, true))
          {
            return;
          }
                                                      
          continue;
        }
        else
        {
          pqInternalNotice(&conn->noticeHooks, "server sent binary data (\"B\" message) without prior row description (\"T\" message)");
                                                           
          conn->inStart = conn->inEnd;
          return;
        }
        break;
      case 'G':                    
        conn->asyncStatus = PGASYNC_COPY_IN;
        break;
      case 'H':                     
        conn->asyncStatus = PGASYNC_COPY_OUT;
        break;

           
                                                                  
                                                              
           
      default:
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("unexpected response from server; first received character was \"%c\"\n"), id);
                                                             
        pqSaveErrorResult(conn);
                                                         
        conn->inStart = conn->inEnd;
        conn->asyncStatus = PGASYNC_READY;
        return;
      }                                   
    }
                                            
    conn->inStart = conn->inCursor;
  }
}

   
                                                                   
                                                                
                                                                    
                 
   
                                                                     
                                                                      
                                              
   
static int
getRowDescriptions(PGconn *conn)
{
  PGresult *result;
  int nfields;
  const char *errmsg;
  int i;

  result = PQmakeEmptyPGresult(conn, PGRES_TUPLES_OK);
  if (!result)
  {
    errmsg = NULL;                                       
    goto advance_and_error;
  }

                                              
                                                   
  if (pqGetInt(&(result->numAttributes), 2, conn))
  {
    goto EOFexit;
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

                     
  for (i = 0; i < nfields; i++)
  {
    int typid;
    int typlen;
    int atttypmod;

    if (pqGets(&conn->workBuffer, conn) || pqGetInt(&typid, 4, conn) || pqGetInt(&typlen, 2, conn) || pqGetInt(&atttypmod, 4, conn))
    {
      goto EOFexit;
    }

       
                                                                     
                                         
       
    typlen = (int)((int16)typlen);

    result->attDescs[i].name = pqResultStrdup(result, conn->workBuffer.data);
    if (!result->attDescs[i].name)
    {
      errmsg = NULL;                                       
      goto advance_and_error;
    }
    result->attDescs[i].tableid = 0;
    result->attDescs[i].columnid = 0;
    result->attDescs[i].format = 0;
    result->attDescs[i].typid = typid;
    result->attDescs[i].typlen = typlen;
    result->attDescs[i].atttypmod = atttypmod;
  }

                
  conn->result = result;

                                                                        
  conn->inStart = conn->inCursor;

     
                                                                            
                                     
     

                       
  return 0;

advance_and_error:

     
                                                                             
                                                                            
                                                                    
     
  conn->inStart = conn->inEnd;

     
                                                                      
                                                            
     
  pqClearAsyncResult(conn);

     
                                                                       
                                                                           
                                                                           
                                              
     
  if (!errmsg)
  {
    errmsg = libpq_gettext("out of memory for query result");
  }

  printfPQExpBuffer(&conn->errorMessage, "%s\n", errmsg);

     
                                                                           
                      
     
  conn->result = PQmakeEmptyPGresult(conn, PGRES_FATAL_ERROR);
  conn->asyncStatus = PGASYNC_READY;

EOFexit:
  if (result && result != conn->result)
  {
    PQclear(result);
  }
  return EOF;
}

   
                                                                  
                                                                        
                                                                    
                 
   
                                                                     
                                                                      
                                              
   
static int
getAnotherTuple(PGconn *conn, bool binary)
{
  PGresult *result = conn->result;
  int nfields = result->numAttributes;
  const char *errmsg;
  PGdataValue *rowbuf;

                                                                  
  char std_bitmap[64];                                 
  char *bitmap = std_bitmap;
  int i;
  size_t nbytes;                                        
  char bmap;                                    
  int bitmap_index;                
  int bitcnt;                                                    
  int vlen;                                                

                                   
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

                             
  result->binary = binary;

     
                                                                      
                                                              
     
  if (binary)
  {
    for (i = 0; i < nfields; i++)
    {
      result->attDescs[i].format = 1;
    }
  }

                                 
  nbytes = (nfields + BITS_PER_BYTE - 1) / BITS_PER_BYTE;
                                                         
  if (nbytes > sizeof(std_bitmap))
  {
    bitmap = (char *)malloc(nbytes);
    if (!bitmap)
    {
      errmsg = NULL;                                       
      goto advance_and_error;
    }
  }

  if (pqGetnchar(bitmap, nbytes, conn))
  {
    goto EOFexit;
  }

                       
  bitmap_index = 0;
  bmap = bitmap[bitmap_index];
  bitcnt = 0;

  for (i = 0; i < nfields; i++)
  {
                              
    if (!(bmap & 0200))
    {
      vlen = NULL_LEN;
    }
    else if (pqGetInt(&vlen, 4, conn))
    {
      goto EOFexit;
    }
    else
    {
      if (!binary)
      {
        vlen = vlen - 4;
      }
      if (vlen < 0)
      {
        vlen = 0;
      }
    }
    rowbuf[i].len = vlen;

       
                                                                     
                                                                        
                                        
       
    rowbuf[i].value = conn->inBuffer + conn->inCursor;

                                  
    if (vlen > 0)
    {
      if (pqSkipnchar(vlen, conn))
      {
        goto EOFexit;
      }
    }

                                  
    bitcnt++;
    if (bitcnt == BITS_PER_BYTE)
    {
      bitmap_index++;
      bmap = bitmap[bitmap_index];
      bitcnt = 0;
    }
    else
    {
      bmap <<= 1;
    }
  }

                                             
  if (bitmap != std_bitmap)
  {
    free(bitmap);
  }
  bitmap = NULL;

                                                                        
  conn->inStart = conn->inCursor;

                                 
  errmsg = NULL;
  if (pqRowProcessor(conn, &errmsg))
  {
    return 0;                              
  }

  goto set_error_result;                                       

advance_and_error:

     
                                                                             
                                                                            
                                                                    
     
  conn->inStart = conn->inEnd;

set_error_result:

     
                                                                      
                                                            
     
  pqClearAsyncResult(conn);

     
                                                                       
                                                                           
                                                                           
                                              
     
  if (!errmsg)
  {
    errmsg = libpq_gettext("out of memory for query result");
  }

  printfPQExpBuffer(&conn->errorMessage, "%s\n", errmsg);

     
                                                                           
                      
     
  conn->result = PQmakeEmptyPGresult(conn, PGRES_FATAL_ERROR);
  conn->asyncStatus = PGASYNC_READY;

EOFexit:
  if (bitmap != NULL && bitmap != std_bitmap)
  {
    free(bitmap);
  }
  return EOF;
}

   
                                                        
                                                                           
                                                             
                                                     
                                     
   
static int
pqGetErrorNotice2(PGconn *conn, bool isError)
{
  PGresult *res = NULL;
  PQExpBufferData workBuf;
  char *startp;
  char *splitp;

     
                                                                           
                                                                    
                                                                         
     
  if (isError)
  {
    pqClearAsyncResult(conn);
  }

     
                                                                   
                                                                             
                                             
     
  initPQExpBuffer(&workBuf);
  if (pqGets(&workBuf, conn))
  {
    goto failure;
  }

     
                                                                        
                                                                       
                         
     
                                                                             
                                                                         
                                                
     
  res = PQmakeEmptyPGresult(conn, PGRES_EMPTY_QUERY);
  if (res)
  {
    res->resultStatus = isError ? PGRES_FATAL_ERROR : PGRES_NONFATAL_ERROR;
    res->errMsg = pqResultStrdup(res, workBuf.data);
  }

     
                                                                            
                                                                             
                                                                      
                                                                             
                                                                            
            
     
  while (workBuf.len > 0 && workBuf.data[workBuf.len - 1] == '\n')
  {
    workBuf.data[--workBuf.len] = '\0';
  }
  splitp = strstr(workBuf.data, ":  ");
  if (splitp)
  {
                                                 
    *splitp = '\0';
    pqSaveMessageField(res, PG_DIAG_SEVERITY, workBuf.data);
    startp = splitp + 3;
  }
  else
  {
                                         
    startp = workBuf.data;
  }
  splitp = strchr(startp, '\n');
  if (splitp)
  {
                                                          
    *splitp++ = '\0';
    pqSaveMessageField(res, PG_DIAG_MESSAGE_PRIMARY, startp);
                                                          
    while (*splitp && isspace((unsigned char)*splitp))
    {
      splitp++;
    }
    pqSaveMessageField(res, PG_DIAG_MESSAGE_DETAIL, splitp);
  }
  else
  {
                                             
    pqSaveMessageField(res, PG_DIAG_MESSAGE_PRIMARY, startp);
  }

     
                                                                         
                                                                           
                                                        
     
  if (isError)
  {
    pqClearAsyncResult(conn);                             
    conn->result = res;
    resetPQExpBuffer(&conn->errorMessage);
    if (res && !PQExpBufferDataBroken(workBuf) && res->errMsg)
    {
      appendPQExpBufferStr(&conn->errorMessage, res->errMsg);
    }
    else
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory"));
    }
    if (conn->xactStatus == PQTRANS_INTRANS)
    {
      conn->xactStatus = PQTRANS_INERROR;
    }
  }
  else
  {
    if (res)
    {
      if (res->noticeHooks.noticeRec != NULL)
      {
        res->noticeHooks.noticeRec(res->noticeHooks.noticeRecArg, res);
      }
      PQclear(res);
    }
  }

  termPQExpBuffer(&workBuf);
  return 0;

failure:
  if (res)
  {
    PQclear(res);
  }
  termPQExpBuffer(&workBuf);
  return EOF;
}

   
                                                                         
   
                                                                       
                                                                        
                                                                        
                                                                       
                                          
   
                                                                          
                                         
   
static void
checkXactStatus(PGconn *conn, const char *cmdTag)
{
  if (strcmp(cmdTag, "BEGIN") == 0)
  {
    conn->xactStatus = PQTRANS_INTRANS;
  }
  else if (strcmp(cmdTag, "COMMIT") == 0)
  {
    conn->xactStatus = PQTRANS_IDLE;
  }
  else if (strcmp(cmdTag, "ROLLBACK") == 0)
  {
    conn->xactStatus = PQTRANS_IDLE;
  }
  else if (strcmp(cmdTag, "START TRANSACTION") == 0)               
  {
    conn->xactStatus = PQTRANS_INTRANS;
  }

     
                                                                       
                                                                           
                           
     
  else if (strcmp(cmdTag, "*ABORT STATE*") == 0)                   
  {
    conn->xactStatus = PQTRANS_INERROR;
  }
}

   
                                              
                                                                           
                                                                  
                                                            
                                     
   
static int
getNotify(PGconn *conn)
{
  int be_pid;
  int nmlen;
  PGnotify *newNotify;

  if (pqGetInt(&be_pid, 4, conn))
  {
    return EOF;
  }
  if (pqGets(&conn->workBuffer, conn))
  {
    return EOF;
  }

     
                                                                          
                                                                           
                                                             
     
  nmlen = strlen(conn->workBuffer.data);
  newNotify = (PGnotify *)malloc(sizeof(PGnotify) + nmlen + 1);
  if (newNotify)
  {
    newNotify->relname = (char *)newNotify + sizeof(PGnotify);
    strcpy(newNotify->relname, conn->workBuffer.data);
                                             
    newNotify->extra = newNotify->relname + nmlen;
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

  return 0;
}

   
                                                                       
   
                                                                       
                                              
                                                                       
                                                                    
                    
   
int
pqGetCopyData2(PGconn *conn, char **buffer, int async)
{
  bool found;
  int msgLength;

  for (;;)
  {
       
                                           
       
    conn->inCursor = conn->inStart;
    found = false;
    while (conn->inCursor < conn->inEnd)
    {
      char c = conn->inBuffer[conn->inCursor++];

      if (c == '\n')
      {
        found = true;
        break;
      }
    }
    if (!found)
    {
      goto nodata;
    }
    msgLength = conn->inCursor - conn->inStart;

       
                                                                           
                                                  
       
    if (msgLength == 3 && strncmp(&conn->inBuffer[conn->inStart], "\\.\n", 3) == 0)
    {
      conn->inStart = conn->inCursor;
      conn->asyncStatus = PGASYNC_BUSY;
      return -1;
    }

       
                                         
       
    *buffer = (char *)malloc(msgLength + 1);
    if (*buffer == NULL)
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
      return -2;
    }
    memcpy(*buffer, &conn->inBuffer[conn->inStart], msgLength);
    (*buffer)[msgLength] = '\0';                           

                               
    conn->inStart = conn->inCursor;

    return msgLength;

  nodata:
                                             
    if (async)
    {
      return 0;
    }
                                
    if (pqWait(true, false, conn) || pqReadData(conn) < 0)
    {
      return -2;
    }
  }
}

   
                                                                  
   
                                    
   
int
pqGetline2(PGconn *conn, char *s, int maxlen)
{
  int result = 1;                                       

  if (conn->sock == PGINVALID_SOCKET || conn->asyncStatus != PGASYNC_COPY_OUT)
  {
    *s = '\0';
    return EOF;
  }

     
                                                                             
                                                  
     
  while (maxlen > 1)
  {
    if (conn->inStart < conn->inEnd)
    {
      char c = conn->inBuffer[conn->inStart++];

      if (c == '\n')
      {
        result = 0;                   
        break;
      }
      *s++ = c;
      maxlen--;
    }
    else
    {
                                  
      if (pqWait(true, false, conn) || pqReadData(conn) < 0)
      {
        result = EOF;
        break;
      }
    }
  }
  *s = '\0';

  return result;
}

   
                                                           
   
                                    
   
int
pqGetlineAsync2(PGconn *conn, char *buffer, int bufsize)
{
  int avail;

  if (conn->asyncStatus != PGASYNC_COPY_OUT)
  {
    return -1;                                 
  }

     
                                                                           
                                                                            
                                                                            
                                                                           
                                                        
     

  conn->inCursor = conn->inStart;

  avail = bufsize;
  while (avail > 0 && conn->inCursor < conn->inEnd)
  {
    char c = conn->inBuffer[conn->inCursor++];

    *buffer++ = c;
    --avail;
    if (c == '\n')
    {
                                                                 
      conn->inStart = conn->inCursor;
                                     
      if (bufsize - avail == 3 && buffer[-3] == '\\' && buffer[-2] == '.')
      {
        return -1;
      }
                                                  
      return bufsize - avail;
    }
  }

     
                                                                       
                                                                             
                                                                             
                                                                          
                                                                            
                                                          
     
  if (avail == 0 && bufsize > 3)
  {
    conn->inStart = conn->inCursor - 3;
    return bufsize - 3;
  }
  return 0;
}

   
             
   
                                    
   
int
pqEndcopy2(PGconn *conn)
{
  PGresult *result;

  if (conn->asyncStatus != PGASYNC_COPY_IN && conn->asyncStatus != PGASYNC_COPY_OUT)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("no COPY in progress\n"));
    return 1;
  }

     
                                                                           
                         
     
  if (pqFlush(conn) && pqIsnonblocking(conn))
  {
    return 1;
  }

                                                                 
  if (pqIsnonblocking(conn) && PQisBusy(conn))
  {
    return 1;
  }

                             
  conn->asyncStatus = PGASYNC_BUSY;
  resetPQExpBuffer(&conn->errorMessage);

                                        
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

     
                                                                             
                                                                            
                                                     
     
  pqInternalNotice(&conn->noticeHooks, "lost synchronization with server, resetting connection");

     
                                                                   
                                                                             
            
     
  if (pqIsnonblocking(conn))
  {
    PQresetStart(conn);
  }
  else
  {
    PQreset(conn);
  }

  return 1;
}

   
                                                        
   
                                    
   
PGresult *
pqFunctionCall2(PGconn *conn, Oid fnid, int *result_buf, int *actual_result_len, int result_is_int, const PQArgBlock *args, int nargs)
{
  bool needInput = false;
  ExecStatusType status = PGRES_FATAL_ERROR;
  char id;
  int i;

                                               

  if (pqPutMsgStart('F', false, conn) < 0 ||                        
      pqPuts(" ", conn) < 0 ||                                 
      pqPutInt(fnid, 4, conn) != 0 ||                         
      pqPutInt(nargs, 4, conn) != 0)                        
  {
                                                
    return NULL;
  }

  for (i = 0; i < nargs; ++i)
  {                             
    if (pqPutInt(args[i].len, 4, conn))
    {
      return NULL;
    }

    if (args[i].isint)
    {
      if (pqPutInt(args[i].u.integer, 4, conn))
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

       
                                                                          
       
    conn->inCursor = conn->inStart;
    needInput = true;

    if (pqGetc(&id, conn))
    {
      continue;
    }

       
                                                                     
                                                                          
                  
       
    switch (id)
    {
    case 'V':                      
      if (pqGetc(&id, conn))
      {
        continue;
      }
      if (id == 'G')
      {
                                              
        if (pqGetInt(actual_result_len, 4, conn))
        {
          continue;
        }
        if (result_is_int)
        {
          if (pqGetInt(result_buf, 4, conn))
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
        if (pqGetc(&id, conn))                       
        {
          continue;
        }
      }
      if (id == '0')
      {
                                                        
        status = PGRES_COMMAND_OK;
      }
      else
      {
                                                
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("protocol error: id=0x%x\n"), id);
        pqSaveErrorResult(conn);
        conn->inStart = conn->inCursor;
        return pqPrepareAsyncResult(conn);
      }
      break;
    case 'E':                   
      if (pqGetErrorNotice2(conn, true))
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
                                                                 
      if (pqGetErrorNotice2(conn, false))
      {
        continue;
      }
      break;
    case 'Z':                                     
                                        
      conn->inStart = conn->inCursor;
                                                                   
      if (conn->result)
      {
        return pqPrepareAsyncResult(conn);
      }
      return PQmakeEmptyPGresult(conn, status);
    default:
                                              
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("protocol error: id=0x%x\n"), id);
      pqSaveErrorResult(conn);
      conn->inStart = conn->inCursor;
      return pqPrepareAsyncResult(conn);
    }
                                            
    conn->inStart = conn->inCursor;
    needInput = false;
  }

     
                                                             
                                                                         
                                                      
     
  pqSaveErrorResult(conn);
  return pqPrepareAsyncResult(conn);
}

   
                            
   
                                                              
   
char *
pqBuildStartupPacket2(PGconn *conn, int *packetlen, const PQEnvironmentOption *options)
{
  StartupPacket *startpacket;

  *packetlen = sizeof(StartupPacket);
  startpacket = (StartupPacket *)malloc(sizeof(StartupPacket));
  if (!startpacket)
  {
    return NULL;
  }

  MemSet(startpacket, 0, sizeof(StartupPacket));

  startpacket->protoVersion = pg_hton32(conn->pversion);

                                                                          
  strncpy(startpacket->user, conn->pguser, SM_USER);
  strncpy(startpacket->database, conn->dbName, SM_DATABASE);
  strncpy(startpacket->tty, conn->pgtty, SM_TTY);

  if (conn->pgoptions)
  {
    strncpy(startpacket->options, conn->pgoptions, SM_OPTIONS);
  }

  return (char *)startpacket;
}
