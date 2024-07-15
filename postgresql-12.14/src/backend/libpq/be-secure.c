                                                                            
   
               
                                                                          
                                                                 
                                                    
   
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   

#include "postgres.h"

#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#include <arpa/inet.h>
#endif

#include "libpq/libpq.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "tcop/tcopprot.h"
#include "utils/memutils.h"
#include "storage/ipc.h"
#include "storage/proc.h"

char *ssl_library;
char *ssl_cert_file;
char *ssl_key_file;
char *ssl_ca_file;
char *ssl_crl_file;
char *ssl_dh_params_file;
char *ssl_passphrase_command;
bool ssl_passphrase_command_supports_reload;

#ifdef USE_SSL
bool ssl_loaded_verify_locations = false;
#endif

                                              
char *SSLCipherSuites = NULL;

                                          
char *SSLECDHCurve;

                                                   
bool SSLPreferServerCiphers;

int ssl_min_protocol_version;
int ssl_max_protocol_version;

                                                                  
                                                   
                                                                  

   
                              
   
                                                                              
                                                                         
                                                          
   
int
secure_initialize(bool isServerStart)
{
#ifdef USE_SSL
  return be_tls_init(isServerStart);
#else
  return 0;
#endif
}

   
                                   
   
void
secure_destroy(void)
{
#ifdef USE_SSL
  be_tls_destroy();
#endif
}

   
                                                                       
   
bool
secure_loaded_verify_locations(void)
{
#ifdef USE_SSL
  return ssl_loaded_verify_locations;
#else
  return false;
#endif
}

   
                                        
   
int
secure_open_server(Port *port)
{
  int r = 0;

#ifdef USE_SSL
  r = be_tls_open_server(port);

  ereport(DEBUG2, (errmsg("SSL connection from \"%s\"", port->peer_cn ? port->peer_cn : "(anonymous)")));
#endif

  return r;
}

   
                         
   
void
secure_close(Port *port)
{
#ifdef USE_SSL
  if (port->ssl_in_use)
  {
    be_tls_close(port);
  }
#endif
}

   
                                       
   
ssize_t
secure_read(Port *port, void *ptr, size_t len)
{
  ssize_t n;
  int waitfor;

                                                          
  ProcessClientReadInterrupt(false);

retry:
#ifdef USE_SSL
  waitfor = 0;
  if (port->ssl_in_use)
  {
    n = be_tls_read(port, ptr, len, &waitfor);
  }
  else
#endif
#ifdef ENABLE_GSS
      if (port->gss && port->gss->enc)
  {
    n = be_gssapi_read(port, ptr, len);
    waitfor = WL_SOCKET_READABLE;
  }
  else
#endif
  {
    n = secure_raw_read(port, ptr, len);
    waitfor = WL_SOCKET_READABLE;
  }

                                                        
  if (n < 0 && !port->noblock && (errno == EWOULDBLOCK || errno == EAGAIN))
  {
    WaitEvent event;

    Assert(waitfor);

    ModifyWaitEvent(FeBeWaitSet, 0, waitfor, NULL);

    WaitEventSetWait(FeBeWaitSet, -1                 , &event, 1, WAIT_EVENT_CLIENT_READ);

       
                                                                      
                                                                           
                                                                         
                                                                      
                                                                           
                                                                           
                                                                       
                           
       
                                                                           
                                                                      
                                                                           
                                                                          
                                                                           
                                          
       
    if (event.events & WL_POSTMASTER_DEATH)
    {
      ereport(FATAL, (errcode(ERRCODE_ADMIN_SHUTDOWN), errmsg("terminating connection due to unexpected postmaster exit")));
    }

                           
    if (event.events & WL_LATCH_SET)
    {
      ResetLatch(MyLatch);
      ProcessClientReadInterrupt(true);

         
                                                                      
                                                                         
                                       
         
    }
    goto retry;
  }

     
                                                                            
                           
     
  ProcessClientReadInterrupt(false);

  return n;
}

ssize_t
secure_raw_read(Port *port, void *ptr, size_t len)
{
  ssize_t n;

     
                                                                        
                                                                          
     
#ifdef WIN32
  pgwin32_noblock = true;
#endif
  n = recv(port->sock, ptr, len, 0);
#ifdef WIN32
  pgwin32_noblock = false;
#endif

  return n;
}

   
                                      
   
ssize_t
secure_write(Port *port, void *ptr, size_t len)
{
  ssize_t n;
  int waitfor;

                                                          
  ProcessClientWriteInterrupt(false);

retry:
  waitfor = 0;
#ifdef USE_SSL
  if (port->ssl_in_use)
  {
    n = be_tls_write(port, ptr, len, &waitfor);
  }
  else
#endif
#ifdef ENABLE_GSS
      if (port->gss && port->gss->enc)
  {
    n = be_gssapi_write(port, ptr, len);
    waitfor = WL_SOCKET_WRITEABLE;
  }
  else
#endif
  {
    n = secure_raw_write(port, ptr, len);
    waitfor = WL_SOCKET_WRITEABLE;
  }

  if (n < 0 && !port->noblock && (errno == EWOULDBLOCK || errno == EAGAIN))
  {
    WaitEvent event;

    Assert(waitfor);

    ModifyWaitEvent(FeBeWaitSet, 0, waitfor, NULL);

    WaitEventSetWait(FeBeWaitSet, -1                 , &event, 1, WAIT_EVENT_CLIENT_WRITE);

                                      
    if (event.events & WL_POSTMASTER_DEATH)
    {
      ereport(FATAL, (errcode(ERRCODE_ADMIN_SHUTDOWN), errmsg("terminating connection due to unexpected postmaster exit")));
    }

                           
    if (event.events & WL_LATCH_SET)
    {
      ResetLatch(MyLatch);
      ProcessClientWriteInterrupt(true);

         
                                                                       
                                                                         
                                               
         
    }
    goto retry;
  }

     
                                                                            
                            
     
  ProcessClientWriteInterrupt(false);

  return n;
}

ssize_t
secure_raw_write(Port *port, const void *ptr, size_t len)
{
  ssize_t n;

#ifdef WIN32
  pgwin32_noblock = true;
#endif
  n = send(port->sock, ptr, len, 0);
#ifdef WIN32
  pgwin32_noblock = false;
#endif

  return n;
}
