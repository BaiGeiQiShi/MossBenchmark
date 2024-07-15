                                                                            
   
                 
                                                        
   
                                                                
   
                                                                            
   
#include "postgres_fe.h"

#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include "pg_rewind.h"
#include "datapagemap.h"
#include "fetch.h"
#include "file_ops.h"
#include "filemap.h"

#include "libpq-fe.h"
#include "catalog/pg_type_d.h"
#include "fe_utils/connect.h"
#include "port/pg_bswap.h"

static PGconn *conn = NULL;

   
                                                    
   
                                                                          
                                                                              
                                                                  
   
#define CHUNKSIZE 1000000

static void
receiveFileChunks(const char *sql);
static void
execute_pagemap(datapagemap_t *pagemap, const char *path);
static char *
run_simple_query(const char *sql);
static void
run_simple_command(const char *sql);

void
libpqConnect(const char *connstr)
{
  char *str;
  PGresult *res;

  conn = PQconnectdb(connstr);
  if (PQstatus(conn) == CONNECTION_BAD)
  {
    pg_fatal("%s", PQerrorMessage(conn));
  }

  if (showprogress)
  {
    pg_log_info("connected to server");
  }

                                     
  run_simple_command("SET statement_timeout = 0");
  run_simple_command("SET lock_timeout = 0");
  run_simple_command("SET idle_in_transaction_session_timeout = 0");

  res = PQexec(conn, ALWAYS_SECURE_SEARCH_PATH_SQL);
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    pg_fatal("could not clear search_path: %s", PQresultErrorMessage(res));
  }
  PQclear(res);

     
                                                                   
                                                                      
                                                                        
                                                            
     
  str = run_simple_query("SELECT pg_is_in_recovery()");
  if (strcmp(str, "f") != 0)
  {
    pg_fatal("source server must not be in recovery mode");
  }
  pg_free(str);

     
                                                                            
                                                                            
                                           
     
  str = run_simple_query("SHOW full_page_writes");
  if (strcmp(str, "on") != 0)
  {
    pg_fatal("full_page_writes must be enabled in the source server");
  }
  pg_free(str);

     
                                                                          
                                                                        
                                                                   
                                                                          
                                                               
     
  run_simple_command("SET synchronous_commit = off");
}

   
                                             
                                             
   
static char *
run_simple_query(const char *sql)
{
  PGresult *res;
  char *result;

  res = PQexec(conn, sql);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    pg_fatal("error running query (%s) in source server: %s", sql, PQresultErrorMessage(res));
  }

                                   
  if (PQnfields(res) != 1 || PQntuples(res) != 1 || PQgetisnull(res, 0, 0))
  {
    pg_fatal("unexpected result set from query");
  }

  result = pg_strdup(PQgetvalue(res, 0, 0));

  PQclear(res);

  return result;
}

   
                   
                                                
   
static void
run_simple_command(const char *sql)
{
  PGresult *res;

  res = PQexec(conn, sql);

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    pg_fatal("error running query (%s) in source server: %s", sql, PQresultErrorMessage(res));
  }

  PQclear(res);
}

   
                                              
   
XLogRecPtr
libpqGetCurrentXlogInsertLocation(void)
{
  XLogRecPtr result;
  uint32 hi;
  uint32 lo;
  char *val;

  val = run_simple_query("SELECT pg_current_wal_insert_lsn()");

  if (sscanf(val, "%X/%X", &hi, &lo) != 2)
  {
    pg_fatal("unrecognized result \"%s\" for current WAL insert location", val);
  }

  result = ((uint64)hi) << 32 | lo;

  pg_free(val);

  return result;
}

   
                                                  
   
void
libpqProcessFileList(void)
{
  PGresult *res;
  const char *sql;
  int i;

     
                                                                       
     
                                                                             
                                                     
     
                                                                          
                                                                            
                                                
     
  sql = "WITH RECURSIVE files (path, filename, size, isdir) AS (\n"
        "  SELECT '' AS path, filename, size, isdir FROM\n"
        "  (SELECT pg_ls_dir('.', true, false) AS filename) AS fn,\n"
        "        pg_stat_file(fn.filename, true) AS this\n"
        "  UNION ALL\n"
        "  SELECT parent.path || parent.filename || '/' AS path,\n"
        "         fn, this.size, this.isdir\n"
        "  FROM files AS parent,\n"
        "       pg_ls_dir(parent.path || parent.filename, true, false) AS fn,\n"
        "       pg_stat_file(parent.path || parent.filename || '/' || fn, true) AS this\n"
        "       WHERE parent.isdir = 't'\n"
        ")\n"
        "SELECT path || filename, size, isdir,\n"
        "       pg_tablespace_location(pg_tablespace.oid) AS link_target\n"
        "FROM files\n"
        "LEFT OUTER JOIN pg_tablespace ON files.path = 'pg_tblspc/'\n"
        "                             AND oid::text = files.filename\n";
  res = PQexec(conn, sql);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    pg_fatal("could not fetch file list: %s", PQresultErrorMessage(res));
  }

                                   
  if (PQnfields(res) != 4)
  {
    pg_fatal("unexpected result set while fetching file list");
  }

                                      
  for (i = 0; i < PQntuples(res); i++)
  {
    char *path;
    int64 filesize;
    bool isdir;
    char *link_target;
    file_type_t type;

    if (PQgetisnull(res, i, 1))
    {
         
                                                                  
                             
         
      continue;
    }

    path = PQgetvalue(res, i, 0);
    filesize = atol(PQgetvalue(res, i, 1));
    isdir = (strcmp(PQgetvalue(res, i, 2), "t") == 0);
    link_target = PQgetvalue(res, i, 3);

    if (link_target[0])
    {
      type = FILE_TYPE_SYMLINK;
    }
    else if (isdir)
    {
      type = FILE_TYPE_DIRECTORY;
    }
    else
    {
      type = FILE_TYPE_REGULAR;
    }

    process_source_file(path, type, filesize, link_target);
  }
  PQclear(res);
}

       
                                                                           
                                                                          
                                                                   
   
                                                              
                                        
                               
       
   
static void
receiveFileChunks(const char *sql)
{
  PGresult *res;

  if (PQsendQueryParams(conn, sql, 0, NULL, NULL, NULL, NULL, 1) != 1)
  {
    pg_fatal("could not send query: %s", PQerrorMessage(conn));
  }

  pg_log_debug("getting file chunks");

  if (PQsetSingleRowMode(conn) != 1)
  {
    pg_fatal("could not set libpq connection to single row mode");
  }

  while ((res = PQgetResult(conn)) != NULL)
  {
    char *filename;
    int filenamelen;
    int64 chunkoff;
    char chunkoff_str[32];
    int chunksize;
    char *chunk;

    switch (PQresultStatus(res))
    {
    case PGRES_SINGLE_TUPLE:
      break;

    case PGRES_TUPLES_OK:
      PQclear(res);
      continue;                            

    default:
      pg_fatal("unexpected result while fetching remote files: %s", PQresultErrorMessage(res));
    }

                                     
    if (PQnfields(res) != 3 || PQntuples(res) != 1)
    {
      pg_fatal("unexpected result set size while fetching remote files");
    }

    if (PQftype(res, 0) != TEXTOID || PQftype(res, 1) != INT8OID || PQftype(res, 2) != BYTEAOID)
    {
      pg_fatal("unexpected data types in result set while fetching remote files: %u %u %u", PQftype(res, 0), PQftype(res, 1), PQftype(res, 2));
    }

    if (PQfformat(res, 0) != 1 && PQfformat(res, 1) != 1 && PQfformat(res, 2) != 1)
    {
      pg_fatal("unexpected result format while fetching remote files");
    }

    if (PQgetisnull(res, 0, 0) || PQgetisnull(res, 0, 1))
    {
      pg_fatal("unexpected null values in result while fetching remote files");
    }

    if (PQgetlength(res, 0, 1) != sizeof(int64))
    {
      pg_fatal("unexpected result length while fetching remote files");
    }

                                            
    memcpy(&chunkoff, PQgetvalue(res, 0, 1), sizeof(int64));
    chunkoff = pg_ntoh64(chunkoff);
    chunksize = PQgetlength(res, 0, 2);

    filenamelen = PQgetlength(res, 0, 0);
    filename = pg_malloc(filenamelen + 1);
    memcpy(filename, PQgetvalue(res, 0, 0), filenamelen);
    filename[filenamelen] = '\0';

    chunk = PQgetvalue(res, 0, 2);

       
                                                                         
                                                                          
                                                                         
                                                                         
                                                                       
                                                                 
       
    if (PQgetisnull(res, 0, 2))
    {
      pg_log_debug("received null value for chunk for file \"%s\", file has been deleted", filename);
      remove_target_file(filename, true);
      pg_free(filename);
      PQclear(res);
      continue;
    }

       
                                                                   
                             
       
    snprintf(chunkoff_str, sizeof(chunkoff_str), INT64_FORMAT, chunkoff);
    pg_log_debug("received chunk for file \"%s\", offset %s, size %d", filename, chunkoff_str, chunksize);

    open_target_file(filename, false);

    write_target_range(chunk, chunkoff, chunksize);

    pg_free(filename);

    PQclear(res);
  }
}

   
                                               
   
char *
libpqGetFile(const char *filename, size_t *filesize)
{
  PGresult *res;
  char *result;
  int len;
  const char *paramValues[1];

  paramValues[0] = filename;
  res = PQexecParams(conn, "SELECT pg_read_binary_file($1)", 1, NULL, paramValues, NULL, NULL, 1);

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    pg_fatal("could not fetch remote file \"%s\": %s", filename, PQresultErrorMessage(res));
  }

                                   
  if (PQntuples(res) != 1 || PQgetisnull(res, 0, 0))
  {
    pg_fatal("unexpected result set while fetching remote file \"%s\"", filename);
  }

                                      
  len = PQgetlength(res, 0, 0);
  result = pg_malloc(len + 1);
  memcpy(result, PQgetvalue(res, 0, 0), len);
  result[len] = '\0';

  PQclear(res);

  pg_log_debug("fetched file \"%s\", length %d", filename, len);

  if (filesize)
  {
    *filesize = len;
  }
  return result;
}

   
                                                          
   
                                                                            
                                                                             
                                        
   
static void
fetch_file_range(const char *path, uint64 begin, uint64 end)
{
  char linebuf[MAXPGPATH + 23];

                                             
  while (end - begin > 0)
  {
    unsigned int len;

                                                                 
    if (end - begin > CHUNKSIZE)
    {
      len = CHUNKSIZE;
    }
    else
    {
      len = (unsigned int)(end - begin);
    }

    snprintf(linebuf, sizeof(linebuf), "%s\t" UINT64_FORMAT "\t%u\n", path, begin, len);

    if (PQputCopyData(conn, linebuf, strlen(linebuf)) != 1)
    {
      pg_fatal("could not send COPY data: %s", PQerrorMessage(conn));
    }

    begin += len;
  }
}

   
                                                               
   
void
libpq_executeFileMap(filemap_t *map)
{
  file_entry_t *entry;
  const char *sql;
  PGresult *res;
  int i;

     
                                                                         
                    
     
  sql = "CREATE TEMPORARY TABLE fetchchunks(path text, begin int8, len int4);";
  run_simple_command(sql);

  sql = "COPY fetchchunks FROM STDIN";
  res = PQexec(conn, sql);

  if (PQresultStatus(res) != PGRES_COPY_IN)
  {
    pg_fatal("could not send file list: %s", PQresultErrorMessage(res));
  }
  PQclear(res);

  for (i = 0; i < map->narray; i++)
  {
    entry = map->array[i];

                                                              
    execute_pagemap(&entry->pagemap, entry->path);

    switch (entry->action)
    {
    case FILE_ACTION_NONE:
                              
      break;

    case FILE_ACTION_COPY:
                                                        
      open_target_file(entry->path, true);
      fetch_file_range(entry->path, 0, entry->newsize);
      break;

    case FILE_ACTION_TRUNCATE:
      truncate_target_file(entry->path, entry->newsize);
      break;

    case FILE_ACTION_COPY_TAIL:
      fetch_file_range(entry->path, entry->oldsize, entry->newsize);
      break;

    case FILE_ACTION_REMOVE:
      remove_target(entry);
      break;

    case FILE_ACTION_CREATE:
      create_target(entry);
      break;
    }
  }

  if (PQputCopyEnd(conn, NULL) != 1)
  {
    pg_fatal("could not send end-of-COPY: %s", PQerrorMessage(conn));
  }

  while ((res = PQgetResult(conn)) != NULL)
  {
    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
      pg_fatal("unexpected result while sending file list: %s", PQresultErrorMessage(res));
    }
    PQclear(res);
  }

     
                                                                           
                                                               
     
  sql = "SELECT path, begin,\n"
        "  pg_read_binary_file(path, begin, len, true) AS chunk\n"
        "FROM fetchchunks\n";

  receiveFileChunks(sql);
}

static void
execute_pagemap(datapagemap_t *pagemap, const char *path)
{
  datapagemap_iterator_t *iter;
  BlockNumber blkno;
  off_t offset;

  iter = datapagemap_iterate(pagemap);
  while (datapagemap_next(iter, &blkno))
  {
    offset = blkno * BLCKSZ;

    fetch_file_range(path, offset, offset + BLCKSZ);
  }
  pg_free(iter);
}
