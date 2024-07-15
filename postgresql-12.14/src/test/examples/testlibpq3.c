   
                                  
   
   
                
                                                
   
                                                                        
                                                   
   
                             
                                 
                                                 
                                                                              
                                                                          
   
                           
   
                
                   
                                
                                      
   
                
                   
                            
                                      
   

#ifdef WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "libpq-fe.h"

                     
#include <netinet/in.h>
#include <arpa/inet.h>

static void
exit_nicely(PGconn *conn)
{
  PQfinish(conn);
  exit(1);
}

   
                                                                          
                                                                         
                                  
   
static void
show_binary_results(PGresult *res)
{
  int i, j;
  int i_fnum, t_fnum, b_fnum;

                                                                      
  i_fnum = PQfnumber(res, "i");
  t_fnum = PQfnumber(res, "t");
  b_fnum = PQfnumber(res, "b");

  for (i = 0; i < PQntuples(res); i++)
  {
    char *iptr;
    char *tptr;
    char *bptr;
    int blen;
    int ival;

                                                                     
    iptr = PQgetvalue(res, i, i_fnum);
    tptr = PQgetvalue(res, i, t_fnum);
    bptr = PQgetvalue(res, i, b_fnum);

       
                                                                         
                                                   
       
    ival = ntohl(*((uint32_t *)iptr));

       
                                                                         
                                                                         
                      
       
                                                                           
                                                                           
       
    blen = PQgetlength(res, i, b_fnum);

    printf("tuple %d: got\n", i);
    printf(" i = (%d bytes) %d\n", PQgetlength(res, i, i_fnum), ival);
    printf(" t = (%d bytes) '%s'\n", PQgetlength(res, i, t_fnum), tptr);
    printf(" b = (%d bytes) ", blen);
    for (j = 0; j < blen; j++)
    {
      printf("\\%03o", bptr[j]);
    }
    printf("\n\n");
  }
}

int
main(int argc, char **argv)
{
  const char *conninfo;
  PGconn *conn;
  PGresult *res;
  const char *paramValues[1];
  int paramLengths[1];
  int paramFormats[1];
  uint32_t binaryIntVal;

     
                                                                         
                                                                             
                                                                            
     
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

                                                                             
  res = PQexec(conn, "SET search_path = testlibpq3");
  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    fprintf(stderr, "SET failed: %s", PQerrorMessage(conn));
    PQclear(res);
    exit_nicely(conn);
  }
  PQclear(res);

     
                                                                           
                                                                     
     
                                                                           
                                                                             
                                                                           
                                                                             
                                            
     

                                               
  paramValues[0] = "joe's place";

  res = PQexecParams(conn, "SELECT * FROM test1 WHERE t = $1", 1,                
      NULL,                                                                                              
      paramValues, NULL,                                                                                   
      NULL,                                                                                       
      1);                                                                                     

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "SELECT failed: %s", PQerrorMessage(conn));
    PQclear(res);
    exit_nicely(conn);
  }

  show_binary_results(res);

  PQclear(res);

     
                                                                             
                                                    
     
                                                                     
                                                                           
                                                                           
                        
     

                                                       
  binaryIntVal = htonl((uint32_t)2);

                                                
  paramValues[0] = (char *)&binaryIntVal;
  paramLengths[0] = sizeof(binaryIntVal);
  paramFormats[0] = 1;             

  res = PQexecParams(conn, "SELECT * FROM test1 WHERE i = $1::int4", 1,                
      NULL,                                                                                                    
      paramValues, paramLengths, paramFormats, 1);                                                  

  if (PQresultStatus(res) != PGRES_TUPLES_OK)
  {
    fprintf(stderr, "SELECT failed: %s", PQerrorMessage(conn));
    PQclear(res);
    exit_nicely(conn);
  }

  show_binary_results(res);

  PQclear(res);

                                                        
  PQfinish(conn);

  return 0;
}
