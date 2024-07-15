                                                                            
   
              
                                                           
   
                                                                         
                                                                        
   
   
                  
                                  
   
                                                                            
   
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "libpq-fe.h"
#include "libpq/libpq-fs.h"

#define BUFSIZE 1024

   
                
                                                                       
   
   
static Oid
importFile(PGconn *conn, char *filename)
{
  Oid lobjId;
  int lobj_fd;
  char buf[BUFSIZE];
  int nbytes, tmp;
  int fd;

     
                                 
     
  fd = open(filename, O_RDONLY, 0666);
  if (fd < 0)
  {            
    fprintf(stderr, "cannot open unix file\"%s\"\n", filename);
  }

     
                             
     
  lobjId = lo_creat(conn, INV_READ | INV_WRITE);
  if (lobjId == 0)
  {
    fprintf(stderr, "cannot create large object");
  }

  lobj_fd = lo_open(conn, lobjId, INV_WRITE);

     
                                                                
     
  while ((nbytes = read(fd, buf, BUFSIZE)) > 0)
  {
    tmp = lo_write(conn, lobj_fd, buf, nbytes);
    if (tmp < nbytes)
    {
      fprintf(stderr, "error while reading \"%s\"", filename);
    }
  }

  close(fd);
  lo_close(conn, lobj_fd);

  return lobjId;
}

static void
pickout(PGconn *conn, Oid lobjId, pg_int64 start, int len)
{
  int lobj_fd;
  char *buf;
  int nbytes;
  int nread;

  lobj_fd = lo_open(conn, lobjId, INV_READ);
  if (lobj_fd < 0)
  {
    fprintf(stderr, "cannot open large object %u", lobjId);
  }

  if (lo_lseek64(conn, lobj_fd, start, SEEK_SET) < 0)
  {
    fprintf(stderr, "error in lo_lseek64: %s", PQerrorMessage(conn));
  }

  if (lo_tell64(conn, lobj_fd) != start)
  {
    fprintf(stderr, "error in lo_tell64: %s", PQerrorMessage(conn));
  }

  buf = malloc(len + 1);

  nread = 0;
  while (len - nread > 0)
  {
    nbytes = lo_read(conn, lobj_fd, buf, len - nread);
    buf[nbytes] = '\0';
    fprintf(stderr, ">>> %s", buf);
    nread += nbytes;
    if (nbytes <= 0)
    {
      break;                    
    }
  }
  free(buf);
  fprintf(stderr, "\n");
  lo_close(conn, lobj_fd);
}

static void
overwrite(PGconn *conn, Oid lobjId, pg_int64 start, int len)
{
  int lobj_fd;
  char *buf;
  int nbytes;
  int nwritten;
  int i;

  lobj_fd = lo_open(conn, lobjId, INV_WRITE);
  if (lobj_fd < 0)
  {
    fprintf(stderr, "cannot open large object %u", lobjId);
  }

  if (lo_lseek64(conn, lobj_fd, start, SEEK_SET) < 0)
  {
    fprintf(stderr, "error in lo_lseek64: %s", PQerrorMessage(conn));
  }

  buf = malloc(len + 1);

  for (i = 0; i < len; i++)
  {
    buf[i] = 'X';
  }
  buf[i] = '\0';

  nwritten = 0;
  while (len - nwritten > 0)
  {
    nbytes = lo_write(conn, lobj_fd, buf + nwritten, len - nwritten);
    nwritten += nbytes;
    if (nbytes <= 0)
    {
      fprintf(stderr, "\nWRITE FAILED!\n");
      break;
    }
  }
  free(buf);
  fprintf(stderr, "\n");
  lo_close(conn, lobj_fd);
}

static void
my_truncate(PGconn *conn, Oid lobjId, pg_int64 len)
{
  int lobj_fd;

  lobj_fd = lo_open(conn, lobjId, INV_READ | INV_WRITE);
  if (lobj_fd < 0)
  {
    fprintf(stderr, "cannot open large object %u", lobjId);
  }

  if (lo_truncate64(conn, lobj_fd, len) < 0)
  {
    fprintf(stderr, "error in lo_truncate64: %s", PQerrorMessage(conn));
  }

  lo_close(conn, lobj_fd);
}

   
                
                                                          
   
   
static void
exportFile(PGconn *conn, Oid lobjId, char *filename)
{
  int lobj_fd;
  char buf[BUFSIZE];
  int nbytes, tmp;
  int fd;

     
                           
     
  lobj_fd = lo_open(conn, lobjId, INV_READ);
  if (lobj_fd < 0)
  {
    fprintf(stderr, "cannot open large object %u", lobjId);
  }

     
                                    
     
  fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0666);
  if (fd < 0)
  {            
    fprintf(stderr, "cannot open unix file\"%s\"", filename);
  }

     
                                                                
     
  while ((nbytes = lo_read(conn, lobj_fd, buf, BUFSIZE)) > 0)
  {
    tmp = write(fd, buf, nbytes);
    if (tmp < nbytes)
    {
      fprintf(stderr, "error while writing \"%s\"", filename);
    }
  }

  lo_close(conn, lobj_fd);
  close(fd);

  return;
}

static void
exit_nicely(PGconn *conn)
{
  PQfinish(conn);
  exit(1);
}

int
main(int argc, char **argv)
{
  char *in_filename, *out_filename, *out_filename2;
  char *database;
  Oid lobjOid;
  PGconn *conn;
  PGresult *res;

  if (argc != 5)
  {
    fprintf(stderr, "Usage: %s database_name in_filename out_filename out_filename2\n", argv[0]);
    exit(1);
  }

  database = argv[1];
  in_filename = argv[2];
  out_filename = argv[3];
  out_filename2 = argv[4];

     
                           
     
  conn = PQsetdb(NULL, NULL, NULL, NULL, database);

                                                                      
  if (PQstatus(conn) != CONNECTION_OK)
  {
    fprintf(stderr, "Connection to database failed: %s", PQerrorMessage(conn));
    exit_nicely(conn);
  }

                                                                             
  res = PQexec(conn, "SELECT pg_catalog.set_config('search_path', '', false)");
  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "SET failed: %s", PQerrorMessage(conn));
    PQclear(res);
    exit_nicely(conn);
  }
  PQclear(res);

  res = PQexec(conn, "begin");
  PQclear(res);
  printf("importing file \"%s\" ...\n", in_filename);
                                                
  lobjOid = lo_import(conn, in_filename);
  if (lobjOid == 0)
  {
    fprintf(stderr, "%s\n", PQerrorMessage(conn));
  }
  else
  {
    printf("\tas large object %u.\n", lobjOid);

    printf("picking out bytes 4294967000-4294968000 of the large object\n");
    pickout(conn, lobjOid, 4294967000U, 1000);

    printf("overwriting bytes 4294967000-4294968000 of the large object with X's\n");
    overwrite(conn, lobjOid, 4294967000U, 1000);

    printf("exporting large object to file \"%s\" ...\n", out_filename);
                                                   
    if (lo_export(conn, lobjOid, out_filename) < 0)
    {
      fprintf(stderr, "%s\n", PQerrorMessage(conn));
    }

    printf("truncating to 3294968000 bytes\n");
    my_truncate(conn, lobjOid, 3294968000U);

    printf("exporting truncated large object to file \"%s\" ...\n", out_filename2);
    if (lo_export(conn, lobjOid, out_filename2) < 0)
    {
      fprintf(stderr, "%s\n", PQerrorMessage(conn));
    }
  }

  res = PQexec(conn, "end");
  PQclear(res);
  PQfinish(conn);
  return 0;
}
