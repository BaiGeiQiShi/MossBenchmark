                                                                            
   
                      
   
                                                      
   
                                                                               
                                                                        
                                                
   
                                                                         
                                                                        
   
                  
                                             
   
                                                                            
   

#include "postgres_fe.h"

#include "fe-secure-common.h"

#include "libpq-int.h"
#include "pqexpbuffer.h"

   
                                                                
   
                         
                                                  
                                                         
                                                                         
                                   
                                                              
   
                                                                            
                                                            
   
                                                                       
   
static bool
wildcard_certificate_match(const char *pattern, const char *string)
{
  int lenpat = strlen(pattern);
  int lenstr = strlen(string);

                                                                        
  if (lenpat < 3 || pattern[0] != '*' || pattern[1] != '.')
  {
    return false;
  }

                                                                
  if (lenpat > lenstr)
  {
    return false;
  }

     
                                                                            
     
  if (pg_strcasecmp(pattern + 1, string + lenstr - lenpat + 1) != 0)
  {
    return false;
  }

     
                                                                            
                    
     
  if (strchr(string, '.') < string + lenstr - lenpat)
  {
    return false;
  }

                                                                            
  return true;
}

   
                                                                            
   
                                                                          
                                         
   
                                                                           
                                         
   
int
pq_verify_peer_name_matches_certificate_name(PGconn *conn, const char *namedata, size_t namelen, char **store_name)
{
  char *name;
  int result;
  char *host = conn->connhost[conn->whichhost].host;

  *store_name = NULL;

  if (!(host && host[0] != '\0'))
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("host name must be specified\n"));
    return -1;
  }

     
                                                                       
                                              
     
  name = malloc(namelen + 1);
  if (name == NULL)
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("out of memory\n"));
    return -1;
  }
  memcpy(name, namedata, namelen);
  name[namelen] = '\0';

     
                                                                        
                                         
     
  if (namelen != strlen(name))
  {
    free(name);
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("SSL certificate's name contains embedded null\n"));
    return -1;
  }

  if (pg_strcasecmp(name, host) == 0)
  {
                          
    result = 1;
  }
  else if (wildcard_certificate_match(name, host))
  {
                               
    result = 1;
  }
  else
  {
    result = 0;
  }

  *store_name = name;
  return result;
}

   
                                                                            
   
                                                                               
   
bool
pq_verify_peer_name_matches_certificate(PGconn *conn)
{
  char *host = conn->connhost[conn->whichhost].host;
  int rc;
  int names_examined = 0;
  char *first_name = NULL;

     
                                                                   
                                                      
     
  if (strcmp(conn->sslmode, "verify-full") != 0)
  {
    return true;
  }

                                                      
  if (!(host && host[0] != '\0'))
  {
    printfPQExpBuffer(&conn->errorMessage, libpq_gettext("host name must be specified for a verified SSL connection\n"));
    return false;
  }

  rc = pgtls_verify_peer_name_matches_certificate_guts(conn, &names_examined, &first_name);

  if (rc == 0)
  {
       
                                                                           
                                                                     
                                                                        
                      
       
    if (names_examined > 1)
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_ngettext("server certificate for \"%s\" (and %d other name) does not match host name \"%s\"\n", "server certificate for \"%s\" (and %d other names) does not match host name \"%s\"\n", names_examined - 1), first_name, names_examined - 1, host);
    }
    else if (names_examined == 1)
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("server certificate for \"%s\" does not match host name \"%s\"\n"), first_name, host);
    }
    else
    {
      printfPQExpBuffer(&conn->errorMessage, libpq_gettext("could not get server's host name from server certificate\n"));
    }
  }

                
  if (first_name)
  {
    free(first_name);
  }

  return (rc == 1);
}
