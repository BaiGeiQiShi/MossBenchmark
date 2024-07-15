                                                                            
   
            
                                              
   
                                                                         
   
                  
                                     
   
                                                                            
   

#include "postgres.h"

   
                                                                
                         
   
                                                                     
                                                                     
                                                                    
              
   
                                                                         
                                                            
   
int pgwin32_noblock = 0;

                                                                            
#undef socket
#undef bind
#undef listen
#undef accept
#undef connect
#undef select
#undef recv
#undef send

   
                                                                
                                                                  
   

   
                                                 
   
                                                                            
                                                                          
                                                                             
                                                                        
                                                                            
                             
   
static void
TranslateSocketError(void)
{
  switch (WSAGetLastError())
  {
  case WSAEINVAL:
  case WSANOTINITIALISED:
  case WSAEINVALIDPROVIDER:
  case WSAEINVALIDPROCTABLE:
  case WSAEDESTADDRREQ:
    errno = EINVAL;
    break;
  case WSAEINPROGRESS:
    errno = EINPROGRESS;
    break;
  case WSAEFAULT:
    errno = EFAULT;
    break;
  case WSAEISCONN:
    errno = EISCONN;
    break;
  case WSAEMSGSIZE:
    errno = EMSGSIZE;
    break;
  case WSAEAFNOSUPPORT:
    errno = EAFNOSUPPORT;
    break;
  case WSAEMFILE:
    errno = EMFILE;
    break;
  case WSAENOBUFS:
    errno = ENOBUFS;
    break;
  case WSAEPROTONOSUPPORT:
  case WSAEPROTOTYPE:
  case WSAESOCKTNOSUPPORT:
    errno = EPROTONOSUPPORT;
    break;
  case WSAECONNABORTED:
    errno = ECONNABORTED;
    break;
  case WSAECONNREFUSED:
    errno = ECONNREFUSED;
    break;
  case WSAECONNRESET:
    errno = ECONNRESET;
    break;
  case WSAEINTR:
    errno = EINTR;
    break;
  case WSAENOTSOCK:
    errno = ENOTSOCK;
    break;
  case WSAEOPNOTSUPP:
    errno = EOPNOTSUPP;
    break;
  case WSAEWOULDBLOCK:
    errno = EWOULDBLOCK;
    break;
  case WSAEACCES:
    errno = EACCES;
    break;
  case WSAEADDRINUSE:
    errno = EADDRINUSE;
    break;
  case WSAEADDRNOTAVAIL:
    errno = EADDRNOTAVAIL;
    break;
  case WSAEHOSTUNREACH:
  case WSAEHOSTDOWN:
  case WSAHOST_NOT_FOUND:
  case WSAENETDOWN:
  case WSAENETUNREACH:
  case WSAENETRESET:
    errno = EHOSTUNREACH;
    break;
  case WSAENOTCONN:
  case WSAESHUTDOWN:
  case WSAEDISCON:
    errno = ENOTCONN;
    break;
  default:
    ereport(NOTICE, (errmsg_internal("unrecognized win32 socket error code: %d", WSAGetLastError())));
    errno = EINVAL;
  }
}

static int
pgwin32_poll_signals(void)
{
  if (UNBLOCKED_SIGNAL_QUEUE())
  {
    pgwin32_dispatch_queued_signals();
    errno = EINTR;
    return 1;
  }
  return 0;
}

static int
isDataGram(SOCKET s)
{
  int type;
  int typelen = sizeof(type);

  if (getsockopt(s, SOL_SOCKET, SO_TYPE, (char *)&type, &typelen))
  {
    return 1;
  }

  return (type == SOCK_DGRAM) ? 1 : 0;
}

int
pgwin32_waitforsinglesocket(SOCKET s, int what, int timeout)
{
  static HANDLE waitevent = INVALID_HANDLE_VALUE;
  static SOCKET current_socket = INVALID_SOCKET;
  static int isUDP = 0;
  HANDLE events[2];
  int r;

                                                                       
  if (waitevent == INVALID_HANDLE_VALUE)
  {
    waitevent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (waitevent == INVALID_HANDLE_VALUE)
    {
      ereport(ERROR, (errmsg_internal("could not create socket waiting event: error code %lu", GetLastError())));
    }
  }
  else if (!ResetEvent(waitevent))
  {
    ereport(ERROR, (errmsg_internal("could not reset socket waiting event: error code %lu", GetLastError())));
  }

     
                                                                         
                                                                         
                                                   
     
  if (current_socket != s)
  {
    isUDP = isDataGram(s);
  }
  current_socket = s;

     
                                                                   
                                                                           
                 
     
  if (WSAEventSelect(s, waitevent, what) != 0)
  {
    TranslateSocketError();
    return 0;
  }

  events[0] = pgwin32_signal_event;
  events[1] = waitevent;

     
                                                                             
                                                                  
                                                                          
                                                                         
                                                                           
     
  if ((what & FD_WRITE) && isUDP)
  {
    for (;;)
    {
      r = WaitForMultipleObjectsEx(2, events, FALSE, 100, TRUE);

      if (r == WAIT_TIMEOUT)
      {
        char c;
        WSABUF buf;
        DWORD sent;

        buf.buf = &c;
        buf.len = 0;

        r = WSASend(s, &buf, 1, &sent, 0, NULL, NULL);
        if (r == 0)                                         
        {
          WSAEventSelect(s, NULL, 0);
          return 1;
        }
        else if (WSAGetLastError() != WSAEWOULDBLOCK)
        {
          TranslateSocketError();
          WSAEventSelect(s, NULL, 0);
          return 0;
        }
      }
      else
      {
        break;
      }
    }
  }
  else
  {
    r = WaitForMultipleObjectsEx(2, events, FALSE, timeout, TRUE);
  }

  WSAEventSelect(s, NULL, 0);

  if (r == WAIT_OBJECT_0 || r == WAIT_IO_COMPLETION)
  {
    pgwin32_dispatch_queued_signals();
    errno = EINTR;
    return 0;
  }
  if (r == WAIT_OBJECT_0 + 1)
  {
    return 1;
  }
  if (r == WAIT_TIMEOUT)
  {
    errno = EWOULDBLOCK;
    return 0;
  }
  ereport(ERROR, (errmsg_internal("unrecognized return value from WaitForMultipleObjects: %d (error code %lu)", r, GetLastError())));
  return 0;
}

   
                                                              
   
SOCKET
pgwin32_socket(int af, int type, int protocol)
{
  SOCKET s;
  unsigned long on = 1;

  s = WSASocket(af, type, protocol, NULL, 0, WSA_FLAG_OVERLAPPED);
  if (s == INVALID_SOCKET)
  {
    TranslateSocketError();
    return INVALID_SOCKET;
  }

  if (ioctlsocket(s, FIONBIO, &on))
  {
    TranslateSocketError();
    return INVALID_SOCKET;
  }
  errno = 0;

  return s;
}

int
pgwin32_bind(SOCKET s, struct sockaddr *addr, int addrlen)
{
  int res;

  res = bind(s, addr, addrlen);
  if (res < 0)
  {
    TranslateSocketError();
  }
  return res;
}

int
pgwin32_listen(SOCKET s, int backlog)
{
  int res;

  res = listen(s, backlog);
  if (res < 0)
  {
    TranslateSocketError();
  }
  return res;
}

SOCKET
pgwin32_accept(SOCKET s, struct sockaddr *addr, int *addrlen)
{
  SOCKET rs;

     
                                                                          
                      
     
  pgwin32_poll_signals();

  rs = WSAAccept(s, addr, addrlen, NULL, 0);
  if (rs == INVALID_SOCKET)
  {
    TranslateSocketError();
    return INVALID_SOCKET;
  }
  return rs;
}

                                        
int
pgwin32_connect(SOCKET s, const struct sockaddr *addr, int addrlen)
{
  int r;

  r = WSAConnect(s, addr, addrlen, NULL, NULL, NULL, NULL);
  if (r == 0)
  {
    return 0;
  }

  if (WSAGetLastError() != WSAEWOULDBLOCK)
  {
    TranslateSocketError();
    return -1;
  }

  while (pgwin32_waitforsinglesocket(s, FD_CONNECT, INFINITE) == 0)
  {
                                                                  
  }

  return 0;
}

int
pgwin32_recv(SOCKET s, char *buf, int len, int f)
{
  WSABUF wbuf;
  int r;
  DWORD b;
  DWORD flags = f;
  int n;

  if (pgwin32_poll_signals())
  {
    return -1;
  }

  wbuf.len = len;
  wbuf.buf = buf;

  r = WSARecv(s, &wbuf, 1, &b, &flags, NULL, NULL);
  if (r != SOCKET_ERROR)
  {
    return b;              
  }

  if (WSAGetLastError() != WSAEWOULDBLOCK)
  {
    TranslateSocketError();
    return -1;
  }

  if (pgwin32_noblock)
  {
       
                                                                        
                                                                 
       
    errno = EWOULDBLOCK;
    return -1;
  }

                                                

  for (n = 0; n < 5; n++)
  {
    if (pgwin32_waitforsinglesocket(s, FD_READ | FD_CLOSE | FD_ACCEPT, INFINITE) == 0)
    {
      return -1;                        
    }

    r = WSARecv(s, &wbuf, 1, &b, &flags, NULL, NULL);
    if (r != SOCKET_ERROR)
    {
      return b;              
    }
    if (WSAGetLastError() != WSAEWOULDBLOCK)
    {
      TranslateSocketError();
      return -1;
    }

       
                                                                           
                                                                       
                                                                          
                                                                          
                                 
       
    pg_usleep(10000);
  }
  ereport(NOTICE, (errmsg_internal("could not read from ready socket (after retries)")));
  errno = EWOULDBLOCK;
  return -1;
}

   
                                                                          
                                                                      
                     
   
                                                                            
                                                                              
          
   

int
pgwin32_send(SOCKET s, const void *buf, int len, int flags)
{
  WSABUF wbuf;
  int r;
  DWORD b;

  if (pgwin32_poll_signals())
  {
    return -1;
  }

  wbuf.len = len;
  wbuf.buf = (char *)buf;

     
                                                                            
                                                                
     
  for (;;)
  {
    r = WSASend(s, &wbuf, 1, &b, flags, NULL, NULL);
    if (r != SOCKET_ERROR && b > 0)
    {
                                      
      return b;
    }

    if (r == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK)
    {
      TranslateSocketError();
      return -1;
    }

    if (pgwin32_noblock)
    {
         
                                                                      
                                                                   
         
      errno = EWOULDBLOCK;
      return -1;
    }

                                                                         

    if (pgwin32_waitforsinglesocket(s, FD_WRITE | FD_CLOSE, INFINITE) == 0)
    {
      return -1;
    }
  }

  return -1;
}

   
                                             
                                       
   
                                                       
                                       
   
int
pgwin32_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, const struct timeval *timeout)
{
  WSAEVENT events[FD_SETSIZE * 2];                                  
                                                                  
                                                             
  SOCKET sockets[FD_SETSIZE * 2];
  int numevents = 0;
  int i;
  int r;
  DWORD timeoutval = WSA_INFINITE;
  FD_SET outreadfds;
  FD_SET outwritefds;
  int nummatches = 0;

  Assert(exceptfds == NULL);

  if (pgwin32_poll_signals())
  {
    return -1;
  }

  FD_ZERO(&outreadfds);
  FD_ZERO(&outwritefds);

     
                                                                            
                                                                       
                                                                            
                                                                            
                                                                       
                                                                            
                                                                          
                                                                             
     
  if (writefds != NULL)
  {
    for (i = 0; i < writefds->fd_count; i++)
    {
      char c;
      WSABUF buf;
      DWORD sent;

      buf.buf = &c;
      buf.len = 0;

      r = WSASend(writefds->fd_array[i], &buf, 1, &sent, 0, NULL, NULL);
      if (r == 0 || WSAGetLastError() != WSAEWOULDBLOCK)
      {
        FD_SET(writefds->fd_array[i], &outwritefds);
      }
    }

                                                                           
    if (outwritefds.fd_count > 0)
    {
      memcpy(writefds, &outwritefds, sizeof(fd_set));
      if (readfds)
      {
        FD_ZERO(readfds);
      }
      return outwritefds.fd_count;
    }
  }

                                       

  if (timeout != NULL)
  {
                                       
    timeoutval = timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
  }

  if (readfds != NULL)
  {
    for (i = 0; i < readfds->fd_count; i++)
    {
      events[numevents] = WSACreateEvent();
      sockets[numevents] = readfds->fd_array[i];
      numevents++;
    }
  }
  if (writefds != NULL)
  {
    for (i = 0; i < writefds->fd_count; i++)
    {
      if (!readfds || !FD_ISSET(writefds->fd_array[i], readfds))
      {
                                                   
        events[numevents] = WSACreateEvent();
        sockets[numevents] = writefds->fd_array[i];
        numevents++;
      }
    }
  }

  for (i = 0; i < numevents; i++)
  {
    int flags = 0;

    if (readfds && FD_ISSET(sockets[i], readfds))
    {
      flags |= FD_READ | FD_ACCEPT | FD_CLOSE;
    }

    if (writefds && FD_ISSET(sockets[i], writefds))
    {
      flags |= FD_WRITE | FD_CLOSE;
    }

    if (WSAEventSelect(sockets[i], events[i], flags) != 0)
    {
      TranslateSocketError();
                                                  
      while (--i >= 0)
      {
        WSAEventSelect(sockets[i], NULL, 0);
      }
      for (i = 0; i < numevents; i++)
      {
        WSACloseEvent(events[i]);
      }
      return -1;
    }
  }

  events[numevents] = pgwin32_signal_event;
  r = WaitForMultipleObjectsEx(numevents + 1, events, FALSE, timeoutval, TRUE);
  if (r != WAIT_TIMEOUT && r != WAIT_IO_COMPLETION && r != (WAIT_OBJECT_0 + numevents))
  {
       
                                                                           
                                                             
       
    WSANETWORKEVENTS resEvents;

    for (i = 0; i < numevents; i++)
    {
      ZeroMemory(&resEvents, sizeof(resEvents));
      if (WSAEnumNetworkEvents(sockets[i], events[i], &resEvents) != 0)
      {
        elog(ERROR, "failed to enumerate network events: error code %u", WSAGetLastError());
      }
                          
      if (readfds && FD_ISSET(sockets[i], readfds))
      {
        if ((resEvents.lNetworkEvents & FD_READ) || (resEvents.lNetworkEvents & FD_ACCEPT) || (resEvents.lNetworkEvents & FD_CLOSE))
        {
          FD_SET(sockets[i], &outreadfds);

          nummatches++;
        }
      }
                           
      if (writefds && FD_ISSET(sockets[i], writefds))
      {
        if ((resEvents.lNetworkEvents & FD_WRITE) || (resEvents.lNetworkEvents & FD_CLOSE))
        {
          FD_SET(sockets[i], &outwritefds);

          nummatches++;
        }
      }
    }
  }

                                      
  for (i = 0; i < numevents; i++)
  {
    WSAEventSelect(sockets[i], NULL, 0);
    WSACloseEvent(events[i]);
  }

  if (r == WSA_WAIT_TIMEOUT)
  {
    if (readfds)
    {
      FD_ZERO(readfds);
    }
    if (writefds)
    {
      FD_ZERO(writefds);
    }
    return 0;
  }

                           
  if (r == WAIT_OBJECT_0 + numevents || r == WAIT_IO_COMPLETION)
  {
    pgwin32_dispatch_queued_signals();
    errno = EINTR;
    if (readfds)
    {
      FD_ZERO(readfds);
    }
    if (writefds)
    {
      FD_ZERO(writefds);
    }
    return -1;
  }

                                                       
  if (readfds)
  {
    memcpy(readfds, &outreadfds, sizeof(fd_set));
  }
  if (writefds)
  {
    memcpy(writefds, &outwritefds, sizeof(fd_set));
  }
  return nummatches;
}
