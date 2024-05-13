/*-------------------------------------------------------------------------
 *
 * startup.c
 *
 * The Startup process initialises the server and performs any recovery
 * actions that have been specified. Notice that there is no "main loop"
 * since the Startup process ends as soon as initialisation is complete.
 * (in standby mode, one can think of the replay loop as a main loop,
 * though.)
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/postmaster/startup.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <signal.h>
#include <unistd.h>

#include "access/xlog.h"
#include "libpq/pqsignal.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/startup.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/pmsignal.h"
#include "storage/standby.h"
#include "utils/guc.h"
#include "utils/timeout.h"

/*
 * Flags set by interrupt handlers for later service in the redo loop.
 */
static volatile sig_atomic_t got_SIGHUP = false;
static volatile sig_atomic_t shutdown_requested = false;
static volatile sig_atomic_t promote_triggered = false;

/*
 * Flag set when executing a restore command, to tell SIGTERM signal handler
 * that it's safe to just proc_exit.
 */
static volatile sig_atomic_t in_restore_command = false;

/* Signal handlers */
static void startupproc_quickdie(SIGNAL_ARGS);
static void StartupProcSigUsr1Handler(SIGNAL_ARGS);
static void StartupProcTriggerHandler(SIGNAL_ARGS);
static void StartupProcSigHupHandler(SIGNAL_ARGS);

/* Callbacks */
static void
StartupProcExit(int code, Datum arg);

/* --------------------------------
 *		signal handler routines
 * --------------------------------
 */

/*
 * startupproc_quickdie() occurs when signalled SIGQUIT by the postmaster.
 *
 * Some backend has bought the farm,
 * so we need to stop what we're doing and exit.
 */
static void
startupproc_quickdie(SIGNAL_ARGS)
{















}

/* SIGUSR1: let latch facility handle the signal */
static void
StartupProcSigUsr1Handler(SIGNAL_ARGS)
{





}

/* SIGUSR2: set flag to finish recovery */
static void
StartupProcTriggerHandler(SIGNAL_ARGS)
{






}

/* SIGHUP: set flag to re-read config file at next convenient time */
static void
StartupProcSigHupHandler(SIGNAL_ARGS)
{






}

/* SIGTERM: set flag to abort redo and exit */
static void
StartupProcShutdownHandler(SIGNAL_ARGS)
{













}

/* Handle SIGHUP and SIGTERM signals of startup process */
void
HandleStartupProcInterrupts(void)
{

























}

/* --------------------------------
 *		signal handler routines
 * --------------------------------
 */
static void
StartupProcExit(int code, Datum arg)
{
  /* Shutdown the recovery environment */
  if (standbyState != STANDBY_DISABLED)
  {

  }
}

/* ----------------------------------
 *	Startup Process main entry point
 * ----------------------------------
 */
void
StartupProcessMain(void)
{
  /* Arrange to clean up at startup process exit */
  on_shmem_exit(StartupProcExit, 0);

  /*
   * Properly accept or ignore signals the postmaster might send us.
   */
  pqsignal(SIGHUP, StartupProcSigHupHandler);    /* reload config file */
  pqsignal(SIGINT, SIG_IGN);                     /* ignore query cancel */
  pqsignal(SIGTERM, StartupProcShutdownHandler); /* request shutdown */
  pqsignal(SIGQUIT, startupproc_quickdie);       /* hard crash time */
  InitializeTimeouts();                          /* establishes SIGALRM handler */
  pqsignal(SIGPIPE, SIG_IGN);
  pqsignal(SIGUSR1, StartupProcSigUsr1Handler);
  pqsignal(SIGUSR2, StartupProcTriggerHandler);

  /*
   * Reset some signals that are accepted by postmaster but not here
   */
  pqsignal(SIGCHLD, SIG_DFL);

  /*
   * Register timeouts needed for standby mode
   */
  RegisterTimeout(STANDBY_DEADLOCK_TIMEOUT, StandbyDeadLockHandler);
  RegisterTimeout(STANDBY_TIMEOUT, StandbyTimeoutHandler);
  RegisterTimeout(STANDBY_LOCK_TIMEOUT, StandbyLockTimeoutHandler);

  /*
   * Unblock signals (they were blocked when the postmaster forked us)
   */
  PG_SETMASK(&UnBlockSig);

  /*
   * Do what we came for.
   */
  StartupXLOG();

  /*
   * Exit normally. Exit code 0 tells postmaster that we completed recovery
   * successfully.
   */
  proc_exit(0);
}

void
PreRestoreCommand(void)
{











}

void
PostRestoreCommand(void)
{

}

bool
IsPromoteTriggered(void)
{

}

void
ResetPromoteTriggered(void)
{

}