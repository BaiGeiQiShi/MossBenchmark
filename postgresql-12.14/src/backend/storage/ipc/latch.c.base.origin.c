/*-------------------------------------------------------------------------
 *
 * latch.c
 *	  Routines for inter-process latches
 *
 * The Unix implementation uses the so-called self-pipe trick to overcome the
 * race condition involved with poll() (or epoll_wait() on linux) and setting
 * a global flag in the signal handler. When a latch is set and the current
 * process is waiting for it, the signal handler wakes up the poll() in
 * WaitLatch by writing a byte to a pipe. A signal by itself doesn't interrupt
 * poll() on all platforms, and even on platforms where it does, a signal that
 * arrives just before the poll() call does not prevent poll() from entering
 * sleep. An incoming byte on a pipe however reliably interrupts the sleep,
 * and causes poll() to return immediately even if the signal arrives before
 * poll() begins.
 *
 * When SetLatch is called from the same process that owns the latch,
 * SetLatch writes the byte directly to the pipe. If it's owned by another
 * process, SIGUSR1 is sent and the signal handler in the waiting process
 * writes the byte to the pipe on behalf of the signaling process.
 *
 * The Windows implementation uses Windows events that are inherited by all
 * postmaster child processes. There's no need for the self-pipe trick there.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/storage/ipc/latch.c
 *
 *-------------------------------------------------------------------------
 */
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

/*
 * Select the fd readiness primitive to use. Normally the "most modern"
 * primitive supported by the OS will be used, but for testing it can be
 * useful to manually specify the used primitive.  If desired, just add a
 * define somewhere before this block.
 */
#if defined(WAIT_USE_EPOLL) || defined(WAIT_USE_POLL) || defined(WAIT_USE_WIN32)
/* don't overwrite manual choice */
#elif defined(HAVE_SYS_EPOLL_H)
#define WAIT_USE_EPOLL
#elif defined(HAVE_POLL)
#define WAIT_USE_POLL
#elif WIN32
#define WAIT_USE_WIN32
#else
#error "no wait set implementation available"
#endif

/* typedef in latch.h */
struct WaitEventSet
{
  int nevents;       /* number of registered events */
  int nevents_space; /* maximum number of events in this set */

  /*
   * Array, of nevents_space length, storing the definition of events this
   * set is waiting for.
   */
  WaitEvent *events;

  /*
   * If WL_LATCH_SET is specified in any wait event, latch is a pointer to
   * said latch, and latch_pos the offset in the ->events array. This is
   * useful because we check the state of the latch before performing doing
   * syscalls related to waiting.
   */
  Latch *latch;
  int latch_pos;

  /*
   * WL_EXIT_ON_PM_DEATH is converted to WL_POSTMASTER_DEATH, but this flag
   * is set so that we'll exit immediately if postmaster death is detected,
   * instead of returning.
   */
  bool exit_on_postmaster_death;

#if defined(WAIT_USE_EPOLL)
  int epoll_fd;
  /* epoll_wait returns events in a user provided arrays, allocate once */
  struct epoll_event *epoll_ret_events;
#elif defined(WAIT_USE_POLL)
  /* poll expects events to be waited on every poll() call, prepare once */
  struct pollfd *pollfds;
#elif defined(WAIT_USE_WIN32)

  /*
   * Array of windows events. The first element always contains
   * pgwin32_signal_event, so the remaining elements are offset by one (i.e.
   * event->pos + 1).
   */
  HANDLE *handles;
#endif
};

#ifndef WIN32
/* Are we currently in WaitLatch? The signal handler would like to know. */
static volatile sig_atomic_t waiting = false;

/* Read and write ends of the self-pipe */
static int selfpipe_readfd = -1;
static int selfpipe_writefd = -1;

/* Process owning the self-pipe --- needed for checking purposes */
static int selfpipe_owner_pid = 0;

/* Private function prototypes */
static void
sendSelfPipeByte(void);
static void
drainSelfPipe(void);
#endif /* WIN32 */

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

/*
 * Initialize the process-local latch infrastructure.
 *
 * This must be called once during startup of any process that can wait on
 * latches, before it issues any InitLatch() or OwnLatch() calls.
 */
void
InitializeLatchSupport(void)
{











































































}

/*
 * Initialize a process-local latch.
 */
void
InitLatch(Latch *latch)
{














}

/*
 * Initialize a shared latch that can be set from other processes. The latch
 * is initially owned by no-one; use OwnLatch to associate it with the
 * current process.
 *
 * InitSharedLatch needs to be called in postmaster before forking child
 * processes, usually right after allocating the shared memory block
 * containing the latch with ShmemInitStruct. (The Unix implementation
 * doesn't actually require that, but the Windows one does.) Because of
 * this restriction, we have no concurrency issues to worry about here.
 *
 * Note that other handles created in this module are never marked as
 * inheritable.  Thus we do not need to worry about cleaning up child
 * process references to postmaster-private latches or WaitEventSets.
 */
void
InitSharedLatch(Latch *latch)
{




















}

/*
 * Associate a shared latch with the current process, allowing it to
 * wait on the latch.
 *
 * Although there is a sanity check for latch-already-owned, we don't do
 * any sort of locking here, meaning that we could fail to detect the error
 * if two processes try to own the same latch at about the same time.  If
 * there is any risk of that, caller must provide an interlock to prevent it.
 *
 * In any process that calls OwnLatch(), make sure that
 * latch_sigusr1_handler() is called from the SIGUSR1 signal handler,
 * as shared latches use SIGUSR1 for inter-process communication.
 */
void
OwnLatch(Latch *latch)
{














}

/*
 * Disown a shared latch currently owned by the current process.
 */
void
DisownLatch(Latch *latch)
{




}

/*
 * Wait for a given latch to be set, or for postmaster death, or until timeout
 * is exceeded. 'wakeEvents' is a bitmask that specifies which of those events
 * to wait for. If the latch is already set (and WL_LATCH_SET is given), the
 * function returns immediately.
 *
 * The "timeout" is given in milliseconds. It must be >= 0 if WL_TIMEOUT flag
 * is given.  Although it is declared as "long", we don't actually support
 * timeouts longer than INT_MAX milliseconds.  Note that some extra overhead
 * is incurred when WL_TIMEOUT is given, so avoid using a timeout if possible.
 *
 * The latch must be owned by the current process, ie. it must be a
 * process-local latch initialized with InitLatch, or a shared latch
 * associated with the current process by calling OwnLatch.
 *
 * Returns bit mask indicating which condition(s) caused the wake-up. Note
 * that if multiple wake-up conditions are true, there is no guarantee that
 * we return all of them in one call, but we will return at least one.
 */
int
WaitLatch(Latch *latch, int wakeEvents, long timeout, uint32 wait_event_info)
{

}

/*
 * Like WaitLatch, but with an extra socket argument for WL_SOCKET_*
 * conditions.
 *
 * When waiting on a socket, EOF and error conditions always cause the socket
 * to be reported as readable/writable/connected, so that the caller can deal
 * with the condition.
 *
 * wakeEvents must include either WL_EXIT_ON_PM_DEATH for automatic exit
 * if the postmaster dies or WL_POSTMASTER_DEATH for a flag set in the
 * return value if the postmaster dies.  The latter is useful for rare cases
 * where some behavior other than immediate exit is needed.
 *
 * NB: These days this is just a wrapper around the WaitEventSet API. When
 * using a latch very frequently, consider creating a longer living
 * WaitEventSet instead; that's more efficient.
 */
int
WaitLatchOrSocket(Latch *latch, int wakeEvents, pgsocket sock, long timeout, uint32 wait_event_info)
{






















































}

/*
 * Sets a latch and wakes up anyone waiting on it.
 *
 * This is cheap if the latch is already set, otherwise not so much.
 *
 * NB: when calling this in a signal handler, be sure to save and restore
 * errno around it.  (That's standard practice in most signal handlers, of
 * course, but we used to omit it in handlers that only set a flag.)
 *
 * NB: this function is called from critical sections and signal handlers so
 * throwing an error is not a good idea.
 */
void
SetLatch(Latch *latch)
{

















































































}

/*
 * Clear the latch. Calling WaitLatch after this will sleep, unless
 * the latch is set again before the WaitLatch call.
 */
void
ResetLatch(Latch *latch)
{












}

/*
 * Create a WaitEventSet with space for nevents different events to wait for.
 *
 * These events can then be efficiently waited upon together, using
 * WaitEventSetWait().
 */
WaitEventSet *
CreateWaitEventSet(MemoryContext context, int nevents)
{
















































































}

/*
 * Free a previously created WaitEventSet.
 *
 * Note: preferably, this shouldn't have to free any resources that could be
 * inherited across an exec().  If it did, we'd likely leak those resources in
 * many scenarios.  For the epoll case, we ensure that by setting FD_CLOEXEC
 * when the FD is created.  For the Windows case, we assume that the handles
 * involved are non-inheritable.
 */
void
FreeWaitEventSet(WaitEventSet *set)
{

























}

/* ---
 * Add an event to the set. Possible events are:
 * - WL_LATCH_SET: Wait for the latch to be set
 * - WL_POSTMASTER_DEATH: Wait for postmaster to die
 * - WL_SOCKET_READABLE: Wait for socket to become readable,
 *	 can be combined in one event with other WL_SOCKET_* events
 * - WL_SOCKET_WRITEABLE: Wait for socket to become writeable,
 *	 can be combined with other WL_SOCKET_* events
 * - WL_SOCKET_CONNECTED: Wait for socket connection to be established,
 *	 can be combined with other WL_SOCKET_* events (on non-Windows
 *	 platforms, this is the same as WL_SOCKET_WRITEABLE)
 * - WL_EXIT_ON_PM_DEATH: Exit immediately if the postmaster dies
 *
 * Returns the offset in WaitEventSet->events (starting from 0), which can be
 * used to modify previously added wait events using ModifyWaitEvent().
 *
 * In the WL_LATCH_SET case the latch must be owned by the current process,
 * i.e. it must be a process-local latch initialized with InitLatch, or a
 * shared latch associated with the current process by calling OwnLatch.
 *
 * In the WL_SOCKET_READABLE/WRITEABLE/CONNECTED cases, EOF and error
 * conditions cause the socket to be reported as readable/writable/connected,
 * so that the caller can deal with the condition.
 *
 * The user_data pointer specified here will be set for the events returned
 * by WaitEventSetWait(), allowing to easily associate additional data with
 * events.
 */
int
AddWaitEventToSet(WaitEventSet *set, uint32 events, pgsocket fd, Latch *latch, void *user_data)
{










































































}

/*
 * Change the event mask and, in the WL_LATCH_SET case, the latch associated
 * with the WaitEvent.
 *
 * 'pos' is the id returned by AddWaitEventToSet.
 */
void
ModifyWaitEvent(WaitEventSet *set, int pos, uint32 events, Latch *latch)
{











































}

#if defined(WAIT_USE_EPOLL)
/*
 * action can be one of EPOLL_CTL_ADD | EPOLL_CTL_MOD | EPOLL_CTL_DEL
 */
static void
WaitEventAdjustEpoll(WaitEventSet *set, WaitEvent *event, int action)
{












































}
#endif

#if defined(WAIT_USE_POLL)
static void
WaitEventAdjustPoll(WaitEventSet *set, WaitEvent *event)
{
  struct pollfd *pollfd = &set->pollfds[event->pos];

  pollfd->revents = 0;
  pollfd->fd = event->fd;

  /* prepare pollfd entry once */
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
    int flags = FD_CLOSE; /* always check for errors/EOF */

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

/*
 * Wait for events added to the set to happen, or until the timeout is
 * reached.  At most nevents occurred events are returned.
 *
 * If timeout = -1, block until an event occurs; if 0, check sockets for
 * readiness, but don't block; if > 0, block for at most timeout milliseconds.
 *
 * Returns the number of events occurred, or 0 if the timeout was reached.
 *
 * Returned events will have the fd, pos, user_data fields set to the
 * values associated with the registered event.
 */
int
WaitEventSetWait(WaitEventSet *set, long timeout, WaitEvent *occurred_events, int nevents, uint32 wait_event_info)
{








































































































}

#if defined(WAIT_USE_EPOLL)

/*
 * Wait using linux's epoll_wait(2).
 *
 * This is the preferable wait method, as several readiness notifications are
 * delivered, without having to iterate through all of set->events. The return
 * epoll_event struct contain a pointer to our events, making association
 * easy.
 */
static inline int
WaitEventSetWaitBlock(WaitEventSet *set, int cur_timeout, WaitEvent *occurred_events, int nevents)
{







































































































}

#elif defined(WAIT_USE_POLL)

/*
 * Wait using poll(2).
 *
 * This allows to receive readiness notifications for several events at once,
 * but requires iterating through all of set->pollfds.
 */
static inline int
WaitEventSetWaitBlock(WaitEventSet *set, int cur_timeout, WaitEvent *occurred_events, int nevents)
{
  int returned_events = 0;
  int rc;
  WaitEvent *cur_event;
  struct pollfd *cur_pollfd;

  /* Sleep */
  rc = poll(set->pollfds, set->nevents, (int)cur_timeout);

  /* Check return code */
  if (rc < 0)
  {
    /* EINTR is okay, otherwise complain */
    if (errno != EINTR)
    {
      waiting = false;
      ereport(ERROR, (errcode_for_socket_access(), errmsg("%s failed: %m", "poll()")));
    }
    return 0;
  }
  else if (rc == 0)
  {
    /* timeout exceeded */
    return -1;
  }

  for (cur_event = set->events, cur_pollfd = set->pollfds; cur_event < (set->events + set->nevents) && returned_events < nevents; cur_event++, cur_pollfd++)
  {
    /* no activity on this FD, skip */
    if (cur_pollfd->revents == 0)
    {
      continue;
    }

    occurred_events->pos = cur_event->pos;
    occurred_events->user_data = cur_event->user_data;
    occurred_events->events = 0;

    if (cur_event->events == WL_LATCH_SET && (cur_pollfd->revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL)))
    {
      /* There's data in the self-pipe, clear it. */
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
      /*
       * We expect an POLLHUP when the remote end is closed, but because
       * we don't expect the pipe to become readable or to have any
       * errors either, treat those cases as postmaster death, too.
       *
       * Be paranoid about a spurious event signalling the postmaster as
       * being dead.  There have been reports about that happening with
       * older primitives (select(2) to be specific), and a spurious
       * WL_POSTMASTER_DEATH event would be painful. Re-checking doesn't
       * cost much.
       */
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
        /* data available in socket, or EOF */
        occurred_events->events |= WL_SOCKET_READABLE;
      }

      if ((cur_event->events & WL_SOCKET_WRITEABLE) && (cur_pollfd->revents & (POLLOUT | errflags)))
      {
        /* writeable, or EOF */
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

/*
 * Wait using Windows' WaitForMultipleObjects().
 *
 * Unfortunately this will only ever return a single readiness notification at
 * a time.  Note that while the official documentation for
 * WaitForMultipleObjects is ambiguous about multiple events being "consumed"
 * with a single bWaitAll = FALSE call,
 * https://blogs.msdn.microsoft.com/oldnewthing/20150409-00/?p=44273 confirms
 * that only one event is "consumed".
 */
static inline int
WaitEventSetWaitBlock(WaitEventSet *set, int cur_timeout, WaitEvent *occurred_events, int nevents)
{
  int returned_events = 0;
  DWORD rc;
  WaitEvent *cur_event;

  /* Reset any wait events that need it */
  for (cur_event = set->events; cur_event < (set->events + set->nevents); cur_event++)
  {
    if (cur_event->reset)
    {
      WaitEventAdjustWin32(set, cur_event);
      cur_event->reset = false;
    }

    /*
     * Windows does not guarantee to log an FD_WRITE network event
     * indicating that more data can be sent unless the previous send()
     * failed with WSAEWOULDBLOCK.  While our caller might well have made
     * such a call, we cannot assume that here.  Therefore, if waiting for
     * write-ready, force the issue by doing a dummy send().  If the dummy
     * send() succeeds, assume that the socket is in fact write-ready, and
     * return immediately.  Also, if it fails with something other than
     * WSAEWOULDBLOCK, return a write-ready indication to let our caller
     * deal with the error condition.
     */
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

  /*
   * Sleep.
   *
   * Need to wait for ->nevents + 1, because signal handle is in [0].
   */
  rc = WaitForMultipleObjects(set->nevents + 1, set->handles, FALSE, cur_timeout);

  /* Check return code */
  if (rc == WAIT_FAILED)
  {
    elog(ERROR, "WaitForMultipleObjects() failed: error code %lu", GetLastError());
  }
  else if (rc == WAIT_TIMEOUT)
  {
    /* timeout exceeded */
    return -1;
  }

  if (rc == WAIT_OBJECT_0)
  {
    /* Service newly-arrived signals */
    pgwin32_dispatch_queued_signals();
    return 0; /* retry */
  }

  /*
   * With an offset of one, due to the always present pgwin32_signal_event,
   * the handle offset directly corresponds to a wait event.
   */
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
    /*
     * Postmaster apparently died.  Since the consequences of falsely
     * returning WL_POSTMASTER_DEATH could be pretty unpleasant, we take
     * the trouble to positively verify this with PostmasterIsAlive(),
     * even though there is no known reason to think that the event could
     * be falsely set on Windows.
     */
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
      /* data available in socket */
      occurred_events->events |= WL_SOCKET_READABLE;

      /*------
       * WaitForMultipleObjects doesn't guarantee that a read event will
       * be returned if the latch is set at the same time.  Even if it
       * did, the caller might drop that event expecting it to reoccur
       * on next call.  So, we must force the event to be reset if this
       * WaitEventSet is used again in order to avoid an indefinite
       * hang.  Refer
       *https://msdn.microsoft.com/en-us/library/windows/desktop/ms741576(v=vs.85).aspx
       * for the behavior of socket events.
       *------
       */
      cur_event->reset = true;
    }
    if ((cur_event->events & WL_SOCKET_WRITEABLE) && (resEvents.lNetworkEvents & FD_WRITE))
    {
      /* writeable */
      occurred_events->events |= WL_SOCKET_WRITEABLE;
    }
    if ((cur_event->events & WL_SOCKET_CONNECTED) && (resEvents.lNetworkEvents & FD_CONNECT))
    {
      /* connected */
      occurred_events->events |= WL_SOCKET_CONNECTED;
    }
    if (resEvents.lNetworkEvents & FD_CLOSE)
    {
      /* EOF/error, so signal all caller-requested socket flags */
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

/*
 * SetLatch uses SIGUSR1 to wake up the process waiting on the latch.
 *
 * Wake up WaitLatch, if we're waiting.  (We might not be, since SIGUSR1 is
 * overloaded for multiple purposes; or we might not have reached WaitLatch
 * yet, in which case we don't need to fill the pipe either.)
 *
 * NB: when calling this in a signal handler, be sure to save and restore
 * errno around it.
 */
#ifndef WIN32
void
latch_sigusr1_handler(void)
{




}
#endif /* !WIN32 */

/* Send one byte to the self-pipe, to wake up WaitLatch */
#ifndef WIN32
static void
sendSelfPipeByte(void)
{





























}
#endif /* !WIN32 */

/*
 * Read all available data from the self-pipe
 *
 * Note: this is only called when waiting = true.  If it fails and doesn't
 * return, it must reset that flag first (though ideally, this will never
 * happen).
 */
#ifndef WIN32
static void
drainSelfPipe(void)
{






































}
#endif /* !WIN32 */