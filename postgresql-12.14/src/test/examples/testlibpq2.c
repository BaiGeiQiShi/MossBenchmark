   
                                  
   
   
                
                                                    
   
                                                           
                 
                                                  
   
                                           
                                                   
                                                   
   
                              
                                  
                                
                                
                                           
                                                      
   
                                                          
   
                                             
   

#ifdef WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "libpq-fe.h"

static void
exit_nicely(PGconn *conn)
{
  PQfinish(conn);
  exit(1);
}

int
main(int argc, char **argv)
{
  const char *conninfo;
  PGconn *conn;
  PGresult *res;
  PGnotify *notify;
  int nnotifies;

     
                                                                         
                                                                             
                                                                            
     
  if (argc > 1)
  {
    conninfo = argv[1];
  }
  else
  {
    conninfo = "dbname = postgres";
  }

                                         
  conn = PQconnectdb(conninfo);

                                                                      
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

     
                                                                          
     
  res = PQexec(conn, "LISTEN TBL2");
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "LISTEN command failed: %s", PQerrorMessage(conn));
    PQclear(res);
    exit_nicely(conn);
  }
  PQclear(res);

                                              
  nnotifies = 0;
  while (nnotifies < 4)
  {
       
                                                                          
                                                                   
                   
       
    int sock;
    fd_set input_mask;

    sock = PQsocket(conn);

    if (sock < 0)
    {
      break;                       
    }

    FD_ZERO(&input_mask);
    FD_SET(sock, &input_mask);

    if (select(sock + 1, &input_mask, NULL, NULL, NULL) < 0)
    {
      fprintf(stderr, "select() failed: %s\n", strerror(errno));
      exit_nicely(conn);
    }

                             
    PQconsumeInput(conn);
    while ((notify = PQnotifies(conn)) != NULL)
    {
      fprintf(stderr, "ASYNC NOTIFY of '%s' received from backend PID %d\n", notify->relname, notify->be_pid);
      PQfreemem(notify);
      nnotifies++;
      PQconsumeInput(conn);
    }
  }

  fprintf(stderr, "Done.\n");

                                                        
  PQfinish(conn);

  return 0;
}
