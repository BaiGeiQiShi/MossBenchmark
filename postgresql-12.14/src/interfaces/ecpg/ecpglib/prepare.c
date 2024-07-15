                                           

#define POSTGRES_ECPG_INTERNAL
#include "postgres_fe.h"

#include <ctype.h>

#include "ecpgtype.h"
#include "ecpglib.h"
#include "ecpgerrno.h"
#include "ecpglib_extern.h"
#include "sqlca.h"

#define STMTID_SIZE 32

   
                                                                     
                                                                     
                                                     
                                                                        
              
   
#define stmtCacheNBuckets 2039                               
#define stmtCacheEntPerBucket 8

#define stmtCacheArraySize (stmtCacheNBuckets * stmtCacheEntPerBucket + 1)

typedef struct
{
  int lineno;
  char stmtID[STMTID_SIZE];
  char *ecpgQuery;
  long execs;                                  
  const char *connection;                                   
} stmtCacheEntry;

static int nextStmtID = 1;
static stmtCacheEntry *stmtCacheEntries = NULL;

static bool
deallocate_one(int lineno, enum COMPAT_MODE c, struct connection *con, struct prepared_statement *prev, struct prepared_statement *this);

static bool
isvarchar(unsigned char c)
{
  if (isalnum(c))
  {
    return true;
  }

  if (c == '_' || c == '>' || c == '-' || c == '.')
  {
    return true;
  }

  if (c >= 128)
  {
    return true;
  }

  return false;
}

bool
ecpg_register_prepared_stmt(struct statement *stmt)
{
  struct statement *prep_stmt;
  struct prepared_statement *this;
  struct connection *con = stmt->connection;
  struct prepared_statement *prev = NULL;
  int lineno = stmt->lineno;

                                                        
  this = ecpg_find_prepared_statement(stmt->name, con, &prev);
  if (this && !deallocate_one(lineno, ECPG_COMPAT_PGSQL, con, prev, this))
  {
    return false;
  }

                              
  this = (struct prepared_statement *)ecpg_alloc(sizeof(struct prepared_statement), lineno);
  if (!this)
  {
    return false;
  }

  prep_stmt = (struct statement *)ecpg_alloc(sizeof(struct statement), lineno);
  if (!prep_stmt)
  {
    ecpg_free(this);
    return false;
  }
  memset(prep_stmt, 0, sizeof(struct statement));

                        
  prep_stmt->lineno = lineno;
  prep_stmt->connection = con;
  prep_stmt->command = ecpg_strdup(stmt->command, lineno);
  prep_stmt->inlist = prep_stmt->outlist = NULL;
  this->name = ecpg_strdup(stmt->name, lineno);
  this->stmt = prep_stmt;
  this->prepared = true;

  if (con->prep_stmts == NULL)
  {
    this->next = NULL;
  }
  else
  {
    this->next = con->prep_stmts;
  }

  con->prep_stmts = this;
  return true;
}

static bool
replace_variables(char **text, int lineno)
{
  bool string = false;
  int counter = 1, ptr = 0;

  for (; (*text)[ptr] != '\0'; ptr++)
  {
    if ((*text)[ptr] == '\'')
    {
      string = string ? false : true;
    }

    if (string || (((*text)[ptr] != ':') && ((*text)[ptr] != '?')))
    {
      continue;
    }

    if (((*text)[ptr] == ':') && ((*text)[ptr + 1] == ':'))
    {
      ptr += 2;                 
    }
    else
    {
                                              
      int buffersize = sizeof(int) * CHAR_BIT * 10 / 3;
      int len;
      char *buffer, *newcopy;

      if (!(buffer = (char *)ecpg_alloc(buffersize, lineno)))
      {
        return false;
      }

      snprintf(buffer, buffersize, "$%d", counter++);

      for (len = 1; (*text)[ptr + len] && isvarchar((*text)[ptr + len]); len++)
                  ;
      if (!(newcopy = (char *)ecpg_alloc(strlen(*text) - len + strlen(buffer) + 1, lineno)))
      {
        ecpg_free(buffer);
        return false;
      }

      memcpy(newcopy, *text, ptr);
      strcpy(newcopy + ptr, buffer);
      strcat(newcopy, (*text) + ptr + len);

      ecpg_free(*text);
      ecpg_free(buffer);

      *text = newcopy;

      if ((*text)[ptr] == '\0')                         
      {
        ptr--;                                            
                                   
      }
    }
  }
  return true;
}

static bool
prepare_common(int lineno, struct connection *con, const char *name, const char *variable)
{
  struct statement *stmt;
  struct prepared_statement *this;
  PGresult *query;

                              
  this = (struct prepared_statement *)ecpg_alloc(sizeof(struct prepared_statement), lineno);
  if (!this)
  {
    return false;
  }

  stmt = (struct statement *)ecpg_alloc(sizeof(struct statement), lineno);
  if (!stmt)
  {
    ecpg_free(this);
    return false;
  }

                        
  stmt->lineno = lineno;
  stmt->connection = con;
  stmt->command = ecpg_strdup(variable, lineno);
  stmt->inlist = stmt->outlist = NULL;

                                                                     
  replace_variables(&(stmt->command), lineno);

                                          
  this->name = ecpg_strdup(name, lineno);
  this->stmt = stmt;

                                                
  query = PQprepare(stmt->connection->connection, name, stmt->command, 0, NULL);
  if (!ecpg_check_PQresult(query, stmt->lineno, stmt->connection->connection, stmt->compat))
  {
    ecpg_free(stmt->command);
    ecpg_free(this->name);
    ecpg_free(this);
    ecpg_free(stmt);
    return false;
  }

  ecpg_log("prepare_common on line %d: name %s; query: \"%s\"\n", stmt->lineno, name, stmt->command);
  PQclear(query);
  this->prepared = true;

  if (con->prep_stmts == NULL)
  {
    this->next = NULL;
  }
  else
  {
    this->next = con->prep_stmts;
  }

  con->prep_stmts = this;
  return true;
}

                                           
                                                                                               
bool
ECPGprepare(int lineno, const char *connection_name, const bool questionmarks, const char *name, const char *variable)
{
  struct connection *con;
  struct prepared_statement *this, *prev;

  (void)questionmarks;                         

  con = ecpg_get_connection(connection_name);
  if (!ecpg_init(con, connection_name, lineno))
  {
    return false;
  }

                                                        
  this = ecpg_find_prepared_statement(name, con, &prev);
  if (this && !deallocate_one(lineno, ECPG_COMPAT_PGSQL, con, prev, this))
  {
    return false;
  }

  return prepare_common(lineno, con, name, variable);
}

struct prepared_statement *
ecpg_find_prepared_statement(const char *name, struct connection *con, struct prepared_statement **prev_)
{
  struct prepared_statement *this, *prev;

  for (this = con->prep_stmts, prev = NULL; this != NULL; prev = this, this = this->next)
  {
    if (strcmp(this->name, name) == 0)
    {
      if (prev_)
      {
        *prev_ = prev;
      }
      return this;
    }
  }
  return NULL;
}

static bool
deallocate_one(int lineno, enum COMPAT_MODE c, struct connection *con, struct prepared_statement *prev, struct prepared_statement *this)
{
  bool r = false;

  ecpg_log("deallocate_one on line %d: name %s\n", lineno, this->name);

                                                     
  if (this->prepared)
  {
    char *text;
    PGresult *query;

    text = (char *)ecpg_alloc(strlen("deallocate \"\" ") + strlen(this->name), this->stmt->lineno);

    if (text)
    {
      sprintf(text, "deallocate \"%s\"", this->name);
      query = PQexec(this->stmt->connection->connection, text);
      ecpg_free(text);
      if (ecpg_check_PQresult(query, lineno, this->stmt->connection->connection, this->stmt->compat))
      {
        PQclear(query);
        r = true;
      }
    }
  }

     
                                                                            
                                                     
     
  if (!r && !INFORMIX_MODE(c))
  {
    ecpg_raise(lineno, ECPG_INVALID_STMT, ECPG_SQLSTATE_INVALID_SQL_STATEMENT_NAME, this->name);
    return false;
  }

                                    
  ecpg_free(this->stmt->command);
  ecpg_free(this->stmt);
  ecpg_free(this->name);
  if (prev != NULL)
  {
    prev->next = this->next;
  }
  else
  {
    con->prep_stmts = this->next;
  }

  ecpg_free(this);
  return true;
}

                                                      
bool
ECPGdeallocate(int lineno, int c, const char *connection_name, const char *name)
{
  struct connection *con;
  struct prepared_statement *this, *prev;

  con = ecpg_get_connection(connection_name);
  if (!ecpg_init(con, connection_name, lineno))
  {
    return false;
  }

  this = ecpg_find_prepared_statement(name, con, &prev);
  if (this)
  {
    return deallocate_one(lineno, c, con, prev, this);
  }

                                       
  if (INFORMIX_MODE(c))
  {
    return true;
  }
  ecpg_raise(lineno, ECPG_INVALID_STMT, ECPG_SQLSTATE_INVALID_SQL_STATEMENT_NAME, name);
  return false;
}

bool
ecpg_deallocate_all_conn(int lineno, enum COMPAT_MODE c, struct connection *con)
{
                                          
  while (con->prep_stmts)
  {
    if (!deallocate_one(lineno, c, con, NULL, con->prep_stmts))
    {
      return false;
    }
  }

  return true;
}

bool
ECPGdeallocate_all(int lineno, int compat, const char *connection_name)
{
  return ecpg_deallocate_all_conn(lineno, compat, ecpg_get_connection(connection_name));
}

char *
ecpg_prepared(const char *name, struct connection *con)
{
  struct prepared_statement *this;

  this = ecpg_find_prepared_statement(name, con, NULL);
  return this ? this->stmt->command : NULL;
}

                                   
                                                           
char *
ECPGprepared_statement(const char *connection_name, const char *name, int lineno)
{
  (void)lineno;                              

  return ecpg_prepared(name, ecpg_get_connection(connection_name));
}

   
                                                                        
   
static int
HashStmt(const char *ecpgQuery)
{
  int stmtIx, bucketNo, hashLeng, stmtLeng;
  uint64 hashVal, rotVal;

  stmtLeng = strlen(ecpgQuery);
  hashLeng = 50;                                                   
  if (hashLeng > stmtLeng)                                       
  {
    hashLeng = stmtLeng;                            
  }

  hashVal = 0;
  for (stmtIx = 0; stmtIx < hashLeng; ++stmtIx)
  {
    hashVal = hashVal + (unsigned char)ecpgQuery[stmtIx];
                                               
    hashVal = hashVal << 13;
    rotVal = (hashVal & UINT64CONST(0x1fff00000000)) >> 32;
    hashVal = (hashVal & UINT64CONST(0xffffffff)) | rotVal;
  }

  bucketNo = hashVal % stmtCacheNBuckets;

                                                 
  return bucketNo * stmtCacheEntPerBucket + 1;
}

   
                                                                                 
                                     
                                                       
   
static int
SearchStmtCache(const char *ecpgQuery)
{
  int entNo, entIx;

                                         
  if (stmtCacheEntries == NULL)
  {
    return 0;
  }

                          
  entNo = HashStmt(ecpgQuery);

                        
  for (entIx = 0; entIx < stmtCacheEntPerBucket; ++entIx)
  {
    if (stmtCacheEntries[entNo].stmtID[0])                               
    {
      if (strcmp(ecpgQuery, stmtCacheEntries[entNo].ecpgQuery) == 0)
      {
        break;               
      }
    }
    ++entNo;                   
  }

                                                   
  if (entIx >= stmtCacheEntPerBucket)
  {
    entNo = 0;
  }

  return entNo;
}

   
                                        
                                 
                            
   
static int
ecpg_freeStmtCacheEntry(int lineno, int compat, int entNo)                      
{
  stmtCacheEntry *entry;
  struct connection *con;
  struct prepared_statement *this, *prev;

                                  
  if (stmtCacheEntries == NULL)
  {
    return -1;
  }

  entry = &stmtCacheEntries[entNo];
  if (!entry->stmtID[0])                                       
  {
    return 0;
  }

  con = ecpg_get_connection(entry->connection);

                                                
  this = ecpg_find_prepared_statement(entry->stmtID, con, &prev);
  if (this && !deallocate_one(lineno, compat, con, prev, this))
  {
    return -1;
  }

  entry->stmtID[0] = '\0';

                                               
  if (entry->ecpgQuery)
  {
    ecpg_free(entry->ecpgQuery);
    entry->ecpgQuery = 0;
  }

  return entNo;
}

   
                                       
                                                          
   
static int
AddStmtToCache(int lineno,                           
    const char *stmtID,                       
    const char *connection,                 
    int compat,                                      
    const char *ecpgQuery)             
{
  int ix, initEntNo, luEntNo, entNo;
  stmtCacheEntry *entry;

                                                           
  if (stmtCacheEntries == NULL)
  {
    stmtCacheEntries = (stmtCacheEntry *)ecpg_alloc(sizeof(stmtCacheEntry) * stmtCacheArraySize, lineno);
    if (stmtCacheEntries == NULL)
    {
      return -1;
    }
  }

                          
  initEntNo = HashStmt(ecpgQuery);

                                  
  entNo = initEntNo;                                             
                                   
  luEntNo = initEntNo;                                               
  for (ix = 0; ix < stmtCacheEntPerBucket; ++ix)
  {
    entry = &stmtCacheEntries[entNo];
    if (!entry->stmtID[0])                             
    {
      break;
    }
    if (entry->execs < stmtCacheEntries[luEntNo].execs)
    {
      luEntNo = entNo;                                  
    }
    ++entNo;                        
  }

     
                                                                             
                
     
  if (ix >= stmtCacheEntPerBucket)
  {
    entNo = luEntNo;
  }

                                                        
  if (ecpg_freeStmtCacheEntry(lineno, compat, entNo) < 0)
  {
    return -1;
  }

                                  
  entry = &stmtCacheEntries[entNo];
  entry->lineno = lineno;
  entry->ecpgQuery = ecpg_strdup(ecpgQuery, lineno);
  entry->connection = connection;
  entry->execs = 0;
  memcpy(entry->stmtID, stmtID, sizeof(entry->stmtID));

  return entNo;
}

                                                                     
bool
ecpg_auto_prepare(int lineno, const char *connection_name, const int compat, char **name, const char *query)
{
  int entNo;

                                                     
  entNo = SearchStmtCache(query);

                                                     
  if (entNo)
  {
    char *stmtID;
    struct connection *con;
    struct prepared_statement *prep;

    ecpg_log("ecpg_auto_prepare on line %d: statement found in cache; entry %d\n", lineno, entNo);

    stmtID = stmtCacheEntries[entNo].stmtID;

    con = ecpg_get_connection(connection_name);
    prep = ecpg_find_prepared_statement(stmtID, con, NULL);
                                                              
    if (!prep && !prepare_common(lineno, con, stmtID, query))
    {
      return false;
    }

    *name = ecpg_strdup(stmtID, lineno);
  }
  else
  {
    char stmtID[STMTID_SIZE];

    ecpg_log("ecpg_auto_prepare on line %d: statement not in cache; inserting\n", lineno);

                                 
    sprintf(stmtID, "ecpg%d", nextStmtID++);

    if (!ECPGprepare(lineno, connection_name, 0, stmtID, query))
    {
      return false;
    }

    entNo = AddStmtToCache(lineno, stmtID, connection_name, compat, query);
    if (entNo < 0)
    {
      return false;
    }

    *name = ecpg_strdup(stmtID, lineno);
  }

                              
  stmtCacheEntries[entNo].execs++;

  return true;
}
