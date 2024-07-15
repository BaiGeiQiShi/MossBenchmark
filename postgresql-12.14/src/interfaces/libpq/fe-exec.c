                                                                            
   
             
                                                              
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                                            
   
#include "postgres_fe.h"

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>

#include "libpq-fe.h"
#include "libpq-int.h"

#include "mb/pg_wchar.h"

#ifdef WIN32
#include "win32.h"
#else
#include <unistd.h>
#endif

                                                             
char *const pgresStatus[] = {"PGRES_EMPTY_QUERY", "PGRES_COMMAND_OK", "PGRES_TUPLES_OK", "PGRES_COPY_OUT", "PGRES_COPY_IN", "PGRES_BAD_RESPONSE", "PGRES_NONFATAL_ERROR", "PGRES_FATAL_ERROR", "PGRES_COPY_BOTH", "PGRES_SINGLE_TUPLE"};

   
                                                                          
                                                      
   
static int static_client_encoding = PG_SQL_ASCII;
static bool static_std_strings = false;

static PGEvent *
dupEvents(PGEvent *events, int count, size_t *memSize);
static bool
pqAddTuple(PGresult *res, PGresAttValue *tup, const char **errmsgp);
static bool
PQsendQueryStart(PGconn *conn);
static int
PQsendQueryGuts(PGconn *conn, const char *command, const char *stmtName, int nParams, const Oid *paramTypes, const char *const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat);
static void
parseInput(PGconn *conn);
static PGresult *
getCopyResult(PGconn *conn, ExecStatusType copytype);
static bool
PQexecStart(PGconn *conn);
static PGresult *
PQexecFinish(PGconn *conn);
static int
PQsendDescribe(PGconn *conn, char desc_type, const char *desc_target);
static int
check_field_number(const PGresult *res, int field_num);

                    
                                  
   
                                                                        
                                                                       
                                                                       
                                                                        
                                                                         
                                                                         
                                                                          
                                                                    
                                              
                                                                         
                                                                          
                                                                          
                                           
   
                                                                            
                                                                       
                                                                           
                                                               
                                                                               
                                                                             
                                                                          
                                
                                                                           
                                                                          
                        
   
                                                                          
                                                                             
                                                                             
                                                                               
                                                                              
   
                                                 
                                                                          
                                                                          
                                                                             
                                                                      
                                          
                                                                            
                                                                      
                                              
                                                             
                             
                                                                        
                    
                                                                        
                                                                           
                    
   

#define PGRESULT_DATA_BLOCKSIZE 2048
#define PGRESULT_ALIGN_BOUNDARY MAXIMUM_ALIGNOF                     
#define PGRESULT_BLOCK_OVERHEAD Max(sizeof(PGresult_data), PGRESULT_ALIGN_BOUNDARY)
#define PGRESULT_SEP_ALLOC_THRESHOLD (PGRESULT_DATA_BLOCKSIZE / 2)

   
                       
                                                                       
                                                                  
                                                                          
   
PGresult *
PQmakeEmptyPGresult(PGconn *conn, ExecStatusType status)
{
  PGresult *result;

  result = (PGresult *)malloc(sizeof(PGresult));
  if (!result)
  {
    return NULL;
  }

  result->ntups = 0;
  result->numAttributes = 0;
  result->attDescs = NULL;
  result->tuples = NULL;
  result->tupArrSize = 0;
  result->numParameters = 0;
  result->paramDescs = NULL;
  result->resultStatus = status;
  result->cmdStatus[0] = '\0';
  result->binary = 0;
  result->events = NULL;
  result->nEvents = 0;
  result->errMsg = NULL;
  result->errFields = NULL;
  result->errQuery = NULL;
  result->null_field[0] = '\0';
  result->curBlock = NULL;
  result->curOffset = 0;
  result->spaceLeft = 0;
  result->memorySize = sizeof(PGresult);

  if (conn)
  {
                                                                       
    result->noticeHooks = conn->noticeHooks;
    result->client_encoding = conn->client_encoding;

                                              
    switch (status)
    {
    case PGRES_EMPTY_QUERY:
    case PGRES_COMMAND_OK:
    case PGRES_TUPLES_OK:
    case PGRES_COPY_OUT:
    case PGRES_COPY_IN:
    case PGRES_COPY_BOTH:
    case PGRES_SINGLE_TUPLE:
                           
      break;
    default:
      pqSetResultError(result, conn->errorMessage.data);
      break;
    }

                                                                      
    if (conn->nEvents > 0)
    {
      result->events = dupEvents(conn->events, conn->nEvents, &result->memorySize);
      if (!result->events)
      {
        PQclear(result);
        return NULL;
      }
      result->nEvents = conn->nEvents;
    }
  }
  else
  {
                     
    result->noticeHooks.noticeRec = NULL;
    result->noticeHooks.noticeRecArg = NULL;
    result->noticeHooks.noticeProc = NULL;
    result->noticeHooks.noticeProcArg = NULL;
    result->client_encoding = PG_SQL_ASCII;
  }

  return result;
}

   
                    
   
                                                                            
                                                                     
                                                                 
                                                                  
                             
   
int
PQsetResultAttrs(PGresult *res, int numAttributes, PGresAttDesc *attDescs)
{
  int i;

                                                           
  if (!res || res->numAttributes > 0)
  {
    return false;
  }

                            
  if (numAttributes <= 0 || !attDescs)
  {
    return true;
  }

  res->attDescs = (PGresAttDesc *)PQresultAlloc(res, numAttributes * sizeof(PGresAttDesc));

  if (!res->attDescs)
  {
    return false;
  }

  res->numAttributes = numAttributes;
  memcpy(res->attDescs, attDescs, numAttributes * sizeof(PGresAttDesc));

                                                           
  res->binary = 1;
  for (i = 0; i < res->numAttributes; i++)
  {
    if (res->attDescs[i].name)
    {
      res->attDescs[i].name = pqResultStrdup(res, res->attDescs[i].name);
    }
    else
    {
      res->attDescs[i].name = res->null_field;
    }

    if (!res->attDescs[i].name)
    {
      return false;
    }

    if (res->attDescs[i].format == 0)
    {
      res->binary = 0;
    }
  }

  return true;
}

   
                
   
                                                                             
                                                                           
                                                             
                                                                           
                          
   
                                                                           
                                                                   
                                                                     
                               
   
            
                                                           
   
                                                                       
                                                                      
   
                                                         
   
                                                                    
   
PGresult *
PQcopyResult(const PGresult *src, int flags)
{
  PGresult *dest;
  int i;

  if (!src)
  {
    return NULL;
  }

  dest = PQmakeEmptyPGresult(NULL, PGRES_TUPLES_OK);
  if (!dest)
  {
    return NULL;
  }

                                                                 
  dest->client_encoding = src->client_encoding;
  strcpy(dest->cmdStatus, src->cmdStatus);

                    
  if (flags & (PG_COPYRES_ATTRS | PG_COPYRES_TUPLES))
  {
    if (!PQsetResultAttrs(dest, src->numAttributes, src->attDescs))
    {
      PQclear(dest);
      return NULL;
    }
  }

                             
  if (flags & PG_COPYRES_TUPLES)
  {
    int tup, field;

    for (tup = 0; tup < src->ntups; tup++)
    {
      for (field = 0; field < src->numAttributes; field++)
      {
        if (!PQsetvalue(dest, tup, field, src->tuples[tup][field].value, src->tuples[tup][field].len))
        {
          PQclear(dest);
          return NULL;
        }
      }
    }
  }

                                   
  if (flags & PG_COPYRES_NOTICEHOOKS)
  {
    dest->noticeHooks = src->noticeHooks;
  }

                               
  if ((flags & PG_COPYRES_EVENTS) && src->nEvents > 0)
  {
    dest->events = dupEvents(src->events, src->nEvents, &dest->memorySize);
    if (!dest->events)
    {
      PQclear(dest);
      return NULL;
    }
    dest->nEvents = src->nEvents;
  }

                                            
  for (i = 0; i < dest->nEvents; i++)
  {
    if (src->events[i].resultInitialized)
    {
      PGEventResultCopy evt;

      evt.src = src;
      evt.dest = dest;
      if (!dest->events[i].proc(PGEVT_RESULTCOPY, &evt, dest->events[i].passThrough))
      {
        PQclear(dest);
        return NULL;
      }
      dest->events[i].resultInitialized = true;
    }
  }

  return dest;
}

   
                                                             
                                                                  
                                                      
                                                   
   
static PGEvent *
dupEvents(PGEvent *events, int count, size_t *memSize)
{
  PGEvent *newEvents;
  size_t msize;
  int i;

  if (!events || count <= 0)
  {
    return NULL;
  }

  msize = count * sizeof(PGEvent);
  newEvents = (PGEvent *)malloc(msize);
  if (!newEvents)
  {
    return NULL;
  }

  for (i = 0; i < count; i++)
  {
    newEvents[i].proc = events[i].proc;
    newEvents[i].passThrough = events[i].passThrough;
    newEvents[i].data = NULL;
    newEvents[i].resultInitialized = false;
    newEvents[i].name = strdup(events[i].name);
    if (!newEvents[i].name)
    {
      while (--i >= 0)
      {
        free(newEvents[i].name);
      }
      free(newEvents);
      return NULL;
    }
    msize += strlen(events[i].name) + 1;
  }

  *memSize += msize;
  return newEvents;
}

   
                                                                       
                                                                        
                        
                                                              
                                                                      
   
int
PQsetvalue(PGresult *res, int tup_num, int field_num, char *value, int len)
{
  PGresAttValue *attval;
  const char *errmsg = NULL;

                                                                
  if (!check_field_number(res, field_num))
  {
    return false;
  }

                                         
  if (tup_num < 0 || tup_num > res->ntups)
  {
    pqInternalNotice(&res->noticeHooks, "row number %d is out of range 0..%d", tup_num, res->ntups);
    return false;
  }

                                     
  if (tup_num == res->ntups)
  {
    PGresAttValue *tup;
    int i;

    tup = (PGresAttValue *)pqResultAlloc(res, res->numAttributes * sizeof(PGresAttValue), true);

    if (!tup)
    {
      goto fail;
    }

                                        
    for (i = 0; i < res->numAttributes; i++)
    {
      tup[i].len = NULL_LEN;
      tup[i].value = res->null_field;
    }

                             
    if (!pqAddTuple(res, tup, &errmsg))
    {
      goto fail;
    }
  }

  attval = &res->tuples[tup_num][field_num];

                                                                   
  if (len == NULL_LEN || value == NULL)
  {
    attval->len = NULL_LEN;
    attval->value = res->null_field;
  }
  else if (len <= 0)
  {
    attval->len = 0;
    attval->value = res->null_field;
  }
  else
  {
    attval->value = (char *)pqResultAlloc(res, len + 1, true);
    if (!attval->value)
    {
      goto fail;
    }
    attval->len = len;
    memcpy(attval->value, value, len);
    attval->value[len] = '\0';
  }

  return true;

     
                                                                            
                                                         
     
fail:
  if (!errmsg)
  {
    errmsg = libpq_gettext("out of memory");
  }
  pqInternalNotice(&res->noticeHooks, "%s", errmsg);

  return false;
}

   
                                                                             
   
                                                                       
                                      
   
void *
PQresultAlloc(PGresult *res, size_t nBytes)
{
  return pqResultAlloc(res, nBytes, true);
}

   
                   
                                                
   
                                                        
                                                                      
                                  
                                                                       
                                      
   
void *
pqResultAlloc(PGresult *res, size_t nBytes, bool isBinary)
{
  char *space;
  PGresult_data *block;

  if (!res)
  {
    return NULL;
  }

  if (nBytes <= 0)
  {
    return res->null_field;
  }

     
                                                                           
               
     
  if (isBinary)
  {
    int offset = res->curOffset % PGRESULT_ALIGN_BOUNDARY;

    if (offset)
    {
      res->curOffset += PGRESULT_ALIGN_BOUNDARY - offset;
      res->spaceLeft -= PGRESULT_ALIGN_BOUNDARY - offset;
    }
  }

                                                                 
  if (nBytes <= (size_t)res->spaceLeft)
  {
    space = res->curBlock->space + res->curOffset;
    res->curOffset += nBytes;
    res->spaceLeft -= nBytes;
    return space;
  }

     
                                                                        
                                                                           
                                                                            
                                                                         
     
  if (nBytes >= PGRESULT_SEP_ALLOC_THRESHOLD)
  {
    size_t alloc_size = nBytes + PGRESULT_BLOCK_OVERHEAD;

    block = (PGresult_data *)malloc(alloc_size);
    if (!block)
    {
      return NULL;
    }
    res->memorySize += alloc_size;
    space = block->space + PGRESULT_BLOCK_OVERHEAD;
    if (res->curBlock)
    {
         
                                                                     
                                                           
         
      block->next = res->curBlock->next;
      res->curBlock->next = block;
    }
    else
    {
                                                                
      block->next = NULL;
      res->curBlock = block;
      res->spaceLeft = 0;                               
    }
    return space;
  }

                                     
  block = (PGresult_data *)malloc(PGRESULT_DATA_BLOCKSIZE);
  if (!block)
  {
    return NULL;
  }
  res->memorySize += PGRESULT_DATA_BLOCKSIZE;
  block->next = res->curBlock;
  res->curBlock = block;
  if (isBinary)
  {
                                     
    res->curOffset = PGRESULT_BLOCK_OVERHEAD;
    res->spaceLeft = PGRESULT_DATA_BLOCKSIZE - PGRESULT_BLOCK_OVERHEAD;
  }
  else
  {
                                                         
    res->curOffset = sizeof(PGresult_data);
    res->spaceLeft = PGRESULT_DATA_BLOCKSIZE - sizeof(PGresult_data);
  }

  space = block->space + res->curOffset;
  res->curOffset += nBytes;
  res->spaceLeft -= nBytes;
  return space;
}

   
                        
                                                    
   
size_t
PQresultMemorySize(const PGresult *res)
{
  if (!res)
  {
    return 0;
  }
  return res->memorySize;
}

   
                    
                                                             
   
char *
pqResultStrdup(PGresult *res, const char *str)
{
  char *space = (char *)pqResultAlloc(res, strlen(str) + 1, false);

  if (space)
  {
    strcpy(space, str);
  }
  return space;
}

   
                      
                                             
   
void
pqSetResultError(PGresult *res, const char *msg)
{
  if (!res)
  {
    return;
  }
  if (msg && *msg)
  {
    res->errMsg = pqResultStrdup(res, msg);
  }
  else
  {
    res->errMsg = NULL;
  }
}

   
                           
                                                                     
   
void
pqCatenateResultError(PGresult *res, const char *msg)
{
  PQExpBufferData errorBuf;

  if (!res || !msg)
  {
    return;
  }
  initPQExpBuffer(&errorBuf);
  if (res->errMsg)
  {
    appendPQExpBufferStr(&errorBuf, res->errMsg);
  }
  appendPQExpBufferStr(&errorBuf, msg);
  pqSetResultError(res, errorBuf.data);
  termPQExpBuffer(&errorBuf);
}

   
             
                                                  
   
void
PQclear(PGresult *res)
{
  PGresult_data *block;
  int i;

  if (!res)
  {
    return;
  }

  for (i = 0; i < res->nEvents; i++)
  {
                                                                   
    if (res->events[i].resultInitialized)
    {
      PGEventResultDestroy evt;

      evt.result = res;
      (void)res->events[i].proc(PGEVT_RESULTDESTROY, &evt, res->events[i].passThrough);
    }
    free(res->events[i].name);
  }

  if (res->events)
  {
    free(res->events);
  }

                                      
  while ((block = res->curBlock) != NULL)
  {
    res->curBlock = block->next;
    free(block);
  }

                                              
  if (res->tuples)
  {
    free(res->tuples);
  }

                                                               
  res->attDescs = NULL;
  res->tuples = NULL;
  res->paramDescs = NULL;
  res->errFields = NULL;
  res->events = NULL;
  res->nEvents = 0;
                                            

                                          
  free(res);
}

   
                                                                          
   
                                       
   
void
pqClearAsyncResult(PGconn *conn)
{
  if (conn->result)
  {
    PQclear(conn->result);
  }
  conn->result = NULL;
  if (conn->next_result)
  {
    PQclear(conn->next_result);
  }
  conn->next_result = NULL;
}

   
                                                                        
                                                                       
                                                                       
                                                                      
                                                                         
                                                                       
                                                                             
                                                                             
                                                                        
                                                    
   
void
pqSaveErrorResult(PGconn *conn)
{
     
                                                                             
                                            
     
  if (conn->result == NULL || conn->result->resultStatus != PGRES_FATAL_ERROR || conn->result->errMsg == NULL)
  {
    pqClearAsyncResult(conn);
    conn->result = PQmakeEmptyPGresult(conn, PGRES_FATAL_ERROR);
  }
  else
  {
                                                                   
    pqCatenateResultError(conn->result, conn->errorMessage.data);
  }
}

   
                                                                             
                                                                           
                                                
   
static void
pqSaveWriteError(PGconn *conn)
{
     
                                                                 
                               
     
  pqSaveErrorResult(conn);

     
                                                                         
                                                                             
                                                
     
  if (conn->write_err_msg && conn->write_err_msg[0] != '\0')
  {
    pqCatenateResultError(conn->result, conn->write_err_msg);
  }
  else
  {
    pqCatenateResultError(conn->result, libpq_gettext("write to server failed\n"));
  }
}

   
                                                                             
                                                                         
                                                                          
                                                                            
                 
   
PGresult *
pqPrepareAsyncResult(PGconn *conn)
{
  PGresult *res;

     
                                                                            
                                                                          
                         
     
  res = conn->result;
  if (!res)
  {
    res = PQmakeEmptyPGresult(conn, PGRES_FATAL_ERROR);
  }
  else
  {
       
                                                                          
                                         
       
    resetPQExpBuffer(&conn->errorMessage);
    appendPQExpBufferStr(&conn->errorMessage, PQresultErrorMessage(res));
  }

     
                                                                        
                                                                        
                                                                             
                                                             
     
  conn->result = conn->next_result;
  conn->next_result = NULL;

  return res;
}

   
                                                                     
   
                                                                          
                                              
   
                                                                             
                                                              
   
void
pqInternalNotice(const PGNoticeHooks *hooks, const char *fmt, ...)
{
  char msgBuf[1024];
  va_list args;
  PGresult *res;

  if (hooks->noticeRec == NULL)
  {
    return;                                     
  }

                          
  va_start(args, fmt);
  vsnprintf(msgBuf, sizeof(msgBuf), libpq_gettext(fmt), args);
  va_end(args);
  msgBuf[sizeof(msgBuf) - 1] = '\0';                                     

                                                      
  res = PQmakeEmptyPGresult(NULL, PGRES_NONFATAL_ERROR);
  if (!res)
  {
    return;
  }
  res->noticeHooks = *hooks;

     
                              
     
  pqSaveMessageField(res, PG_DIAG_MESSAGE_PRIMARY, msgBuf);
  pqSaveMessageField(res, PG_DIAG_SEVERITY, libpq_gettext("NOTICE"));
  pqSaveMessageField(res, PG_DIAG_SEVERITY_NONLOCALIZED, "NOTICE");
                                          

     
                                                                           
                                                      
     
  res->errMsg = (char *)pqResultAlloc(res, strlen(msgBuf) + 2, false);
  if (res->errMsg)
  {
    sprintf(res->errMsg, "%s\n", msgBuf);

       
                                       
       
    res->noticeHooks.noticeRec(res->noticeHooks.noticeRecArg, res);
  }
  PQclear(res);
}

   
              
                                                                          
                                                                    
   
                                                                    
                                                                    
   
static bool
pqAddTuple(PGresult *res, PGresAttValue *tup, const char **errmsgp)
{
  if (res->ntups >= res->tupArrSize)
  {
       
                              
       
                                                                      
                                                                          
                                                                       
                                                                    
                                                                       
                                                                          
                                      
       
    int newSize;
    PGresAttValue **newTuples;

       
                                                                         
                                                            
       
    if (res->tupArrSize <= INT_MAX / 2)
    {
      newSize = (res->tupArrSize > 0) ? res->tupArrSize * 2 : 128;
    }
    else if (res->tupArrSize < INT_MAX)
    {
      newSize = INT_MAX;
    }
    else
    {
      *errmsgp = libpq_gettext("PGresult cannot support more than INT_MAX tuples");
      return false;
    }

       
                                                                           
                                                                        
                                               
       
#if INT_MAX >= (SIZE_MAX / 2)
    if (newSize > SIZE_MAX / sizeof(PGresAttValue *))
    {
      *errmsgp = libpq_gettext("size_t overflow");
      return false;
    }
#endif

    if (res->tuples == NULL)
    {
      newTuples = (PGresAttValue **)malloc(newSize * sizeof(PGresAttValue *));
    }
    else
    {
      newTuples = (PGresAttValue **)realloc(res->tuples, newSize * sizeof(PGresAttValue *));
    }
    if (!newTuples)
    {
      return false;                               
    }
    res->memorySize += (newSize - res->tupArrSize) * sizeof(PGresAttValue *);
    res->tupArrSize = newSize;
    res->tuples = newTuples;
  }
  res->tuples[res->ntups] = tup;
  res->ntups++;
  return true;
}

   
                                                                     
   
void
pqSaveMessageField(PGresult *res, char code, const char *value)
{
  PGMessageField *pfield;

  pfield = (PGMessageField *)pqResultAlloc(res, offsetof(PGMessageField, contents) + strlen(value) + 1, true);
  if (!pfield)
  {
    return;                     
  }
  pfield->code = code;
  strcpy(pfield->contents, value);
  pfield->next = res->errFields;
  res->errFields = pfield;
}

   
                                                                     
   
void
pqSaveParameterStatus(PGconn *conn, const char *name, const char *value)
{
  pgParameterStatus *pstatus;
  pgParameterStatus *prev;

  if (conn->Pfdebug)
  {
    fprintf(conn->Pfdebug, "pqSaveParameterStatus: '%s' = '%s'\n", name, value);
  }

     
                                                    
     
  for (pstatus = conn->pstatus, prev = NULL; pstatus != NULL; prev = pstatus, pstatus = pstatus->next)
  {
    if (strcmp(pstatus->name, name) == 0)
    {
      if (prev)
      {
        prev->next = pstatus->next;
      }
      else
      {
        conn->pstatus = pstatus->next;
      }
      free(pstatus);                                       
      break;
    }
  }

     
                                             
     
  pstatus = (pgParameterStatus *)malloc(sizeof(pgParameterStatus) + strlen(name) + strlen(value) + 2);
  if (pstatus)
  {
    char *ptr;

    ptr = ((char *)pstatus) + sizeof(pgParameterStatus);
    pstatus->name = ptr;
    strcpy(ptr, name);
    ptr += strlen(name) + 1;
    pstatus->value = ptr;
    strcpy(ptr, value);
    pstatus->next = conn->pstatus;
    conn->pstatus = pstatus;
  }

     
                                                 
                                                                          
                                                                           
                                                                          
                                                 
     
  if (strcmp(name, "client_encoding") == 0)
  {
    conn->client_encoding = pg_char_to_encoding(value);
                                                                         
    if (conn->client_encoding < 0)
    {
      conn->client_encoding = PG_SQL_ASCII;
    }
    static_client_encoding = conn->client_encoding;
  }
  else if (strcmp(name, "standard_conforming_strings") == 0)
  {
    conn->std_strings = (strcmp(value, "on") == 0);
    static_std_strings = conn->std_strings;
  }
  else if (strcmp(name, "server_version") == 0)
  {
    int cnt;
    int vmaj, vmin, vrev;

    cnt = sscanf(value, "%d.%d.%d", &vmaj, &vmin, &vrev);

    if (cnt == 3)
    {
                                 
      conn->sversion = (100 * vmaj + vmin) * 100 + vrev;
    }
    else if (cnt == 2)
    {
      if (vmaj >= 10)
      {
                                  
        conn->sversion = 100 * 100 * vmaj + vmin;
      }
      else
      {
                                                            
        conn->sversion = (100 * vmaj + vmin) * 100;
      }
    }
    else if (cnt == 1)
    {
                                                         
      conn->sversion = 100 * 100 * vmaj;
    }
    else
    {
      conn->sversion = 0;              
    }
  }
}

   
                  
                                                                      
                                           
   
                                                                    
                                                                    
   
                                                                            
                                                                        
                                                                               
                                                                   
   
int
pqRowProcessor(PGconn *conn, const char **errmsgp)
{
  PGresult *res = conn->result;
  int nfields = res->numAttributes;
  const PGdataValue *columns = conn->rowBuf;
  PGresAttValue *tup;
  int i;

     
                                                                          
                                                                             
                                            
     
  if (conn->singleRowMode)
  {
                                                                    
    res = PQcopyResult(res, PG_COPYRES_ATTRS | PG_COPYRES_EVENTS | PG_COPYRES_NOTICEHOOKS);
    if (!res)
    {
      return 0;
    }
  }

     
                                                                         
                         
     
                                                                             
                                                                             
                                                                         
                                          
     
  tup = (PGresAttValue *)pqResultAlloc(res, nfields * sizeof(PGresAttValue), true);
  if (tup == NULL)
  {
    goto fail;
  }

  for (i = 0; i < nfields; i++)
  {
    int clen = columns[i].len;

    if (clen < 0)
    {
                      
      tup[i].len = NULL_LEN;
      tup[i].value = res->null_field;
    }
    else
    {
      bool isbinary = (res->attDescs[i].format != 0);
      char *val;

      val = (char *)pqResultAlloc(res, clen + 1, isbinary);
      if (val == NULL)
      {
        goto fail;
      }

                                                                  
      memcpy(val, columns[i].value, clen);
      val[clen] = '\0';

      tup[i].len = clen;
      tup[i].value = val;
    }
  }

                                                       
  if (!pqAddTuple(res, tup, errmsgp))
  {
    goto fail;
  }

     
                                                                           
                  
     
  if (conn->singleRowMode)
  {
                                                          
    res->resultStatus = PGRES_SINGLE_TUPLE;
                                           
    conn->next_result = conn->result;
    conn->result = res;
                                             
    conn->asyncStatus = PGASYNC_READY;
  }

  return 1;

fail:
                                                          
  if (res != conn->result)
  {
    PQclear(res);
  }
  return 0;
}

   
               
                                                    
   
                                        
                                            
   
int
PQsendQuery(PGconn *conn, const char *query)
{
  if (!PQsendQueryStart(conn))
  {
    return 0;
  }

                          
  if (!query)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("command string is a null pointer\n"));
    return 0;
  }

                                            
  if (pqPutMsgStart('Q', false, conn) < 0 || pqPuts(query, conn) < 0 || pqPutMsgEnd(conn) < 0)
  {
                                                
    return 0;
  }

                                                   
  conn->queryclass = PGQUERY_SIMPLE;

                                                    
                                                             
  if (conn->last_query)
  {
    free(conn->last_query);
  }
  conn->last_query = strdup(query);

     
                                                                             
                                                                           
     
  if (pqFlush(conn) < 0)
  {
                                                
    return 0;
  }

                          
  conn->asyncStatus = PGASYNC_BUSY;
  return 1;
}

   
                     
                                                                     
   
int
PQsendQueryParams(PGconn *conn, const char *command, int nParams, const Oid *paramTypes, const char *const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat)
{
  if (!PQsendQueryStart(conn))
  {
    return 0;
  }

                           
  if (!command)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("command string is a null pointer\n"));
    return 0;
  }
  if (nParams < 0 || nParams > 65535)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("number of parameters must be between 0 and 65535\n"));
    return 0;
  }

  return PQsendQueryGuts(conn, command, "",                            
      nParams, paramTypes, paramValues, paramLengths, paramFormats, resultFormat);
}

   
                 
                                                            
   
                                        
                                            
   
int
PQsendPrepare(PGconn *conn, const char *stmtName, const char *query, int nParams, const Oid *paramTypes)
{
  if (!PQsendQueryStart(conn))
  {
    return 0;
  }

                           
  if (!stmtName)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("statement name is a null pointer\n"));
    return 0;
  }
  if (!query)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("command string is a null pointer\n"));
    return 0;
  }
  if (nParams < 0 || nParams > 65535)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("number of parameters must be between 0 and 65535\n"));
    return 0;
  }

                                             
  if (PG_PROTOCOL_MAJOR(conn->pversion) < 3)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("function requires at least protocol version 3.0\n"));
    return 0;
  }

                                   
  if (pqPutMsgStart('P', false, conn) < 0 || pqPuts(stmtName, conn) < 0 || pqPuts(query, conn) < 0)
  {
    goto sendFailed;
  }

  if (nParams > 0 && paramTypes)
  {
    int i;

    if (pqPutInt(nParams, 2, conn) < 0)
    {
      goto sendFailed;
    }
    for (i = 0; i < nParams; i++)
    {
      if (pqPutInt(paramTypes[i], 4, conn) < 0)
      {
        goto sendFailed;
      }
    }
  }
  else
  {
    if (pqPutInt(0, 2, conn) < 0)
    {
      goto sendFailed;
    }
  }
  if (pqPutMsgEnd(conn) < 0)
  {
    goto sendFailed;
  }

                                  
  if (pqPutMsgStart('S', false, conn) < 0 || pqPutMsgEnd(conn) < 0)
  {
    goto sendFailed;
  }

                                          
  conn->queryclass = PGQUERY_PREPARE;

                                                    
                                                             
  if (conn->last_query)
  {
    free(conn->last_query);
  }
  conn->last_query = strdup(query);

     
                                                                             
                                                                           
     
  if (pqFlush(conn) < 0)
  {
    goto sendFailed;
  }

                          
  conn->asyncStatus = PGASYNC_BUSY;
  return 1;

sendFailed:
                                              
  return 0;
}

   
                       
                                                                   
                                                 
   
int
PQsendQueryPrepared(PGconn *conn, const char *stmtName, int nParams, const char *const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat)
{
  if (!PQsendQueryStart(conn))
  {
    return 0;
  }

                           
  if (!stmtName)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("statement name is a null pointer\n"));
    return 0;
  }
  if (nParams < 0 || nParams > 65535)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("number of parameters must be between 0 and 65535\n"));
    return 0;
  }

  return PQsendQueryGuts(conn, NULL,                          
      stmtName, nParams, NULL,                           
      paramValues, paramLengths, paramFormats, resultFormat);
}

   
                                                            
   
static bool
PQsendQueryStart(PGconn *conn)
{
  if (!conn)
  {
    return false;
  }

                              
  resetPQExpBuffer(&conn->errorMessage);

                                                                
  if (conn->status != CONNECTION_OK)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("no connection to the server\n"));
    return false;
  }
                                              
  if (conn->asyncStatus != PGASYNC_IDLE)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("another command is already in progress\n"));
    return false;
  }

                                                  
  pqClearAsyncResult(conn);

                                        
  conn->singleRowMode = false;

                                     
  return true;
}

   
                   
                                               
                                            
   
                                                                        
   
static int
PQsendQueryGuts(PGconn *conn, const char *command, const char *stmtName, int nParams, const Oid *paramTypes, const char *const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat)
{
  int i;

                                             
  if (PG_PROTOCOL_MAJOR(conn->pversion) < 3)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("function requires at least protocol version 3.0\n"));
    return 0;
  }

     
                                                                           
                                                            
     

  if (command)
  {
                                     
    if (pqPutMsgStart('P', false, conn) < 0 || pqPuts(stmtName, conn) < 0 || pqPuts(command, conn) < 0)
    {
      goto sendFailed;
    }
    if (nParams > 0 && paramTypes)
    {
      if (pqPutInt(nParams, 2, conn) < 0)
      {
        goto sendFailed;
      }
      for (i = 0; i < nParams; i++)
      {
        if (pqPutInt(paramTypes[i], 4, conn) < 0)
        {
          goto sendFailed;
        }
      }
    }
    else
    {
      if (pqPutInt(0, 2, conn) < 0)
      {
        goto sendFailed;
      }
    }
    if (pqPutMsgEnd(conn) < 0)
    {
      goto sendFailed;
    }
  }

                                  
  if (pqPutMsgStart('B', false, conn) < 0 || pqPuts("", conn) < 0 || pqPuts(stmtName, conn) < 0)
  {
    goto sendFailed;
  }

                              
  if (nParams > 0 && paramFormats)
  {
    if (pqPutInt(nParams, 2, conn) < 0)
    {
      goto sendFailed;
    }
    for (i = 0; i < nParams; i++)
    {
      if (pqPutInt(paramFormats[i], 2, conn) < 0)
      {
        goto sendFailed;
      }
    }
  }
  else
  {
    if (pqPutInt(0, 2, conn) < 0)
    {
      goto sendFailed;
    }
  }

  if (pqPutInt(nParams, 2, conn) < 0)
  {
    goto sendFailed;
  }

                       
  for (i = 0; i < nParams; i++)
  {
    if (paramValues && paramValues[i])
    {
      int nbytes;

      if (paramFormats && paramFormats[i] != 0)
      {
                              
        if (paramLengths)
        {
          nbytes = paramLengths[i];
        }
        else
        {
          printfPQExpBuffer(&conn->errorMessage, libpq_gettext("length must be given for binary parameter\n"));
          goto sendFailed;
        }
      }
      else
      {
                                                     
        nbytes = strlen(paramValues[i]);
      }
      if (pqPutInt(nbytes, 4, conn) < 0 || pqPutnchar(paramValues[i], nbytes, conn) < 0)
      {
        goto sendFailed;
      }
    }
    else
    {
                                  
      if (pqPutInt(-1, 4, conn) < 0)
      {
        goto sendFailed;
      }
    }
  }
  if (pqPutInt(1, 2, conn) < 0 || pqPutInt(resultFormat, 2, conn))
  {
    goto sendFailed;
  }
  if (pqPutMsgEnd(conn) < 0)
  {
    goto sendFailed;
  }

                                             
  if (pqPutMsgStart('D', false, conn) < 0 || pqPutc('P', conn) < 0 || pqPuts("", conn) < 0 || pqPutMsgEnd(conn) < 0)
  {
    goto sendFailed;
  }

                                     
  if (pqPutMsgStart('E', false, conn) < 0 || pqPuts("", conn) < 0 || pqPutInt(0, 4, conn) < 0 || pqPutMsgEnd(conn) < 0)
  {
    goto sendFailed;
  }

                                  
  if (pqPutMsgStart('S', false, conn) < 0 || pqPutMsgEnd(conn) < 0)
  {
    goto sendFailed;
  }

                                                     
  conn->queryclass = PGQUERY_EXTENDED;

                                                    
                                                             
  if (conn->last_query)
  {
    free(conn->last_query);
  }
  if (command)
  {
    conn->last_query = strdup(command);
  }
  else
  {
    conn->last_query = NULL;
  }

     
                                                                             
                                                                           
     
  if (pqFlush(conn) < 0)
  {
    goto sendFailed;
  }

                          
  conn->asyncStatus = PGASYNC_BUSY;
  return 1;

sendFailed:
                                              
  return 0;
}

   
                                     
   
int
PQsetSingleRowMode(PGconn *conn)
{
     
                                                                           
                           
     
  if (!conn)
  {
    return 0;
  }
  if (conn->asyncStatus != PGASYNC_BUSY)
  {
    return 0;
  }
  if (conn->queryclass != PGQUERY_SIMPLE && conn->queryclass != PGQUERY_EXTENDED)
  {
    return 0;
  }
  if (conn->result)
  {
    return 0;
  }

                    
  conn->singleRowMode = true;
  return 1;
}

   
                                                
                                  
                        
   
int
PQconsumeInput(PGconn *conn)
{
  if (!conn)
  {
    return 0;
  }

     
                                                                            
                                                                           
                                            
     
  if (pqIsnonblocking(conn))
  {
    if (pqFlush(conn) < 0)
    {
      return 0;
    }
  }

     
                                                                          
                                                                            
                                                                        
                             
     
  if (pqReadData(conn) < 0)
  {
    return 0;
  }

                                             
  return 1;
}

   
                                                             
                                                            
                                                                                
   
static void
parseInput(PGconn *conn)
{
  if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
  {
    pqParseInput3(conn);
  }
  else
  {
    pqParseInput2(conn);
  }
}

   
            
                                                              
   

int
PQisBusy(PGconn *conn)
{
  if (!conn)
  {
    return false;
  }

                                                       
  parseInput(conn);

     
                                                                           
                                                                          
                                                                          
                                                                         
                                                                          
                                        
     
  return conn->asyncStatus == PGASYNC_BUSY && conn->status != CONNECTION_BAD;
}

   
               
                                                                    
                                                              
              
   

PGresult *
PQgetResult(PGconn *conn)
{
  PGresult *res;

  if (!conn)
  {
    return NULL;
  }

                                                       
  parseInput(conn);

                                                             
  while (conn->asyncStatus == PGASYNC_BUSY)
  {
    int flushResult;

       
                                                                          
                                                            
       
    while ((flushResult = pqFlush(conn)) > 0)
    {
      if (pqWait(false, true, conn))
      {
        flushResult = -1;
        break;
      }
    }

       
                                                                           
                                                                      
                                                                         
                                                                          
                                                           
       
    if (flushResult || pqWait(true, false, conn) || pqReadData(conn) < 0)
    {
         
                                                                     
                                                                  
         
      pqSaveErrorResult(conn);
      conn->asyncStatus = PGASYNC_IDLE;
      return pqPrepareAsyncResult(conn);
    }

                   
    parseInput(conn);

       
                                                                          
                                                         
       
    if (conn->write_failed && conn->asyncStatus == PGASYNC_BUSY)
    {
      pqSaveWriteError(conn);
      conn->asyncStatus = PGASYNC_IDLE;
      return pqPrepareAsyncResult(conn);
    }
  }

                                     
  switch (conn->asyncStatus)
  {
  case PGASYNC_IDLE:
    res = NULL;                        
    break;
  case PGASYNC_READY:
    res = pqPrepareAsyncResult(conn);
                                                                  
    conn->asyncStatus = PGASYNC_BUSY;
    break;
  case PGASYNC_COPY_IN:
    res = getCopyResult(conn, PGRES_COPY_IN);
    break;
  case PGASYNC_COPY_OUT:
    res = getCopyResult(conn, PGRES_COPY_OUT);
    break;
  case PGASYNC_COPY_BOTH:
    res = getCopyResult(conn, PGRES_COPY_BOTH);
    break;
  default:
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("unexpected asyncStatus: %d\n"), (int)conn->asyncStatus);
    res = PQmakeEmptyPGresult(conn, PGRES_FATAL_ERROR);
    break;
  }

  if (res)
  {
    int i;

    for (i = 0; i < res->nEvents; i++)
    {
      PGEventResultCreate evt;

      evt.conn = conn;
      evt.result = res;
      if (!res->events[i].proc(PGEVT_RESULTCREATE, &evt, res->events[i].passThrough))
      {
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("PGEventProc \"%s\" failed during PGEVT_RESULTCREATE event\n"), res->events[i].name);
        pqSetResultError(res, conn->errorMessage.data);
        res->resultStatus = PGRES_FATAL_ERROR;
        break;
      }
      res->events[i].resultInitialized = true;
    }
  }

  return res;
}

   
                 
                                                                        
   
static PGresult *
getCopyResult(PGconn *conn, ExecStatusType copytype)
{
     
                                                                         
                                                                          
                                                                             
                                                                           
                                                                          
                                                                    
     
  if (conn->status != CONNECTION_OK)
  {
    pqSaveErrorResult(conn);
    conn->asyncStatus = PGASYNC_IDLE;
    return pqPrepareAsyncResult(conn);
  }

                                                            
  if (conn->result && conn->result->resultStatus == copytype)
  {
    return pqPrepareAsyncResult(conn);
  }

                                             
  return PQmakeEmptyPGresult(conn, copytype);
}

   
          
                                                                         
   
                                                                             
                       
                                                                           
                               
                                                                  
                      
   
PGresult *
PQexec(PGconn *conn, const char *query)
{
  if (!PQexecStart(conn))
  {
    return NULL;
  }
  if (!PQsendQuery(conn, query))
  {
    return NULL;
  }
  return PQexecFinish(conn);
}

   
                
                                                                
   
PGresult *
PQexecParams(PGconn *conn, const char *command, int nParams, const Oid *paramTypes, const char *const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat)
{
  if (!PQexecStart(conn))
  {
    return NULL;
  }
  if (!PQsendQueryParams(conn, command, nParams, paramTypes, paramValues, paramLengths, paramFormats, resultFormat))
  {
    return NULL;
  }
  return PQexecFinish(conn);
}

   
             
                                                                   
   
                                                                             
                       
                                                                           
                               
                                                                  
                      
   
PGresult *
PQprepare(PGconn *conn, const char *stmtName, const char *query, int nParams, const Oid *paramTypes)
{
  if (!PQexecStart(conn))
  {
    return NULL;
  }
  if (!PQsendPrepare(conn, stmtName, query, nParams, paramTypes))
  {
    return NULL;
  }
  return PQexecFinish(conn);
}

   
                  
                                                              
                                                 
   
PGresult *
PQexecPrepared(PGconn *conn, const char *stmtName, int nParams, const char *const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat)
{
  if (!PQexecStart(conn))
  {
    return NULL;
  }
  if (!PQsendQueryPrepared(conn, stmtName, nParams, paramValues, paramLengths, paramFormats, resultFormat))
  {
    return NULL;
  }
  return PQexecFinish(conn);
}

   
                                                                        
   
static bool
PQexecStart(PGconn *conn)
{
  PGresult *result;

  if (!conn)
  {
    return false;
  }

     
                                                                          
                                                                             
     
  while ((result = PQgetResult(conn)) != NULL)
  {
    ExecStatusType resultStatus = result->resultStatus;

    PQclear(result);                           
    if (resultStatus == PGRES_COPY_IN)
    {
      if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
      {
                                                              
        if (PQputCopyEnd(conn, libpq_gettext("COPY terminated by new PQexec")) < 0)
        {
          return false;
        }
                                                                
      }
      else
      {
                                                
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("COPY IN state must be terminated first\n"));
        return false;
      }
    }
    else if (resultStatus == PGRES_COPY_OUT)
    {
      if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
      {
           
                                                                      
                                                                       
                                 
           
        conn->asyncStatus = PGASYNC_BUSY;
                                                                   
      }
      else
      {
                                                
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("COPY OUT state must be terminated first\n"));
        return false;
      }
    }
    else if (resultStatus == PGRES_COPY_BOTH)
    {
                                                  
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("PQexec not allowed during COPY BOTH\n"));
      return false;
    }
                                           
    if (conn->status == CONNECTION_BAD)
    {
      return false;
    }
  }

                            
  return true;
}

   
                                                                        
   
static PGresult *
PQexecFinish(PGconn *conn)
{
  PGresult *result;
  PGresult *lastResult;

     
                                                                           
                                                                         
             
     
                                                                         
                                                           
     
                                                                       
     
  lastResult = NULL;
  while ((result = PQgetResult(conn)) != NULL)
  {
    if (lastResult)
    {
      if (lastResult->resultStatus == PGRES_FATAL_ERROR && result->resultStatus == PGRES_FATAL_ERROR)
      {
        pqCatenateResultError(lastResult, result->errMsg);
        PQclear(result);
        result = lastResult;

           
                                                                    
           
        resetPQExpBuffer(&conn->errorMessage);
        appendPQExpBufferStr(&conn->errorMessage, result->errMsg);
      }
      else
      {
        PQclear(lastResult);
      }
    }
    lastResult = result;
    if (result->resultStatus == PGRES_COPY_IN || result->resultStatus == PGRES_COPY_OUT || result->resultStatus == PGRES_COPY_BOTH || conn->status == CONNECTION_BAD)
    {
      break;
    }
  }

  return lastResult;
}

   
                      
                                                              
   
                                                                             
                       
                                                                           
                                                                         
                                                                          
                                                    
                                                                  
                      
   
PGresult *
PQdescribePrepared(PGconn *conn, const char *stmt)
{
  if (!PQexecStart(conn))
  {
    return NULL;
  }
  if (!PQsendDescribe(conn, 'S', stmt))
  {
    return NULL;
  }
  return PQexecFinish(conn);
}

   
                    
                                                          
   
                                                                          
                                                                           
                                                                      
                           
   
PGresult *
PQdescribePortal(PGconn *conn, const char *portal)
{
  if (!PQexecStart(conn))
  {
    return NULL;
  }
  if (!PQsendDescribe(conn, 'P', portal))
  {
    return NULL;
  }
  return PQexecFinish(conn);
}

   
                          
                                                                         
   
                                        
                                            
   
int
PQsendDescribePrepared(PGconn *conn, const char *stmt)
{
  return PQsendDescribe(conn, 'S', stmt);
}

   
                        
                                                                      
   
                                        
                                            
   
int
PQsendDescribePortal(PGconn *conn, const char *portal)
{
  return PQsendDescribe(conn, 'P', portal);
}

   
                  
                                           
   
                                       
                                             
                              
                                          
   
static int
PQsendDescribe(PGconn *conn, char desc_type, const char *desc_target)
{
                                              
  if (!desc_target)
  {
    desc_target = "";
  }

  if (!PQsendQueryStart(conn))
  {
    return 0;
  }

                                             
  if (PG_PROTOCOL_MAJOR(conn->pversion) < 3)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("function requires at least protocol version 3.0\n"));
    return 0;
  }

                                      
  if (pqPutMsgStart('D', false, conn) < 0 || pqPutc(desc_type, conn) < 0 || pqPuts(desc_target, conn) < 0 || pqPutMsgEnd(conn) < 0)
  {
    goto sendFailed;
  }

                                  
  if (pqPutMsgStart('S', false, conn) < 0 || pqPutMsgEnd(conn) < 0)
  {
    goto sendFailed;
  }

                                        
  conn->queryclass = PGQUERY_DESCRIBE;

                                                  
  if (conn->last_query)
  {
    free(conn->last_query);
    conn->last_query = NULL;
  }

     
                                                                             
                                                                           
     
  if (pqFlush(conn) < 0)
  {
    goto sendFailed;
  }

                          
  conn->asyncStatus = PGASYNC_BUSY;
  return 1;

sendFailed:
                                              
  return 0;
}

   
              
                                                                    
                                 
   
                                       
                                                    
   
                                                                 
   
                                                                       
                                                          
   
PGnotify *
PQnotifies(PGconn *conn)
{
  PGnotify *event;

  if (!conn)
  {
    return NULL;
  }

                                                                          
  parseInput(conn);

  event = conn->notifyHead;
  if (event)
  {
    conn->notifyHead = event->next;
    if (!conn->notifyHead)
    {
      conn->notifyTail = NULL;
    }
    event->next = NULL;                                           
  }
  return event;
}

   
                                                                             
   
                                                                       
                                                
   
int
PQputCopyData(PGconn *conn, const char *buffer, int nbytes)
{
  if (!conn)
  {
    return -1;
  }
  if (conn->asyncStatus != PGASYNC_COPY_IN && conn->asyncStatus != PGASYNC_COPY_BOTH)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("no COPY in progress\n"));
    return -1;
  }

     
                                                                        
                                                                            
                                                                     
                                                                          
                                                                          
                                                        
     
  parseInput(conn);

  if (nbytes > 0)
  {
       
                                                                          
                                                                         
                                                                  
                                                                          
              
       
    if ((conn->outBufSize - conn->outCount - 5) < nbytes)
    {
      if (pqFlush(conn) < 0)
      {
        return -1;
      }
      if (pqCheckOutBufferSpace(conn->outCount + 5 + (size_t)nbytes, conn))
      {
        return pqIsnonblocking(conn) ? 0 : -1;
      }
    }
                                                                     
    if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
    {
      if (pqPutMsgStart('d', false, conn) < 0 || pqPutnchar(buffer, nbytes, conn) < 0 || pqPutMsgEnd(conn) < 0)
      {
        return -1;
      }
    }
    else
    {
      if (pqPutMsgStart(0, false, conn) < 0 || pqPutnchar(buffer, nbytes, conn) < 0 || pqPutMsgEnd(conn) < 0)
      {
        return -1;
      }
    }
  }
  return 1;
}

   
                                                                    
   
                                                                             
   
                                                                       
                                                
   
int
PQputCopyEnd(PGconn *conn, const char *errormsg)
{
  if (!conn)
  {
    return -1;
  }
  if (conn->asyncStatus != PGASYNC_COPY_IN && conn->asyncStatus != PGASYNC_COPY_BOTH)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("no COPY in progress\n"));
    return -1;
  }

     
                                                                       
                                                    
     
  if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
  {
    if (errormsg)
    {
                          
      if (pqPutMsgStart('f', false, conn) < 0 || pqPuts(errormsg, conn) < 0 || pqPutMsgEnd(conn) < 0)
      {
        return -1;
      }
    }
    else
    {
                          
      if (pqPutMsgStart('c', false, conn) < 0 || pqPutMsgEnd(conn) < 0)
      {
        return -1;
      }
    }

       
                                                                           
                     
       
    if (conn->queryclass != PGQUERY_SIMPLE)
    {
      if (pqPutMsgStart('S', false, conn) < 0 || pqPutMsgEnd(conn) < 0)
      {
        return -1;
      }
    }
  }
  else
  {
    if (errormsg)
    {
                                          
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("function requires at least protocol version 3.0\n"));
      return -1;
    }
    else
    {
                                             
      if (pqPutMsgStart(0, false, conn) < 0 || pqPutnchar("\\.\n", 3, conn) < 0 || pqPutMsgEnd(conn) < 0)
      {
        return -1;
      }
    }
  }

                             
  if (conn->asyncStatus == PGASYNC_COPY_BOTH)
  {
    conn->asyncStatus = PGASYNC_COPY_OUT;
  }
  else
  {
    conn->asyncStatus = PGASYNC_BUSY;
  }
  resetPQExpBuffer(&conn->errorMessage);

                         
  if (pqFlush(conn) < 0)
  {
    return -1;
  }

  return 1;
}

   
                                                                       
                
   
                                                                       
                                              
                                                                       
                                                                    
                    
   
int
PQgetCopyData(PGconn *conn, char **buffer, int async)
{
  *buffer = NULL;                            
  if (!conn)
  {
    return -2;
  }
  if (conn->asyncStatus != PGASYNC_COPY_OUT && conn->asyncStatus != PGASYNC_COPY_BOTH)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("no COPY in progress\n"));
    return -2;
  }
  if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
  {
    return pqGetCopyData3(conn, buffer, async);
  }
  else
  {
    return pqGetCopyData2(conn, buffer, async);
  }
}

   
                                                                  
   
                                                                    
                                                                       
   
                                                                            
                                                 
   
                                                                        
                                      
   
                                                                           
                                                          
   
            
                                                   
                                                 
                                                           
                                                             
                                            
                                                                        
   
int
PQgetline(PGconn *conn, char *s, int maxlen)
{
  if (!s || maxlen <= 0)
  {
    return EOF;
  }
  *s = '\0';
                                                            
  if (maxlen < 3)
  {
    return EOF;
  }

  if (!conn)
  {
    return EOF;
  }

  if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
  {
    return pqGetline3(conn, s, maxlen);
  }
  else
  {
    return pqGetline2(conn, s, maxlen);
  }
}

   
                                                           
   
                                                                           
                                                                             
                                                                            
                                                                      
                                                                           
   
                                                                        
                                                                         
                                      
   
                                                                              
                                                                          
                                     
   
            
                                                             
                                 
                                        
   
                                                                              
                                                                           
                                                                             
                                                                            
                                                  
   
                                               
   

int
PQgetlineAsync(PGconn *conn, char *buffer, int bufsize)
{
  if (!conn)
  {
    return -1;
  }

  if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
  {
    return pqGetlineAsync3(conn, buffer, bufsize);
  }
  else
  {
    return pqGetlineAsync2(conn, buffer, bufsize);
  }
}

   
                                                              
                                
   
                                                                            
                                                                          
                 
   
int
PQputline(PGconn *conn, const char *s)
{
  return PQputnbytes(conn, s, strlen(s));
}

   
                                                                          
                                
   
int
PQputnbytes(PGconn *conn, const char *buffer, int nbytes)
{
  if (PQputCopyData(conn, buffer, nbytes) > 0)
  {
    return 0;
  }
  else
  {
    return EOF;
  }
}

   
             
                                                                 
                                                                           
   
                                                                               
                                                                           
                                                                            
                                         
   
            
                 
                 
   
int
PQendcopy(PGconn *conn)
{
  if (!conn)
  {
    return 0;
  }

  if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
  {
    return pqEndcopy3(conn);
  }
  else
  {
    return pqEndcopy2(conn);
  }
}

                    
                                                         
   
                                
                                          
                                           
                                                           
                                                                 
                                     
                                                       
                                                           
                                            
   
           
                                                           
                                                              
                                                                          
                                                                     
                    
   

PGresult *
PQfn(PGconn *conn, int fnid, int *result_buf, int *result_len, int result_is_int, const PQArgBlock *args, int nargs)
{
  *result_len = 0;

  if (!conn)
  {
    return NULL;
  }

                              
  resetPQExpBuffer(&conn->errorMessage);

  if (conn->sock == PGINVALID_SOCKET || conn->asyncStatus != PGASYNC_IDLE || conn->result != NULL)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("connection in wrong state\n"));
    return NULL;
  }

  if (PG_PROTOCOL_MAJOR(conn->pversion) >= 3)
  {
    return pqFunctionCall3(conn, fnid, result_buf, result_len, result_is_int, args, nargs);
  }
  else
  {
    return pqFunctionCall2(conn, fnid, result_buf, result_len, result_is_int, args, nargs);
  }
}

                                                 

ExecStatusType
PQresultStatus(const PGresult *res)
{
  if (!res)
  {
    return PGRES_FATAL_ERROR;
  }
  return res->resultStatus;
}

char *
PQresStatus(ExecStatusType status)
{
  if ((unsigned int)status >= sizeof pgresStatus / sizeof pgresStatus[0])
  {
    return libpq_gettext("invalid ExecStatusType code");
  }
  return pgresStatus[status];
}

char *
PQresultErrorMessage(const PGresult *res)
{
  if (!res || !res->errMsg)
  {
    return "";
  }
  return res->errMsg;
}

char *
PQresultVerboseErrorMessage(const PGresult *res, PGVerbosity verbosity, PGContextVisibility show_context)
{
  PQExpBufferData workBuf;

     
                                                                       
                                                                        
                                                  
     
  if (!res || (res->resultStatus != PGRES_FATAL_ERROR && res->resultStatus != PGRES_NONFATAL_ERROR))
  {
    return strdup(libpq_gettext("PGresult is not an error result\n"));
  }

  initPQExpBuffer(&workBuf);

     
                                                                         
                                                                          
                                                                            
                                                                          
                              
     
  pqBuildErrorMessage3(&workBuf, res, verbosity, show_context);

                                                                  
  if (PQExpBufferDataBroken(workBuf))
  {
    termPQExpBuffer(&workBuf);
    return strdup(libpq_gettext("out of memory\n"));
  }

  return workBuf.data;
}

char *
PQresultErrorField(const PGresult *res, int fieldcode)
{
  PGMessageField *pfield;

  if (!res)
  {
    return NULL;
  }
  for (pfield = res->errFields; pfield != NULL; pfield = pfield->next)
  {
    if (pfield->code == fieldcode)
    {
      return pfield->contents;
    }
  }
  return NULL;
}

int
PQntuples(const PGresult *res)
{
  if (!res)
  {
    return 0;
  }
  return res->ntups;
}

int
PQnfields(const PGresult *res)
{
  if (!res)
  {
    return 0;
  }
  return res->numAttributes;
}

int
PQbinaryTuples(const PGresult *res)
{
  if (!res)
  {
    return 0;
  }
  return res->binary;
}

   
                                                                   
                                   
   

static int
check_field_number(const PGresult *res, int field_num)
{
  if (!res)
  {
    return false;                                         
  }
  if (field_num < 0 || field_num >= res->numAttributes)
  {
    pqInternalNotice(&res->noticeHooks, "column number %d is out of range 0..%d", field_num, res->numAttributes - 1);
    return false;
  }
  return true;
}

static int
check_tuple_field_number(const PGresult *res, int tup_num, int field_num)
{
  if (!res)
  {
    return false;                                         
  }
  if (tup_num < 0 || tup_num >= res->ntups)
  {
    pqInternalNotice(&res->noticeHooks, "row number %d is out of range 0..%d", tup_num, res->ntups - 1);
    return false;
  }
  if (field_num < 0 || field_num >= res->numAttributes)
  {
    pqInternalNotice(&res->noticeHooks, "column number %d is out of range 0..%d", field_num, res->numAttributes - 1);
    return false;
  }
  return true;
}

static int
check_param_number(const PGresult *res, int param_num)
{
  if (!res)
  {
    return false;                                         
  }
  if (param_num < 0 || param_num >= res->numParameters)
  {
    pqInternalNotice(&res->noticeHooks, "parameter number %d is out of range 0..%d", param_num, res->numParameters - 1);
    return false;
  }

  return true;
}

   
                                            
   
char *
PQfname(const PGresult *res, int field_num)
{
  if (!check_field_number(res, field_num))
  {
    return NULL;
  }
  if (res->attDescs)
  {
    return res->attDescs[field_num].name;
  }
  else
  {
    return NULL;
  }
}

   
                                                   
   
                                                                         
                                                                          
                                                                       
                                
   
                                                                       
                                                                   
   
int
PQfnumber(const PGresult *res, const char *field_name)
{
  char *field_case;
  bool in_quotes;
  bool all_lower = true;
  const char *iptr;
  char *optr;
  int i;

  if (!res)
  {
    return -1;
  }

     
                                                                          
                                                          
     
  if (field_name == NULL || field_name[0] == '\0' || res->attDescs == NULL)
  {
    return -1;
  }

     
                                                                     
                                                                         
     
  for (iptr = field_name; *iptr; iptr++)
  {
    char c = *iptr;

    if (c == '"' || c != pg_tolower((unsigned char)c))
    {
      all_lower = false;
      break;
    }
  }

  if (all_lower)
  {
    for (i = 0; i < res->numAttributes; i++)
    {
      if (strcmp(field_name, res->attDescs[i].name) == 0)
      {
        return i;
      }
    }
  }

                                                                 

     
                                                                  
                                                                             
                
     
  field_case = strdup(field_name);
  if (field_case == NULL)
  {
    return -1;             
  }

  in_quotes = false;
  optr = field_case;
  for (iptr = field_case; *iptr; iptr++)
  {
    char c = *iptr;

    if (in_quotes)
    {
      if (c == '"')
      {
        if (iptr[1] == '"')
        {
                                                    
          *optr++ = '"';
          iptr++;
        }
        else
        {
          in_quotes = false;
        }
      }
      else
      {
        *optr++ = c;
      }
    }
    else if (c == '"')
    {
      in_quotes = true;
    }
    else
    {
      c = pg_tolower((unsigned char)c);
      *optr++ = c;
    }
  }
  *optr = '\0';

  for (i = 0; i < res->numAttributes; i++)
  {
    if (strcmp(field_case, res->attDescs[i].name) == 0)
    {
      free(field_case);
      return i;
    }
  }
  free(field_case);
  return -1;
}

Oid
PQftable(const PGresult *res, int field_num)
{
  if (!check_field_number(res, field_num))
  {
    return InvalidOid;
  }
  if (res->attDescs)
  {
    return res->attDescs[field_num].tableid;
  }
  else
  {
    return InvalidOid;
  }
}

int
PQftablecol(const PGresult *res, int field_num)
{
  if (!check_field_number(res, field_num))
  {
    return 0;
  }
  if (res->attDescs)
  {
    return res->attDescs[field_num].columnid;
  }
  else
  {
    return 0;
  }
}

int
PQfformat(const PGresult *res, int field_num)
{
  if (!check_field_number(res, field_num))
  {
    return 0;
  }
  if (res->attDescs)
  {
    return res->attDescs[field_num].format;
  }
  else
  {
    return 0;
  }
}

Oid
PQftype(const PGresult *res, int field_num)
{
  if (!check_field_number(res, field_num))
  {
    return InvalidOid;
  }
  if (res->attDescs)
  {
    return res->attDescs[field_num].typid;
  }
  else
  {
    return InvalidOid;
  }
}

int
PQfsize(const PGresult *res, int field_num)
{
  if (!check_field_number(res, field_num))
  {
    return 0;
  }
  if (res->attDescs)
  {
    return res->attDescs[field_num].typlen;
  }
  else
  {
    return 0;
  }
}

int
PQfmod(const PGresult *res, int field_num)
{
  if (!check_field_number(res, field_num))
  {
    return 0;
  }
  if (res->attDescs)
  {
    return res->attDescs[field_num].atttypmod;
  }
  else
  {
    return 0;
  }
}

char *
PQcmdStatus(PGresult *res)
{
  if (!res)
  {
    return NULL;
  }
  return res->cmdStatus;
}

   
                 
                                                            
                     
   
char *
PQoidStatus(const PGresult *res)
{
     
                                                                         
                                         
     
  static char buf[24];

  size_t len;

  if (!res || strncmp(res->cmdStatus, "INSERT ", 7) != 0)
  {
    return "";
  }

  len = strspn(res->cmdStatus + 7, "0123456789");
  if (len > sizeof(buf) - 1)
  {
    len = sizeof(buf) - 1;
  }
  memcpy(buf, res->cmdStatus + 7, len);
  buf[len] = '\0';

  return buf;
}

   
                
                                                             
               
   
Oid
PQoidValue(const PGresult *res)
{
  char *endptr = NULL;
  unsigned long result;

  if (!res || strncmp(res->cmdStatus, "INSERT ", 7) != 0 || res->cmdStatus[7] < '0' || res->cmdStatus[7] > '9')
  {
    return InvalidOid;
  }

  result = strtoul(res->cmdStatus + 7, &endptr, 10);

  if (!endptr || (*endptr != ' ' && *endptr != '\0'))
  {
    return InvalidOid;
  }
  else
  {
    return (Oid)result;
  }
}

   
                 
                                                                        
                                                                       
              
   
                                           
   
char *
PQcmdTuples(PGresult *res)
{
  char *p, *c;

  if (!res)
  {
    return "";
  }

  if (strncmp(res->cmdStatus, "INSERT ", 7) == 0)
  {
    p = res->cmdStatus + 7;
                                    
    while (*p && *p != ' ')
    {
      p++;
    }
    if (*p == 0)
    {
      goto interpret_error;                
    }
    p++;
  }
  else if (strncmp(res->cmdStatus, "SELECT ", 7) == 0 || strncmp(res->cmdStatus, "DELETE ", 7) == 0 || strncmp(res->cmdStatus, "UPDATE ", 7) == 0)
  {
    p = res->cmdStatus + 7;
  }
  else if (strncmp(res->cmdStatus, "FETCH ", 6) == 0)
  {
    p = res->cmdStatus + 6;
  }
  else if (strncmp(res->cmdStatus, "MOVE ", 5) == 0 || strncmp(res->cmdStatus, "COPY ", 5) == 0)
  {
    p = res->cmdStatus + 5;
  }
  else
  {
    return "";
  }

                                                                        
  for (c = p; *c; c++)
  {
    if (!isdigit((unsigned char)*c))
    {
      goto interpret_error;
    }
  }
  if (c == p)
  {
    goto interpret_error;
  }

  return p;

interpret_error:
  pqInternalNotice(&res->noticeHooks, "could not interpret result from server: %s", res->cmdStatus);
  return "";
}

   
               
                                                          
   
char *
PQgetvalue(const PGresult *res, int tup_num, int field_num)
{
  if (!check_tuple_field_number(res, tup_num, field_num))
  {
    return NULL;
  }
  return res->tuples[tup_num][field_num].value;
}

                
                                                        
   
int
PQgetlength(const PGresult *res, int tup_num, int field_num)
{
  if (!check_tuple_field_number(res, tup_num, field_num))
  {
    return 0;
  }
  if (res->tuples[tup_num][field_num].len != NULL_LEN)
  {
    return res->tuples[tup_num][field_num].len;
  }
  else
  {
    return 0;
  }
}

                
                                             
   
int
PQgetisnull(const PGresult *res, int tup_num, int field_num)
{
  if (!check_tuple_field_number(res, tup_num, field_num))
  {
    return 1;                         
  }
  if (res->tuples[tup_num][field_num].len == NULL_LEN)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}

              
                                                                   
   
int
PQnparams(const PGresult *res)
{
  if (!res)
  {
    return 0;
  }
  return res->numParameters;
}

                
                                                          
   
Oid
PQparamtype(const PGresult *res, int param_num)
{
  if (!check_param_number(res, param_num))
  {
    return InvalidOid;
  }
  if (res->paramDescs)
  {
    return res->paramDescs[param_num].typid;
  }
  else
  {
    return InvalidOid;
  }
}

                     
                                                                         
                                                                   
                                                                           
                                                               
   
int
PQsetnonblocking(PGconn *conn, int arg)
{
  bool barg;

  if (!conn || conn->status == CONNECTION_BAD)
  {
    return -1;
  }

  barg = (arg ? true : false);

                                                                 
  if (barg == conn->nonblocking)
  {
    return 0;
  }

     
                                                                          
                                                                             
                                                                             
                                                          
     
                                                                
  if (pqFlush(conn))
  {
    return -1;
  }

  conn->nonblocking = barg;

  return 0;
}

   
                                                         
                                           
   
int
PQisnonblocking(const PGconn *conn)
{
  if (!conn || conn->status == CONNECTION_BAD)
  {
    return false;
  }
  return pqIsnonblocking(conn);
}

                           
int
PQisthreadsafe(void)
{
#ifdef ENABLE_THREAD_SAFETY
  return true;
#else
  return false;
#endif
}

                                                                      
int
PQflush(PGconn *conn)
{
  if (!conn || conn->status == CONNECTION_BAD)
  {
    return -1;
  }
  return pqFlush(conn);
}

   
                                              
   
                                                                 
                                                                  
   
void
PQfreemem(void *ptr)
{
  free(ptr);
}

   
                                                               
   
                                                                 
                                                                    
                                                                            
   

#undef PQfreeNotify
void
PQfreeNotify(PGnotify *notify);

void
PQfreeNotify(PGnotify *notify)
{
  PQfreemem(notify);
}

   
                                                                
   
                                                                           
   
                                                                           
                                                                               
                                 
   
                                                                           
                                                                          
                                   
   
                                                                               
   
static size_t
PQescapeStringInternal(PGconn *conn, char *to, const char *from, size_t length, int *error, int encoding, bool std_strings)
{
  const char *source = from;
  char *target = to;
  size_t remaining = length;

  if (error)
  {
    *error = 0;
  }

  while (remaining > 0 && *source != '\0')
  {
    char c = *source;
    int len;
    int i;

                                   
    if (!IS_HIGHBIT_SET(c))
    {
                                   
      if (SQL_STR_DOUBLE(c, !std_strings))
      {
        *target++ = c;
      }
                              
      *target++ = c;
      source++;
      remaining--;
      continue;
    }

                                                     
    len = pg_encoding_mblen(encoding, source);

                            
    for (i = 0; i < len; i++)
    {
      if (remaining == 0 || *source == '\0')
      {
        break;
      }
      *target++ = *source++;
      remaining--;
    }

       
                                                                   
                                                                        
                                                                        
                                                                   
                                                                          
                                     
       
    if (i < len)
    {
      if (error)
      {
        *error = 1;
      }
      if (conn)
      {
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("incomplete multibyte character\n"));
      }
      for (; i < len; i++)
      {
        if (((size_t)(target - to)) / 2 >= length)
        {
          break;
        }
        *target++ = ' ';
      }
      break;
    }
  }

                                            
  *target = '\0';

  return target - to;
}

size_t
PQescapeStringConn(PGconn *conn, char *to, const char *from, size_t length, int *error)
{
  if (!conn)
  {
                                   
    *to = '\0';
    if (error)
    {
      *error = 1;
    }
    return 0;
  }
  return PQescapeStringInternal(conn, to, from, length, error, conn->client_encoding, conn->std_strings);
}

size_t
PQescapeString(char *to, const char *from, size_t length)
{
  return PQescapeStringInternal(NULL, to, from, length, NULL, static_client_encoding, static_std_strings);
}

   
                                                                        
                                                                        
                                                                             
                                                                            
   
static char *
PQescapeInternal(PGconn *conn, const char *str, size_t len, bool as_ident)
{
  const char *s;
  char *result;
  char *rp;
  int num_quotes = 0;                                              
  int num_backslashes = 0;
  int input_len;
  int result_size;
  char quote_char = as_ident ? '"' : '\'';

                                                         
  if (!conn)
  {
    return NULL;
  }

                                                            
  for (s = str; (s - str) < len && *s != '\0'; ++s)
  {
    if (*s == quote_char)
    {
      ++num_quotes;
    }
    else if (*s == '\\')
    {
      ++num_backslashes;
    }
    else if (IS_HIGHBIT_SET(*s))
    {
      int charlen;

                                                       
      charlen = pg_encoding_mblen(conn->client_encoding, s);

                                                          
      if ((s - str) + charlen > len || memchr(s, 0, charlen) != NULL)
      {
        printfPQExpBuffer(&conn->errorMessage, libpq_gettext("incomplete multibyte character\n"));
        return NULL;
      }

                                                                      
      s += charlen - 1;
    }
  }

                               
  input_len = s - str;
  result_size = input_len + num_quotes + 3;                             
  if (!as_ident && num_backslashes > 0)
  {
    result_size += num_backslashes + 2;
  }
  result = rp = (char *)malloc(result_size);
  if (rp == NULL)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
    return NULL;
  }

     
                                                                        
                                                                           
                                                                           
                                                                     
                                                       
     
  if (!as_ident && num_backslashes > 0)
  {
    *rp++ = ' ';
    *rp++ = 'E';
  }

                      
  *rp++ = quote_char;

     
                                
     
                                                                        
                                                                     
                                                                            
                                                     
     
                                                                 
                   
     
  if (num_quotes == 0 && (num_backslashes == 0 || as_ident))
  {
    memcpy(rp, str, input_len);
    rp += input_len;
  }
  else
  {
    for (s = str; s - str < input_len; ++s)
    {
      if (*s == quote_char || (!as_ident && *s == '\\'))
      {
        *rp++ = *s;
        *rp++ = *s;
      }
      else if (!IS_HIGHBIT_SET(*s))
      {
        *rp++ = *s;
      }
      else
      {
        int i = pg_encoding_mblen(conn->client_encoding, s);

        while (1)
        {
          *rp++ = *s;
          if (--i == 0)
          {
            break;
          }
          ++s;                                                
        }
      }
    }
  }

                                          
  *rp++ = quote_char;
  *rp = '\0';

  return result;
}

char *
PQescapeLiteral(PGconn *conn, const char *str, size_t len)
{
  return PQescapeInternal(conn, str, len, false);
}

char *
PQescapeIdentifier(PGconn *conn, const char *str, size_t len)
{
  return PQescapeInternal(conn, str, len, true);
}

                                    
static const char hextbl[] = "0123456789abcdef";

static const int8 hexlookup[128] = {
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    10,
    11,
    12,
    13,
    14,
    15,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    10,
    11,
    12,
    13,
    14,
    15,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
};

static inline char
get_hex(char c)
{
  int res = -1;

  if (c > 0 && c < 127)
  {
    res = hexlookup[(unsigned char)c];
  }

  return (char)res;
}

   
                                                       
                                                               
                                                             
   
                                                            
                                                               
                             
                           
                           
                                         
                                               
   
                                                                        
   
static unsigned char *
PQescapeByteaInternal(PGconn *conn, const unsigned char *from, size_t from_length, size_t *to_length, bool std_strings, bool use_hex)
{
  const unsigned char *vp;
  unsigned char *rp;
  unsigned char *result;
  size_t i;
  size_t len;
  size_t bslash_len = (std_strings ? 1 : 2);

     
                                    
     
  len = 1;

  if (use_hex)
  {
    len += bslash_len + 1 + 2 * from_length;
  }
  else
  {
    vp = from;
    for (i = from_length; i > 0; i--, vp++)
    {
      if (*vp < 0x20 || *vp > 0x7e)
      {
        len += bslash_len + 3;
      }
      else if (*vp == '\'')
      {
        len += 2;
      }
      else if (*vp == '\\')
      {
        len += bslash_len + bslash_len;
      }
      else
      {
        len++;
      }
    }
  }

  *to_length = len;
  rp = result = (unsigned char *)malloc(len);
  if (rp == NULL)
  {
    if (conn)
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
    }
    return NULL;
  }

  if (use_hex)
  {
    if (!std_strings)
    {
      *rp++ = '\\';
    }
    *rp++ = '\\';
    *rp++ = 'x';
  }

  vp = from;
  for (i = from_length; i > 0; i--, vp++)
  {
    unsigned char c = *vp;

    if (use_hex)
    {
      *rp++ = hextbl[(c >> 4) & 0xF];
      *rp++ = hextbl[c & 0xF];
    }
    else if (c < 0x20 || c > 0x7e)
    {
      if (!std_strings)
      {
        *rp++ = '\\';
      }
      *rp++ = '\\';
      *rp++ = (c >> 6) + '0';
      *rp++ = ((c >> 3) & 07) + '0';
      *rp++ = (c & 07) + '0';
    }
    else if (c == '\'')
    {
      *rp++ = '\'';
      *rp++ = '\'';
    }
    else if (c == '\\')
    {
      if (!std_strings)
      {
        *rp++ = '\\';
        *rp++ = '\\';
      }
      *rp++ = '\\';
      *rp++ = '\\';
    }
    else
    {
      *rp++ = c;
    }
  }
  *rp = '\0';

  return result;
}

unsigned char *
PQescapeByteaConn(PGconn *conn, const unsigned char *from, size_t from_length, size_t *to_length)
{
  if (!conn)
  {
    return NULL;
  }
  return PQescapeByteaInternal(conn, from, from_length, to_length, conn->std_strings, (conn->sversion >= 90000));
}

unsigned char *
PQescapeBytea(const unsigned char *from, size_t from_length, size_t *to_length)
{
  return PQescapeByteaInternal(NULL, from, from_length, to_length, static_std_strings, false                    );
}

#define ISFIRSTOCTDIGIT(CH) ((CH) >= '0' && (CH) <= '3')
#define ISOCTDIGIT(CH) ((CH) >= '0' && (CH) <= '7')
#define OCTVAL(CH) ((CH) - '0')

   
                                                                         
                                                                     
                                                                  
                                                                    
                                        
   
                                            
                         
                                                              
                                                                           
   
unsigned char *
PQunescapeBytea(const unsigned char *strtext, size_t *retbuflen)
{
  size_t strtextlen, buflen;
  unsigned char *buffer, *tmpbuf;
  size_t i, j;

  if (strtext == NULL)
  {
    return NULL;
  }

  strtextlen = strlen((const char *)strtext);

  if (strtext[0] == '\\' && strtext[1] == 'x')
  {
    const unsigned char *s;
    unsigned char *p;

    buflen = (strtextlen - 2) / 2;
                                    
    buffer = (unsigned char *)malloc(buflen > 0 ? buflen : 1);
    if (buffer == NULL)
    {
      return NULL;
    }

    s = strtext + 2;
    p = buffer;
    while (*s)
    {
      char v1, v2;

         
                                                                 
                                                                    
         
      v1 = get_hex(*s++);
      if (!*s || v1 == (char)-1)
      {
        continue;
      }
      v2 = get_hex(*s++);
      if (v2 != (char)-1)
      {
        *p++ = (v1 << 4) | v2;
      }
    }

    buflen = p - buffer;
  }
  else
  {
       
                                                                     
                                                     
       
    buffer = (unsigned char *)malloc(strtextlen + 1);
    if (buffer == NULL)
    {
      return NULL;
    }

    for (i = j = 0; i < strtextlen;)
    {
      switch (strtext[i])
      {
      case '\\':
        i++;
        if (strtext[i] == '\\')
        {
          buffer[j++] = strtext[i++];
        }
        else
        {
          if ((ISFIRSTOCTDIGIT(strtext[i])) && (ISOCTDIGIT(strtext[i + 1])) && (ISOCTDIGIT(strtext[i + 2])))
          {
            int byte;

            byte = OCTVAL(strtext[i++]);
            byte = (byte << 3) + OCTVAL(strtext[i++]);
            byte = (byte << 3) + OCTVAL(strtext[i++]);
            buffer[j++] = byte;
          }
        }

           
                                                                  
                                                                  
                                                                   
                                                                 
                                                              
           
        break;

      default:
        buffer[j++] = strtext[i++];
        break;
      }
    }
    buflen = j;                                                
  }

                                                        
                                                    
  tmpbuf = realloc(buffer, buflen + 1);

                                                                          
  if (!tmpbuf)
  {
    free(buffer);
    return NULL;
  }

  *retbuflen = buflen;
  return tmpbuf;
}
