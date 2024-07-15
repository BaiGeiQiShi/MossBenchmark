                                                                            
   
             
                                      
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                                            
   

#ifdef WIN32
   
                                                                      
                                                                        
         
   
#include <io.h>
#endif

#include "postgres_fe.h"

#ifdef WIN32
#include "win32.h"
#else
#include <unistd.h>
#endif

#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>

#include "libpq-fe.h"
#include "libpq-int.h"
#include "libpq/libpq-fs.h"                                 
#include "port/pg_bswap.h"

#define LO_BUFSIZE 8192

static int
lo_initialize(PGconn *conn);
static Oid
lo_import_internal(PGconn *conn, const char *filename, Oid oid);
static pg_int64
lo_hton64(pg_int64 host64);
static pg_int64
lo_ntoh64(pg_int64 net64);

   
           
                                    
   
                                                           
                           
   
int
lo_open(PGconn *conn, Oid lobjId, int mode)
{
  int fd;
  int result_len;
  PQArgBlock argv[2];
  PGresult *res;

  if (conn == NULL || conn->lobjfuncs == NULL)
  {
    if (lo_initialize(conn) < 0)
    {
      return -1;
    }
  }

  argv[0].isint = 1;
  argv[0].len = 4;
  argv[0].u.integer = lobjId;

  argv[1].isint = 1;
  argv[1].len = 4;
  argv[1].u.integer = mode;

  res = PQfn(conn, conn->lobjfuncs->fn_lo_open, &fd, &result_len, 1, argv, 2);
  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return fd;
  }
  else
  {
    PQclear(res);
    return -1;
  }
}

   
            
                                     
   
                          
                            
   
int
lo_close(PGconn *conn, int fd)
{
  PQArgBlock argv[1];
  PGresult *res;
  int retval;
  int result_len;

  if (conn == NULL || conn->lobjfuncs == NULL)
  {
    if (lo_initialize(conn) < 0)
    {
      return -1;
    }
  }

  argv[0].isint = 1;
  argv[0].len = 4;
  argv[0].u.integer = fd;
  res = PQfn(conn, conn->lobjfuncs->fn_lo_close, &retval, &result_len, 1, argv, 1);
  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return retval;
  }
  else
  {
    PQclear(res);
    return -1;
  }
}

   
               
                                                          
   
                          
                           
   
int
lo_truncate(PGconn *conn, int fd, size_t len)
{
  PQArgBlock argv[2];
  PGresult *res;
  int retval;
  int result_len;

  if (conn == NULL || conn->lobjfuncs == NULL)
  {
    if (lo_initialize(conn) < 0)
    {
      return -1;
    }
  }

                                                                 
  if (conn->lobjfuncs->fn_lo_truncate == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("cannot determine OID of function lo_truncate\n"));
    return -1;
  }

     
                                                                             
                                                                             
                                                                       
                                                                           
                                                                        
                                                                      
               
     
  if (len > (size_t)INT_MAX)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("argument of lo_truncate exceeds integer range\n"));
    return -1;
  }

  argv[0].isint = 1;
  argv[0].len = 4;
  argv[0].u.integer = fd;

  argv[1].isint = 1;
  argv[1].len = 4;
  argv[1].u.integer = (int)len;

  res = PQfn(conn, conn->lobjfuncs->fn_lo_truncate, &retval, &result_len, 1, argv, 2);

  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return retval;
  }
  else
  {
    PQclear(res);
    return -1;
  }
}

   
                 
                                                          
   
                          
                           
   
int
lo_truncate64(PGconn *conn, int fd, pg_int64 len)
{
  PQArgBlock argv[2];
  PGresult *res;
  int retval;
  int result_len;

  if (conn == NULL || conn->lobjfuncs == NULL)
  {
    if (lo_initialize(conn) < 0)
    {
      return -1;
    }
  }

  if (conn->lobjfuncs->fn_lo_truncate64 == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("cannot determine OID of function lo_truncate64\n"));
    return -1;
  }

  argv[0].isint = 1;
  argv[0].len = 4;
  argv[0].u.integer = fd;

  len = lo_hton64(len);
  argv[1].isint = 0;
  argv[1].len = 8;
  argv[1].u.ptr = (int *)&len;

  res = PQfn(conn, conn->lobjfuncs->fn_lo_truncate64, &retval, &result_len, 1, argv, 2);

  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return retval;
  }
  else
  {
    PQclear(res);
    return -1;
  }
}

   
           
                                                 
   
                                                       
                                                                           
   

int
lo_read(PGconn *conn, int fd, char *buf, size_t len)
{
  PQArgBlock argv[2];
  PGresult *res;
  int result_len;

  if (conn == NULL || conn->lobjfuncs == NULL)
  {
    if (lo_initialize(conn) < 0)
    {
      return -1;
    }
  }

     
                                                                             
                                                                             
                                                                       
            
     
  if (len > (size_t)INT_MAX)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("argument of lo_read exceeds integer range\n"));
    return -1;
  }

  argv[0].isint = 1;
  argv[0].len = 4;
  argv[0].u.integer = fd;

  argv[1].isint = 1;
  argv[1].len = 4;
  argv[1].u.integer = (int)len;

  res = PQfn(conn, conn->lobjfuncs->fn_lo_read, (void *)buf, &result_len, 0, argv, 2);
  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return result_len;
  }
  else
  {
    PQclear(res);
    return -1;
  }
}

   
            
                                                     
   
                                                          
   
int
lo_write(PGconn *conn, int fd, const char *buf, size_t len)
{
  PQArgBlock argv[2];
  PGresult *res;
  int result_len;
  int retval;

  if (conn == NULL || conn->lobjfuncs == NULL)
  {
    if (lo_initialize(conn) < 0)
    {
      return -1;
    }
  }

     
                                                                             
                                                                             
                                                                       
            
     
  if (len > (size_t)INT_MAX)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("argument of lo_write exceeds integer range\n"));
    return -1;
  }

  argv[0].isint = 1;
  argv[0].len = 4;
  argv[0].u.integer = fd;

  argv[1].isint = 0;
  argv[1].len = (int)len;
  argv[1].u.ptr = (int *)unconstify(char *, buf);

  res = PQfn(conn, conn->lobjfuncs->fn_lo_write, &retval, &result_len, 1, argv, 2);
  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return retval;
  }
  else
  {
    PQclear(res);
    return -1;
  }
}

   
            
                                                                 
   
int
lo_lseek(PGconn *conn, int fd, int offset, int whence)
{
  PQArgBlock argv[3];
  PGresult *res;
  int retval;
  int result_len;

  if (conn == NULL || conn->lobjfuncs == NULL)
  {
    if (lo_initialize(conn) < 0)
    {
      return -1;
    }
  }

  argv[0].isint = 1;
  argv[0].len = 4;
  argv[0].u.integer = fd;

  argv[1].isint = 1;
  argv[1].len = 4;
  argv[1].u.integer = offset;

  argv[2].isint = 1;
  argv[2].len = 4;
  argv[2].u.integer = whence;

  res = PQfn(conn, conn->lobjfuncs->fn_lo_lseek, &retval, &result_len, 1, argv, 3);
  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return retval;
  }
  else
  {
    PQclear(res);
    return -1;
  }
}

   
              
                                                                 
   
pg_int64
lo_lseek64(PGconn *conn, int fd, pg_int64 offset, int whence)
{
  PQArgBlock argv[3];
  PGresult *res;
  pg_int64 retval;
  int result_len;

  if (conn == NULL || conn->lobjfuncs == NULL)
  {
    if (lo_initialize(conn) < 0)
    {
      return -1;
    }
  }

  if (conn->lobjfuncs->fn_lo_lseek64 == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("cannot determine OID of function lo_lseek64\n"));
    return -1;
  }

  argv[0].isint = 1;
  argv[0].len = 4;
  argv[0].u.integer = fd;

  offset = lo_hton64(offset);
  argv[1].isint = 0;
  argv[1].len = 8;
  argv[1].u.ptr = (int *)&offset;

  argv[2].isint = 1;
  argv[2].len = 4;
  argv[2].u.integer = whence;

  res = PQfn(conn, conn->lobjfuncs->fn_lo_lseek64, (void *)&retval, &result_len, 0, argv, 3);
  if (PQresultStatus(res) == PGRES_COMMAND_OK && result_len == 8)
  {
    PQclear(res);
    return lo_ntoh64(retval);
  }
  else
  {
    PQclear(res);
    return -1;
  }
}

   
            
                               
                                                       
   
                                                  
                           
   
Oid
lo_creat(PGconn *conn, int mode)
{
  PQArgBlock argv[1];
  PGresult *res;
  int retval;
  int result_len;

  if (conn == NULL || conn->lobjfuncs == NULL)
  {
    if (lo_initialize(conn) < 0)
    {
      return InvalidOid;
    }
  }

  argv[0].isint = 1;
  argv[0].len = 4;
  argv[0].u.integer = mode;
  res = PQfn(conn, conn->lobjfuncs->fn_lo_creat, &retval, &result_len, 1, argv, 1);
  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return (Oid)retval;
  }
  else
  {
    PQclear(res);
    return InvalidOid;
  }
}

   
             
                               
                                                                           
   
                                                  
                           
   
Oid
lo_create(PGconn *conn, Oid lobjId)
{
  PQArgBlock argv[1];
  PGresult *res;
  int retval;
  int result_len;

  if (conn == NULL || conn->lobjfuncs == NULL)
  {
    if (lo_initialize(conn) < 0)
    {
      return InvalidOid;
    }
  }

                                                                 
  if (conn->lobjfuncs->fn_lo_create == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("cannot determine OID of function lo_create\n"));
    return InvalidOid;
  }

  argv[0].isint = 1;
  argv[0].len = 4;
  argv[0].u.integer = lobjId;
  res = PQfn(conn, conn->lobjfuncs->fn_lo_create, &retval, &result_len, 1, argv, 1);
  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return (Oid)retval;
  }
  else
  {
    PQclear(res);
    return InvalidOid;
  }
}

   
           
                                                           
   
int
lo_tell(PGconn *conn, int fd)
{
  int retval;
  PQArgBlock argv[1];
  PGresult *res;
  int result_len;

  if (conn == NULL || conn->lobjfuncs == NULL)
  {
    if (lo_initialize(conn) < 0)
    {
      return -1;
    }
  }

  argv[0].isint = 1;
  argv[0].len = 4;
  argv[0].u.integer = fd;

  res = PQfn(conn, conn->lobjfuncs->fn_lo_tell, &retval, &result_len, 1, argv, 1);
  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return retval;
  }
  else
  {
    PQclear(res);
    return -1;
  }
}

   
             
                                                           
   
pg_int64
lo_tell64(PGconn *conn, int fd)
{
  pg_int64 retval;
  PQArgBlock argv[1];
  PGresult *res;
  int result_len;

  if (conn == NULL || conn->lobjfuncs == NULL)
  {
    if (lo_initialize(conn) < 0)
    {
      return -1;
    }
  }

  if (conn->lobjfuncs->fn_lo_tell64 == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("cannot determine OID of function lo_tell64\n"));
    return -1;
  }

  argv[0].isint = 1;
  argv[0].len = 4;
  argv[0].u.integer = fd;

  res = PQfn(conn, conn->lobjfuncs->fn_lo_tell64, (void *)&retval, &result_len, 0, argv, 1);
  if (PQresultStatus(res) == PGRES_COMMAND_OK && result_len == 8)
  {
    PQclear(res);
    return lo_ntoh64(retval);
  }
  else
  {
    PQclear(res);
    return -1;
  }
}

   
             
                   
   

int
lo_unlink(PGconn *conn, Oid lobjId)
{
  PQArgBlock argv[1];
  PGresult *res;
  int result_len;
  int retval;

  if (conn == NULL || conn->lobjfuncs == NULL)
  {
    if (lo_initialize(conn) < 0)
    {
      return -1;
    }
  }

  argv[0].isint = 1;
  argv[0].len = 4;
  argv[0].u.integer = lobjId;

  res = PQfn(conn, conn->lobjfuncs->fn_lo_unlink, &retval, &result_len, 1, argv, 1);
  if (PQresultStatus(res) == PGRES_COMMAND_OK)
  {
    PQclear(res);
    return retval;
  }
  else
  {
    PQclear(res);
    return -1;
  }
}

   
               
                                                    
   
                                                
                                   
   

Oid
lo_import(PGconn *conn, const char *filename)
{
  return lo_import_internal(conn, filename, InvalidOid);
}

   
                        
                                                    
                                       
   
                                                
                                   
   

Oid
lo_import_with_oid(PGconn *conn, const char *filename, Oid lobjId)
{
  return lo_import_internal(conn, filename, lobjId);
}

static Oid
lo_import_internal(PGconn *conn, const char *filename, Oid oid)
{
  int fd;
  int nbytes, tmp;
  char buf[LO_BUFSIZE];
  Oid lobjOid;
  int lobj;
  char sebuf[PG_STRERROR_R_BUFLEN];

     
                                 
     
  fd = open(filename, O_RDONLY | PG_BINARY, 0666);
  if (fd < 0)
  {            
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not open file \"%s\": %s\n"), filename, strerror_r(errno, sebuf, sizeof(sebuf)));
    return InvalidOid;
  }

     
                                
     
  if (oid == InvalidOid)
  {
    lobjOid = lo_creat(conn, INV_READ | INV_WRITE);
  }
  else
  {
    lobjOid = lo_create(conn, oid);
  }

  if (lobjOid == InvalidOid)
  {
                                                                    
    (void)close(fd);
    return InvalidOid;
  }

  lobj = lo_open(conn, lobjOid, INV_WRITE);
  if (lobj == -1)
  {
                                                                  
    (void)close(fd);
    return InvalidOid;
  }

     
                                                         
     
  while ((nbytes = read(fd, buf, LO_BUFSIZE)) > 0)
  {
    tmp = lo_write(conn, lobj, buf, nbytes);
    if (tmp != nbytes)
    {
         
                                                                       
                                                                     
                                                                       
                                                       
         
      (void)close(fd);
      return InvalidOid;
    }
  }

  if (nbytes < 0)
  {
                                                             
    int save_errno = errno;

    (void)lo_close(conn, lobj);
    (void)close(fd);
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not read from file \"%s\": %s\n"), filename, strerror_r(save_errno, sebuf, sizeof(sebuf)));
    return InvalidOid;
  }

  (void)close(fd);

  if (lo_close(conn, lobj) != 0)
  {
                                                                   
    return InvalidOid;
  }

  return lobjOid;
}

   
               
                                          
                                    
   
int
lo_export(PGconn *conn, Oid lobjId, const char *filename)
{
  int result = 1;
  int fd;
  int nbytes, tmp;
  char buf[LO_BUFSIZE];
  int lobj;
  char sebuf[PG_STRERROR_R_BUFLEN];

     
                            
     
  lobj = lo_open(conn, lobjId, INV_READ);
  if (lobj == -1)
  {
                                                                  
    return -1;
  }

     
                                      
     
  fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC | PG_BINARY, 0666);
  if (fd < 0)
  {
                                                             
    int save_errno = errno;

    (void)lo_close(conn, lobj);
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not open file \"%s\": %s\n"), filename, strerror_r(save_errno, sebuf, sizeof(sebuf)));
    return -1;
  }

     
                                                         
     
  while ((nbytes = lo_read(conn, lobj, buf, LO_BUFSIZE)) > 0)
  {
    tmp = write(fd, buf, nbytes);
    if (tmp != nbytes)
    {
                                                               
      int save_errno = errno;

      (void)lo_close(conn, lobj);
      (void)close(fd);
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not write to file \"%s\": %s\n"), filename, strerror_r(save_errno, sebuf, sizeof(sebuf)));
      return -1;
    }
  }

     
                                                                             
                                                                         
                                                                            
                     
     
  if (nbytes < 0 || lo_close(conn, lobj) != 0)
  {
                                                                      
    result = -1;
  }

                                                                         
  if (close(fd) && result >= 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not write to file \"%s\": %s\n"), filename, strerror_r(errno, sebuf, sizeof(sebuf)));
    result = -1;
  }

  return result;
}

   
                 
   
                                                                     
                                                                   
                                                            
   
static int
lo_initialize(PGconn *conn)
{
  PGresult *res;
  PGlobjfuncs *lobjfuncs;
  int n;
  const char *query;
  const char *fname;
  Oid foid;

  if (!conn)
  {
    return -1;
  }

     
                                                        
     
  lobjfuncs = (PGlobjfuncs *)malloc(sizeof(PGlobjfuncs));
  if (lobjfuncs == NULL)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
    return -1;
  }
  MemSet((char *)lobjfuncs, 0, sizeof(PGlobjfuncs));

     
                                                                           
                                                                      
                                            
     
  if (conn->sversion >= 70300)
  {
    query = "select proname, oid from pg_catalog.pg_proc "
            "where proname in ("
            "'lo_open', "
            "'lo_close', "
            "'lo_creat', "
            "'lo_create', "
            "'lo_unlink', "
            "'lo_lseek', "
            "'lo_lseek64', "
            "'lo_tell', "
            "'lo_tell64', "
            "'lo_truncate', "
            "'lo_truncate64', "
            "'loread', "
            "'lowrite') "
            "and pronamespace = (select oid from pg_catalog.pg_namespace "
            "where nspname = 'pg_catalog')";
  }
  else
  {
    query = "select proname, oid from pg_proc "
            "where proname = 'lo_open' "
            "or proname = 'lo_close' "
            "or proname = 'lo_creat' "
            "or proname = 'lo_unlink' "
            "or proname = 'lo_lseek' "
            "or proname = 'lo_tell' "
            "or proname = 'loread' "
            "or proname = 'lowrite'";
  }

  res = PQexec(conn, query);
  if (res == NULL)
  {
    free(lobjfuncs);
    return -1;
  }

  if (res->resultStatus != PGRES_TUPLES_OK)
  {
    free(lobjfuncs);
    PQclear(res);
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("query to initialize large object functions did not return data\n"));
    return -1;
  }

     
                                                          
     
  for (n = 0; n < PQntuples(res); n++)
  {
    fname = PQgetvalue(res, n, 0);
    foid = (Oid)atoi(PQgetvalue(res, n, 1));
    if (strcmp(fname, "lo_open") == 0)
    {
      lobjfuncs->fn_lo_open = foid;
    }
    else if (strcmp(fname, "lo_close") == 0)
    {
      lobjfuncs->fn_lo_close = foid;
    }
    else if (strcmp(fname, "lo_creat") == 0)
    {
      lobjfuncs->fn_lo_creat = foid;
    }
    else if (strcmp(fname, "lo_create") == 0)
    {
      lobjfuncs->fn_lo_create = foid;
    }
    else if (strcmp(fname, "lo_unlink") == 0)
    {
      lobjfuncs->fn_lo_unlink = foid;
    }
    else if (strcmp(fname, "lo_lseek") == 0)
    {
      lobjfuncs->fn_lo_lseek = foid;
    }
    else if (strcmp(fname, "lo_lseek64") == 0)
    {
      lobjfuncs->fn_lo_lseek64 = foid;
    }
    else if (strcmp(fname, "lo_tell") == 0)
    {
      lobjfuncs->fn_lo_tell = foid;
    }
    else if (strcmp(fname, "lo_tell64") == 0)
    {
      lobjfuncs->fn_lo_tell64 = foid;
    }
    else if (strcmp(fname, "lo_truncate") == 0)
    {
      lobjfuncs->fn_lo_truncate = foid;
    }
    else if (strcmp(fname, "lo_truncate64") == 0)
    {
      lobjfuncs->fn_lo_truncate64 = foid;
    }
    else if (strcmp(fname, "loread") == 0)
    {
      lobjfuncs->fn_lo_read = foid;
    }
    else if (strcmp(fname, "lowrite") == 0)
    {
      lobjfuncs->fn_lo_write = foid;
    }
  }

  PQclear(res);

     
                                                                             
                                                                             
                   
     
  if (lobjfuncs->fn_lo_open == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("cannot determine OID of function lo_open\n"));
    free(lobjfuncs);
    return -1;
  }
  if (lobjfuncs->fn_lo_close == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("cannot determine OID of function lo_close\n"));
    free(lobjfuncs);
    return -1;
  }
  if (lobjfuncs->fn_lo_creat == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("cannot determine OID of function lo_creat\n"));
    free(lobjfuncs);
    return -1;
  }
  if (lobjfuncs->fn_lo_unlink == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("cannot determine OID of function lo_unlink\n"));
    free(lobjfuncs);
    return -1;
  }
  if (lobjfuncs->fn_lo_lseek == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("cannot determine OID of function lo_lseek\n"));
    free(lobjfuncs);
    return -1;
  }
  if (lobjfuncs->fn_lo_tell == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("cannot determine OID of function lo_tell\n"));
    free(lobjfuncs);
    return -1;
  }
  if (lobjfuncs->fn_lo_read == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("cannot determine OID of function loread\n"));
    free(lobjfuncs);
    return -1;
  }
  if (lobjfuncs->fn_lo_write == 0)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("cannot determine OID of function lowrite\n"));
    free(lobjfuncs);
    return -1;
  }

     
                                                   
     
  conn->lobjfuncs = lobjfuncs;
  return 0;
}

   
             
                                                                          
   
static pg_int64
lo_hton64(pg_int64 host64)
{
  union
  {
    pg_int64 i64;
    uint32 i32[2];
  } swap;
  uint32 t;

                                                          
  t = (uint32)(host64 >> 32);
  swap.i32[0] = pg_hton32(t);

                              
  t = (uint32)host64;
  swap.i32[1] = pg_hton32(t);

  return swap.i64;
}

   
             
                                                                          
   
static pg_int64
lo_ntoh64(pg_int64 net64)
{
  union
  {
    pg_int64 i64;
    uint32 i32[2];
  } swap;
  pg_int64 result;

  swap.i64 = net64;

  result = (uint32)pg_ntoh32(swap.i32[0]);
  result <<= 32;
  result |= (uint32)pg_ntoh32(swap.i32[1]);

  return result;
}
