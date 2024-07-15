                                                                            
   
           
                                        
   
                                                                              
                                                                              
                                                                            
                                                                        
                                                                               
                                                                               
                                                                             
                                                                            
                                                                             
                  
   
                                                                      
                                                                           
                                                                          
                                                                   
   
                                                                            
                                                                              
   
                                                                         
                                                                        
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#ifdef HAVE_SYS_EPOLL_H
#include <sys/epoll.h>
#endif
#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#include "miscadmin.h"
#include "pgstat.h"
#include "port/atomics.h"
#include "portability/instr_time.h"
#include "postmaster/postmaster.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/pmsignal.h"
#include "storage/shmem.h"

   
                                                                        
                                                                         
                                                                          
                                       
   
#if defined(WAIT_USE_EPOLL) || defined(WAIT_USE_POLL) || defined(WAIT_USE_WIN32)
                                   
#elif defined(HAVE_SYS_EPOLL_H)
#define WAIT_USE_EPOLL
#elif defined(HAVE_POLL)
#define WAIT_USE_POLL
#elif WIN32
#define WAIT_USE_WIN32
#else
#error "no wait set implementation available"
#endif

                        
struct WaitEventSet
{
  int nevents;                                        
  int nevents_space;                                           

     
                                                                           
                         
     
  WaitEvent *events;

     
                                                                           
                                                                         
                                                                            
                                  
     
  Latch *latch;
  int latch_pos;

     
                                                                            
                                                                            
                           
     
  bool exit_on_postmaster_death;

#if defined(WAIT_USE_EPOLL)
  int epoll_fd;
                                                                          
  struct epoll_event *epoll_ret_events;
#elif defined(WAIT_USE_POLL)
                                                                           
  struct pollfd *pollfds;
#elif defined(WAIT_USE_WIN32)

     
                                                                
                                                                             
                      
     
  HANDLE *handles;
#endif
};

#ifndef WIN32
                                                                           
static volatile sig_atomic_t waiting = false;

                                          
static int selfpipe_readfd = -1;
static int selfpipe_writefd = -1;

                                                                   
static int selfpipe_owner_pid = 0;

                                 
static void
sendSelfPipeByte(void);
static void
drainSelfPipe(void);
#endif            

#if defined(WAIT_USE_EPOLL)
static void
WaitEventAdjustEpoll(WaitEventSet *set, WaitEvent *event, int action);
#elif defined(WAIT_USE_POLL)
static void
WaitEventAdjustPoll(WaitEventSet *set, WaitEvent *event);
#elif defined(WAIT_USE_WIN32)
static void
WaitEventAdjustWin32(WaitEventSet *set, WaitEvent *event);
#endif

static inline int
WaitEventSetWaitBlock(WaitEventSet *set, int cur_timeout, WaitEvent *occurred_events, int nevents);

   
                                                      
   
                                                                           
                                                                  
   
void
InitializeLatchSupport(void)
{
#ifndef WIN32
  int pipefd[2];

  if (IsUnderPostmaster)
  {
       
                                                                         
                                                                        
                                                                   
                                        
       
    if (selfpipe_owner_pid != 0)
    {
                                                                 
      Assert(selfpipe_owner_pid != MyProcPid);
                                                           
      (void)close(selfpipe_readfd);
      (void)close(selfpipe_writefd);
                                                                   
      selfpipe_readfd = selfpipe_writefd = -1;
      selfpipe_owner_pid = 0;
    }
    else
    {
         
                                                                      
                                                                       
                                                                        
         
      Assert(selfpipe_readfd == -1);
    }
  }
  else
  {
                                                                         
    Assert(selfpipe_readfd == -1);
    Assert(selfpipe_owner_pid == 0);
  }

     
                                                                      
                                                                           
                                                                            
                                                                            
                                                                          
                                                                        
                                        
     
  if (pipe(pipefd) < 0)
  {
    elog(FATAL, "pipe() failed: %m");
  }
  if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) == -1)
  {
    elog(FATAL, "fcntl(F_SETFL) failed on read-end of self-pipe: %m");
  }
  if (fcntl(pipefd[1], F_SETFL, O_NONBLOCK) == -1)
  {
    elog(FATAL, "fcntl(F_SETFL) failed on write-end of self-pipe: %m");
  }
  if (fcntl(pipefd[0], F_SETFD, FD_CLOEXEC) == -1)
  {
    elog(FATAL, "fcntl(F_SETFD) failed on read-end of self-pipe: %m");
  }
  if (fcntl(pipefd[1], F_SETFD, FD_CLOEXEC) == -1)
  {
    elog(FATAL, "fcntl(F_SETFD) failed on write-end of self-pipe: %m");
  }

  selfpipe_readfd = pipefd[0];
  selfpipe_writefd = pipefd[1];
  selfpipe_owner_pid = MyProcPid;
#else
                                                 
#endif
}

   
                                     
   
void
InitLatch(Latch *latch)
{
  latch->is_set = false;
  latch->owner_pid = MyProcPid;
  latch->is_shared = false;

#ifndef WIN32
                                                                     
  Assert(selfpipe_readfd >= 0 && selfpipe_owner_pid == MyProcPid);
#else
  latch->event = CreateEvent(NULL, TRUE, FALSE, NULL);
  if (latch->event == NULL)
  {
    elog(ERROR, "CreateEvent failed: error code %lu", GetLastError());
  }
#endif            
}

   
                                                                             
                                                                       
                    
   
                                                                         
                                                                     
                                                                       
                                                                        
                                                                        
   
                                                                      
                                                                      
                                                                      
   
void
InitSharedLatch(Latch *latch)
{
#ifdef WIN32
  SECURITY_ATTRIBUTES sa;

     
                                                                          
     
  ZeroMemory(&sa, sizeof(sa));
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;

  latch->event = CreateEvent(&sa, TRUE, FALSE, NULL);
  if (latch->event == NULL)
  {
    elog(ERROR, "CreateEvent failed: error code %lu", GetLastError());
  }
#endif

  latch->is_set = false;
  latch->owner_pid = 0;
  latch->is_shared = true;
}

   
                                                                     
                      
   
                                                                         
                                                                            
                                                                          
                                                                              
   
                                                        
                                                                      
                                                                  
   
void
OwnLatch(Latch *latch)
{
                     
  Assert(latch->is_shared);

#ifndef WIN32
                                                                     
  Assert(selfpipe_readfd >= 0 && selfpipe_owner_pid == MyProcPid);
#endif

  if (latch->owner_pid != 0)
  {
    elog(ERROR, "latch already owned");
  }

  latch->owner_pid = MyProcPid;
}

   
                                                                 
   
void
DisownLatch(Latch *latch)
{
  Assert(latch->is_shared);
  Assert(latch->owner_pid == MyProcPid);

  latch->owner_pid = 0;
}

   
                                                                               
                                                                               
                                                                             
                                 
   
                                                                              
                                                                           
                                                                             
                                                                               
   
                                                                    
                                                                     
                                                            
   
                                                                           
                                                                            
                                                                       
   
int
WaitLatch(Latch *latch, int wakeEvents, long timeout, uint32 wait_event_info)
{
  return WaitLatchOrSocket(latch, wakeEvents, PGINVALID_SOCKET, timeout, wait_event_info);
}

   
                                                                     
               
   
                                                                              
                                                                              
                       
   
                                                                         
                                                                       
                                                                             
                                                            
   
                                                                           
                                                                    
                                                
   
int
WaitLatchOrSocket(Latch *latch, int wakeEvents, pgsocket sock, long timeout, uint32 wait_event_info)
{
  int ret = 0;
  int rc;
  WaitEvent event;
  WaitEventSet *set = CreateWaitEventSet(CurrentMemoryContext, 3);

  if (wakeEvents & WL_TIMEOUT)
  {
    Assert(timeout >= 0);
  }
  else
  {
    timeout = -1;
  }

  if (wakeEvents & WL_LATCH_SET)
  {
    AddWaitEventToSet(set, WL_LATCH_SET, PGINVALID_SOCKET, latch, NULL);
  }

                                                                        
  Assert(!IsUnderPostmaster || (wakeEvents & WL_EXIT_ON_PM_DEATH) || (wakeEvents & WL_POSTMASTER_DEATH));

  if ((wakeEvents & WL_POSTMASTER_DEATH) && IsUnderPostmaster)
  {
    AddWaitEventToSet(set, WL_POSTMASTER_DEATH, PGINVALID_SOCKET, NULL, NULL);
  }

  if ((wakeEvents & WL_EXIT_ON_PM_DEATH) && IsUnderPostmaster)
  {
    AddWaitEventToSet(set, WL_EXIT_ON_PM_DEATH, PGINVALID_SOCKET, NULL, NULL);
  }

  if (wakeEvents & WL_SOCKET_MASK)
  {
    int ev;

    ev = wakeEvents & WL_SOCKET_MASK;
    AddWaitEventToSet(set, ev, sock, NULL, NULL);
  }

  rc = WaitEventSetWait(set, timeout, &event, 1, wait_event_info);

  if (rc == 0)
  {
    ret |= WL_TIMEOUT;
  }
  else
  {
    ret |= event.events & (WL_LATCH_SET | WL_POSTMASTER_DEATH | WL_SOCKET_MASK);
  }

  FreeWaitEventSet(set);

  return ret;
}

   
                                                   
   
                                                                     
   
                                                                          
                                                                           
                                                                     
   
                                                                             
                                         
   
void
SetLatch(Latch *latch)
{
#ifndef WIN32
  pid_t owner_pid;
#else
  HANDLE handle;
#endif

     
                                                                      
                                                                          
                                         
     
  pg_memory_barrier();

                                 
  if (latch->is_set)
  {
    return;
  }

  latch->is_set = true;

#ifndef WIN32

     
                                                                             
                                                                    
                                                                       
             
     
                                                                          
                                                                            
                                                                           
                                                                         
                                                                      
                                                                    
                                                                      
                                          
     
                                                                      
                                                                            
                                                                            
                                                                             
                                                                         
                                             
     
  owner_pid = latch->owner_pid;
  if (owner_pid == 0)
  {
    return;
  }
  else if (owner_pid == MyProcPid)
  {
    if (waiting)
    {
      sendSelfPipeByte();
    }
  }
  else
  {
    kill(owner_pid, SIGUSR1);
  }
#else

     
                                                                             
                                
     
                                                                             
                                                    
     
  handle = latch->event;
  if (handle)
  {
    SetEvent(handle);

       
                                                                        
                                                                          
       
  }
#endif
}

   
                                                                    
                                                     
   
void
ResetLatch(Latch *latch)
{
                                             
  Assert(latch->owner_pid == MyProcPid);

  latch->is_set = false;

     
                                                                           
                                                                        
                                                                            
                                                                          
     
  pg_memory_barrier();
}

   
                                                                              
   
                                                                    
                       
   
WaitEventSet *
CreateWaitEventSet(MemoryContext context, int nevents)
{
  WaitEventSet *set;
  char *data;
  Size sz = 0;

     
                                                                            
                                                                             
                                                                        
                                                                      
     
  sz += MAXALIGN(sizeof(WaitEventSet));
  sz += MAXALIGN(sizeof(WaitEvent) * nevents);

#if defined(WAIT_USE_EPOLL)
  sz += MAXALIGN(sizeof(struct epoll_event) * nevents);
#elif defined(WAIT_USE_POLL)
  sz += MAXALIGN(sizeof(struct pollfd) * nevents);
#elif defined(WAIT_USE_WIN32)
                                               
  sz += MAXALIGN(sizeof(HANDLE) * (nevents + 1));
#endif

  data = (char *)MemoryContextAllocZero(context, sz);

  set = (WaitEventSet *)data;
  data += MAXALIGN(sizeof(WaitEventSet));

  set->events = (WaitEvent *)data;
  data += MAXALIGN(sizeof(WaitEvent) * nevents);

#if defined(WAIT_USE_EPOLL)
  set->epoll_ret_events = (struct epoll_event *)data;
  data += MAXALIGN(sizeof(struct epoll_event) * nevents);
#elif defined(WAIT_USE_POLL)
  set->pollfds = (struct pollfd *)data;
  data += MAXALIGN(sizeof(struct pollfd) * nevents);
#elif defined(WAIT_USE_WIN32)
  set->handles = (HANDLE)data;
  data += MAXALIGN(sizeof(HANDLE) * nevents);
#endif

  set->latch = NULL;
  set->nevents_space = nevents;
  set->exit_on_postmaster_death = false;

#if defined(WAIT_USE_EPOLL)
#ifdef EPOLL_CLOEXEC
  set->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
  if (set->epoll_fd < 0)
  {
    elog(ERROR, "epoll_create1 failed: %m");
  }
#else
                                                                   
  set->epoll_fd = epoll_create(nevents);
  if (set->epoll_fd < 0)
  {
    elog(ERROR, "epoll_create failed: %m");
  }
  if (fcntl(set->epoll_fd, F_SETFD, FD_CLOEXEC) == -1)
  {
    elog(ERROR, "fcntl(F_SETFD) failed on epoll descriptor: %m");
  }
#endif                    
#elif defined(WAIT_USE_WIN32)

     
                                                                             
                                                                           
                                           
     
                                                                          
                                                                       
                                   
     
  set->handles[0] = pgwin32_signal_event;
  StaticAssertStmt(WSA_INVALID_EVENT == NULL, "");
#endif

  return set;
}

   
                                           
   
                                                                             
                                                                               
                                                                             
                                                                             
                                 
   
void
FreeWaitEventSet(WaitEventSet *set)
{
#if defined(WAIT_USE_EPOLL)
  close(set->epoll_fd);
#elif defined(WAIT_USE_WIN32)
  WaitEvent *cur_event;

  for (cur_event = set->events; cur_event < (set->events + set->nevents); cur_event++)
  {
    if (cur_event->events & WL_LATCH_SET)
    {
                                   
    }
    else if (cur_event->events & WL_POSTMASTER_DEATH)
    {
                                 
    }
    else
    {
                                                               
      WSAEventSelect(cur_event->fd, NULL, 0);
      WSACloseEvent(set->handles[cur_event->pos + 1]);
    }
  }
#endif

  pfree(set);
}

       
                                                 
                                                
                                                     
                                                             
                                                               
                                                               
                                                  
                                                                        
                                                                  
                                                        
                                                                  
   
                                                                              
                                                                        
   
                                                                            
                                                                          
                                                                         
   
                                                                      
                                                                              
                                                   
   
                                                                            
                                                                            
           
   
int
AddWaitEventToSet(WaitEventSet *set, uint32 events, pgsocket fd, Latch *latch, void *user_data)
{
  WaitEvent *event;

                        
  Assert(set->nevents < set->nevents_space);

  if (events == WL_EXIT_ON_PM_DEATH)
  {
    events = WL_POSTMASTER_DEATH;
    set->exit_on_postmaster_death = true;
  }

  if (latch)
  {
    if (latch->owner_pid != MyProcPid)
    {
      elog(ERROR, "cannot wait on a latch owned by another process");
    }
    if (set->latch)
    {
      elog(ERROR, "cannot wait on more than one latch");
    }
    if ((events & WL_LATCH_SET) != WL_LATCH_SET)
    {
      elog(ERROR, "latch events only support being set");
    }
  }
  else
  {
    if (events & WL_LATCH_SET)
    {
      elog(ERROR, "cannot wait on latch without a specified latch");
    }
  }

                                                                     
  if (fd == PGINVALID_SOCKET && (events & WL_SOCKET_MASK))
  {
    elog(ERROR, "cannot wait on socket event without a socket");
  }

  event = &set->events[set->nevents];
  event->pos = set->nevents++;
  event->fd = fd;
  event->events = events;
  event->user_data = user_data;
#ifdef WIN32
  event->reset = false;
#endif

  if (events == WL_LATCH_SET)
  {
    set->latch = latch;
    set->latch_pos = event->pos;
#ifndef WIN32
    event->fd = selfpipe_readfd;
#endif
  }
  else if (events == WL_POSTMASTER_DEATH)
  {
#ifndef WIN32
    event->fd = postmaster_alive_fds[POSTMASTER_FD_WATCH];
#endif
  }

                                                                 
#if defined(WAIT_USE_EPOLL)
  WaitEventAdjustEpoll(set, event, EPOLL_CTL_ADD);
#elif defined(WAIT_USE_POLL)
  WaitEventAdjustPoll(set, event);
#elif defined(WAIT_USE_WIN32)
  WaitEventAdjustWin32(set, event);
#endif

  return event->pos;
}

   
                                                                             
                       
   
                                                  
   
void
ModifyWaitEvent(WaitEventSet *set, int pos, uint32 events, Latch *latch)
{
  WaitEvent *event;

  Assert(pos < set->nevents);

  event = &set->events[pos];

     
                                                                        
                                                                     
                                                                            
                        
     
  if (events == event->events && (!(event->events & WL_LATCH_SET) || set->latch == latch))
  {
    return;
  }

  if (event->events & WL_LATCH_SET && events != event->events)
  {
                                                            
    elog(ERROR, "cannot modify latch event");
  }

  if (event->events & WL_POSTMASTER_DEATH)
  {
    elog(ERROR, "cannot modify postmaster death event");
  }

                                  
  event->events = events;

  if (events == WL_LATCH_SET)
  {
    set->latch = latch;
  }

#if defined(WAIT_USE_EPOLL)
  WaitEventAdjustEpoll(set, event, EPOLL_CTL_MOD);
#elif defined(WAIT_USE_POLL)
  WaitEventAdjustPoll(set, event);
#elif defined(WAIT_USE_WIN32)
  WaitEventAdjustWin32(set, event);
#endif
}

#if defined(WAIT_USE_EPOLL)
   
                                                                      
   
static void
WaitEventAdjustEpoll(WaitEventSet *set, WaitEvent *event, int action)
{
  struct epoll_event epoll_ev;
  int rc;

                                                    
  epoll_ev.data.ptr = event;
                              
  epoll_ev.events = EPOLLERR | EPOLLHUP;

                                 
  if (event->events == WL_LATCH_SET)
  {
    Assert(set->latch != NULL);
    epoll_ev.events |= EPOLLIN;
  }
  else if (event->events == WL_POSTMASTER_DEATH)
  {
    epoll_ev.events |= EPOLLIN;
  }
  else
  {
    Assert(event->fd != PGINVALID_SOCKET);
    Assert(event->events & (WL_SOCKET_READABLE | WL_SOCKET_WRITEABLE));

    if (event->events & WL_SOCKET_READABLE)
    {
      epoll_ev.events |= EPOLLIN;
    }
    if (event->events & WL_SOCKET_WRITEABLE)
    {
      epoll_ev.events |= EPOLLOUT;
    }
  }

     
                                                                       
                                                                       
                                                               
     
  rc = epoll_ctl(set->epoll_fd, action, event->fd, &epoll_ev);

  if (rc < 0)
  {
    ereport(ERROR, (errcode_for_socket_access(),
                                                                               
                       errmsg("%s failed: %m", "epoll_ctl()")));
  }
}
#endif

#if defined(WAIT_USE_POLL)
static void
WaitEventAdjustPoll(WaitEventSet *set, WaitEvent *event)
{
  struct pollfd *pollfd = &set->pollfds[event->pos];

  pollfd->revents = 0;
  pollfd->fd = event->fd;

                                 
  if (event->events == WL_LATCH_SET)
  {
    Assert(set->latch != NULL);
    pollfd->events = POLLIN;
  }
  else if (event->events == WL_POSTMASTER_DEATH)
  {
    pollfd->events = POLLIN;
  }
  else
  {
    Assert(event->events & (WL_SOCKET_READABLE | WL_SOCKET_WRITEABLE));
    pollfd->events = 0;
    if (event->events & WL_SOCKET_READABLE)
    {
      pollfd->events |= POLLIN;
    }
    if (event->events & WL_SOCKET_WRITEABLE)
    {
      pollfd->events |= POLLOUT;
    }
  }

  Assert(event->fd != PGINVALID_SOCKET);
}
#endif

#if defined(WAIT_USE_WIN32)
static void
WaitEventAdjustWin32(WaitEventSet *set, WaitEvent *event)
{
  HANDLE *handle = &set->handles[event->pos + 1];

  if (event->events == WL_LATCH_SET)
  {
    Assert(set->latch != NULL);
    *handle = set->latch->event;
  }
  else if (event->events == WL_POSTMASTER_DEATH)
  {
    *handle = PostmasterHandle;
  }
  else
  {
    int flags = FD_CLOSE;                                  

    if (event->events & WL_SOCKET_READABLE)
    {
      flags |= FD_READ;
    }
    if (event->events & WL_SOCKET_WRITEABLE)
    {
      flags |= FD_WRITE;
    }
    if (event->events & WL_SOCKET_CONNECTED)
    {
      flags |= FD_CONNECT;
    }

    if (*handle == WSA_INVALID_EVENT)
    {
      *handle = WSACreateEvent();
      if (*handle == WSA_INVALID_EVENT)
      {
        elog(ERROR, "failed to create event for socket: error code %u", WSAGetLastError());
      }
    }
    if (WSAEventSelect(event->fd, *handle, flags) != 0)
    {
      elog(ERROR, "failed to set up event for socket: error code %u", WSAGetLastError());
    }

    Assert(event->fd != PGINVALID_SOCKET);
  }
}
#endif

   
                                                                       
                                                           
   
                                                                         
                                                                               
   
                                                                           
   
                                                                      
                                                
   
int
WaitEventSetWait(WaitEventSet *set, long timeout, WaitEvent *occurred_events, int nevents, uint32 wait_event_info)
{
  int returned_events = 0;
  instr_time start_time;
  instr_time cur_time;
  long cur_timeout = -1;

  Assert(nevents > 0);

     
                                                                          
                                                                 
     
  if (timeout >= 0)
  {
    INSTR_TIME_SET_CURRENT(start_time);
    Assert(timeout >= 0 && timeout <= INT_MAX);
    cur_timeout = timeout;
  }

  pgstat_report_wait_start(wait_event_info);

#ifndef WIN32
  waiting = true;
#else
                                                                     
  pgwin32_dispatch_queued_signals();
#endif
  while (returned_events == 0)
  {
    int rc;

       
                                                                
                                                                         
                                                  
       
                                                      
                                                                          
                                                                        
                                                  
       
                                                                         
                                                                     
                                                                         
                                                                      
                                                                       
                                                                         
                                                                
       
                                                                        
                                                                          
                                                                 
       
                                                                          
                                                                          
                                                                        
                                
       
    if (set->latch && set->latch->is_set)
    {
      occurred_events->fd = PGINVALID_SOCKET;
      occurred_events->pos = set->latch_pos;
      occurred_events->user_data = set->events[set->latch_pos].user_data;
      occurred_events->events = WL_LATCH_SET;
      occurred_events++;
      returned_events++;

      break;
    }

       
                                                                          
                                                                          
                                                                   
       
    rc = WaitEventSetWaitBlock(set, cur_timeout, occurred_events, nevents);

    if (rc == -1)
    {
      break;                       
    }
    else
    {
      returned_events = rc;
    }

                                                                  
    if (returned_events == 0 && timeout >= 0)
    {
      INSTR_TIME_SET_CURRENT(cur_time);
      INSTR_TIME_SUBTRACT(cur_time, start_time);
      cur_timeout = timeout - (long)INSTR_TIME_GET_MILLISEC(cur_time);
      if (cur_timeout <= 0)
      {
        break;
      }
    }
  }
#ifndef WIN32
  waiting = false;
#endif

  pgstat_report_wait_end();

  return returned_events;
}

#if defined(WAIT_USE_EPOLL)

   
                                     
   
                                                                              
                                                                               
                                                                          
         
   
static inline int
WaitEventSetWaitBlock(WaitEventSet *set, int cur_timeout, WaitEvent *occurred_events, int nevents)
{
  int returned_events = 0;
  int rc;
  WaitEvent *cur_event;
  struct epoll_event *cur_epoll_event;

             
  rc = epoll_wait(set->epoll_fd, set->epoll_ret_events, Min(nevents, set->nevents_space), cur_timeout);

                         
  if (rc < 0)
  {
                                           
    if (errno != EINTR)
    {
      waiting = false;
      ereport(ERROR, (errcode_for_socket_access(),
                                                                                 
                         errmsg("%s failed: %m", "epoll_wait()")));
    }
    return 0;
  }
  else if (rc == 0)
  {
                          
    return -1;
  }

     
                                                                         
                                                                          
                         
     
  for (cur_epoll_event = set->epoll_ret_events; cur_epoll_event < (set->epoll_ret_events + rc) && returned_events < nevents; cur_epoll_event++)
  {
                                                                 
    cur_event = (WaitEvent *)cur_epoll_event->data.ptr;

    occurred_events->pos = cur_event->pos;
    occurred_events->user_data = cur_event->user_data;
    occurred_events->events = 0;

    if (cur_event->events == WL_LATCH_SET && cur_epoll_event->events & (EPOLLIN | EPOLLERR | EPOLLHUP))
    {
                                                    
      drainSelfPipe();

      if (set->latch->is_set)
      {
        occurred_events->fd = PGINVALID_SOCKET;
        occurred_events->events = WL_LATCH_SET;
        occurred_events++;
        returned_events++;
      }
    }
    else if (cur_event->events == WL_POSTMASTER_DEATH && cur_epoll_event->events & (EPOLLIN | EPOLLERR | EPOLLHUP))
    {
         
                                                                  
                                                                        
                                                                        
         
                                                                         
                                                                        
                                                                     
                                                                         
                    
         
      if (!PostmasterIsAliveInternal())
      {
        if (set->exit_on_postmaster_death)
        {
          proc_exit(1);
        }
        occurred_events->fd = PGINVALID_SOCKET;
        occurred_events->events = WL_POSTMASTER_DEATH;
        occurred_events++;
        returned_events++;
      }
    }
    else if (cur_event->events & (WL_SOCKET_READABLE | WL_SOCKET_WRITEABLE))
    {
      Assert(cur_event->fd != PGINVALID_SOCKET);

      if ((cur_event->events & WL_SOCKET_READABLE) && (cur_epoll_event->events & (EPOLLIN | EPOLLERR | EPOLLHUP)))
      {
                                              
        occurred_events->events |= WL_SOCKET_READABLE;
      }

      if ((cur_event->events & WL_SOCKET_WRITEABLE) && (cur_epoll_event->events & (EPOLLOUT | EPOLLERR | EPOLLHUP)))
      {
                              
        occurred_events->events |= WL_SOCKET_WRITEABLE;
      }

      if (occurred_events->events != 0)
      {
        occurred_events->fd = cur_event->fd;
        occurred_events++;
        returned_events++;
      }
    }
  }

  return returned_events;
}

#elif defined(WAIT_USE_POLL)

   
                       
   
                                                                              
                                                       
   
static inline int
WaitEventSetWaitBlock(WaitEventSet *set, int cur_timeout, WaitEvent *occurred_events, int nevents)
{
  int returned_events = 0;
  int rc;
  WaitEvent *cur_event;
  struct pollfd *cur_pollfd;

             
  rc = poll(set->pollfds, set->nevents, (int)cur_timeout);

                         
  if (rc < 0)
  {
                                           
    if (errno != EINTR)
    {
      waiting = false;
      ereport(ERROR, (errcode_for_socket_access(),
                                                                                 
                         errmsg("%s failed: %m", "poll()")));
    }
    return 0;
  }
  else if (rc == 0)
  {
                          
    return -1;
  }

  for (cur_event = set->events, cur_pollfd = set->pollfds; cur_event < (set->events + set->nevents) && returned_events < nevents; cur_event++, cur_pollfd++)
  {
                                      
    if (cur_pollfd->revents == 0)
    {
      continue;
    }

    occurred_events->pos = cur_event->pos;
    occurred_events->user_data = cur_event->user_data;
    occurred_events->events = 0;

    if (cur_event->events == WL_LATCH_SET && (cur_pollfd->revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)))
    {
                                                    
      drainSelfPipe();

      if (set->latch->is_set)
      {
        occurred_events->fd = PGINVALID_SOCKET;
        occurred_events->events = WL_LATCH_SET;
        occurred_events++;
        returned_events++;
      }
    }
    else if (cur_event->events == WL_POSTMASTER_DEATH && (cur_pollfd->revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)))
    {
         
                                                                         
                                                                    
                                                                    
         
                                                                         
                                                                        
                                                                     
                                                                         
                    
         
      if (!PostmasterIsAliveInternal())
      {
        if (set->exit_on_postmaster_death)
        {
          proc_exit(1);
        }
        occurred_events->fd = PGINVALID_SOCKET;
        occurred_events->events = WL_POSTMASTER_DEATH;
        occurred_events++;
        returned_events++;
      }
    }
    else if (cur_event->events & (WL_SOCKET_READABLE | WL_SOCKET_WRITEABLE))
    {
      int errflags = POLLHUP | POLLERR | POLLNVAL;

      Assert(cur_event->fd >= PGINVALID_SOCKET);

      if ((cur_event->events & WL_SOCKET_READABLE) && (cur_pollfd->revents & (POLLIN | errflags)))
      {
                                              
        occurred_events->events |= WL_SOCKET_READABLE;
      }

      if ((cur_event->events & WL_SOCKET_WRITEABLE) && (cur_pollfd->revents & (POLLOUT | errflags)))
      {
                               
        occurred_events->events |= WL_SOCKET_WRITEABLE;
      }

      if (occurred_events->events != 0)
      {
        occurred_events->fd = cur_event->fd;
        occurred_events++;
        returned_events++;
      }
    }
  }
  return returned_events;
}

#elif defined(WAIT_USE_WIN32)

   
                                                 
   
                                                                               
                                                           
                                                                              
                                        
                                                                              
                                      
   
static inline int
WaitEventSetWaitBlock(WaitEventSet *set, int cur_timeout, WaitEvent *occurred_events, int nevents)
{
  int returned_events = 0;
  DWORD rc;
  WaitEvent *cur_event;

                                          
  for (cur_event = set->events; cur_event < (set->events + set->nevents); cur_event++)
  {
    if (cur_event->reset)
    {
      WaitEventAdjustWin32(set, cur_event);
      cur_event->reset = false;
    }

       
                                                                   
                                                                        
                                                                          
                                                                           
                                                                           
                                                                           
                                                                        
                                                                         
                                      
       
    if (cur_event->events & WL_SOCKET_WRITEABLE)
    {
      char c;
      WSABUF buf;
      DWORD sent;
      int r;

      buf.buf = &c;
      buf.len = 0;

      r = WSASend(cur_event->fd, &buf, 1, &sent, 0, NULL, NULL);
      if (r == 0 || WSAGetLastError() != WSAEWOULDBLOCK)
      {
        occurred_events->pos = cur_event->pos;
        occurred_events->user_data = cur_event->user_data;
        occurred_events->events = WL_SOCKET_WRITEABLE;
        occurred_events->fd = cur_event->fd;
        return 1;
      }
    }
  }

     
            
     
                                                                      
     
  rc = WaitForMultipleObjects(set->nevents + 1, set->handles, FALSE, cur_timeout);

                         
  if (rc == WAIT_FAILED)
  {
    elog(ERROR, "WaitForMultipleObjects() failed: error code %lu", GetLastError());
  }
  else if (rc == WAIT_TIMEOUT)
  {
                          
    return -1;
  }

  if (rc == WAIT_OBJECT_0)
  {
                                       
    pgwin32_dispatch_queued_signals();
    return 0;            
  }

     
                                                                            
                                                             
     
  cur_event = (WaitEvent *)&set->events[rc - WAIT_OBJECT_0 - 1];

  occurred_events->pos = cur_event->pos;
  occurred_events->user_data = cur_event->user_data;
  occurred_events->events = 0;

  if (cur_event->events == WL_LATCH_SET)
  {
    if (!ResetEvent(set->latch->event))
    {
      elog(ERROR, "ResetEvent failed: error code %lu", GetLastError());
    }

    if (set->latch->is_set)
    {
      occurred_events->fd = PGINVALID_SOCKET;
      occurred_events->events = WL_LATCH_SET;
      occurred_events++;
      returned_events++;
    }
  }
  else if (cur_event->events == WL_POSTMASTER_DEATH)
  {
       
                                                                      
                                                                         
                                                                       
                                                                          
                                  
       
    if (!PostmasterIsAliveInternal())
    {
      if (set->exit_on_postmaster_death)
      {
        proc_exit(1);
      }
      occurred_events->fd = PGINVALID_SOCKET;
      occurred_events->events = WL_POSTMASTER_DEATH;
      occurred_events++;
      returned_events++;
    }
  }
  else if (cur_event->events & WL_SOCKET_MASK)
  {
    WSANETWORKEVENTS resEvents;
    HANDLE handle = set->handles[cur_event->pos + 1];

    Assert(cur_event->fd);

    occurred_events->fd = cur_event->fd;

    ZeroMemory(&resEvents, sizeof(resEvents));
    if (WSAEnumNetworkEvents(cur_event->fd, handle, &resEvents) != 0)
    {
      elog(ERROR, "failed to enumerate network events: error code %u", WSAGetLastError());
    }
    if ((cur_event->events & WL_SOCKET_READABLE) && (resEvents.lNetworkEvents & FD_READ))
    {
                                    
      occurred_events->events |= WL_SOCKET_READABLE;

               
                                                                         
                                                                       
                                                                       
                                                                        
                                                                    
                                                                                                      
                                            
               
         
      cur_event->reset = true;
    }
    if ((cur_event->events & WL_SOCKET_WRITEABLE) && (resEvents.lNetworkEvents & FD_WRITE))
    {
                     
      occurred_events->events |= WL_SOCKET_WRITEABLE;
    }
    if ((cur_event->events & WL_SOCKET_CONNECTED) && (resEvents.lNetworkEvents & FD_CONNECT))
    {
                     
      occurred_events->events |= WL_SOCKET_CONNECTED;
    }
    if (resEvents.lNetworkEvents & FD_CLOSE)
    {
                                                                  
      occurred_events->events |= (cur_event->events & WL_SOCKET_MASK);
    }

    if (occurred_events->events != 0)
    {
      occurred_events++;
      returned_events++;
    }
  }

  return returned_events;
}
#endif

   
                                                                      
   
                                                                            
                                                                            
                                                              
   
                                                                          
                    
   
#ifndef WIN32
void
latch_sigusr1_handler(void)
{
  if (waiting)
  {
    sendSelfPipeByte();
  }
}
#endif             

                                                          
#ifndef WIN32
static void
sendSelfPipeByte(void)
{
  int rc;
  char dummy = 0;

retry:
  rc = write(selfpipe_writefd, &dummy, 1);
  if (rc < 0)
  {
                                              
    if (errno == EINTR)
    {
      goto retry;
    }

       
                                                                          
                                               
       
    if (errno == EAGAIN || errno == EWOULDBLOCK)
    {
      return;
    }

       
                                                                        
                                                                         
                                  
       
    return;
  }
}
#endif             

   
                                              
   
                                                                           
                                                                          
            
   
#ifndef WIN32
static void
drainSelfPipe(void)
{
     
                                                                            
                                                                       
     
  char buf[16];
  int rc;

  for (;;)
  {
    rc = read(selfpipe_readfd, buf, sizeof(buf));
    if (rc < 0)
    {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
      {
        break;                        
      }
      else if (errno == EINTR)
      {
        continue;            
      }
      else
      {
        waiting = false;
        elog(ERROR, "read() on self-pipe failed: %m");
      }
    }
    else if (rc == 0)
    {
      waiting = false;
      elog(ERROR, "unexpected EOF on self-pipe");
    }
    else if (rc < sizeof(buf))
    {
                                                                     
      break;
    }
                                                      
  }
}
#endif             
