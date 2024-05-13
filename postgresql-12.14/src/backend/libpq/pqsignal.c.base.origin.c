/*-------------------------------------------------------------------------
 *
 * pqsignal.c
 *	  Backend signal(2) support (see also src/port/pqsignal.c)
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/libpq/pqsignal.c
 *
 * ------------------------------------------------------------------------
 */

#include "postgres.h"

#include "libpq/pqsignal.h"

/* Global variables */
sigset_t UnBlockSig, BlockSig, StartupBlockSig;

/*
 * Initialize BlockSig, UnBlockSig, and StartupBlockSig.
 *
 * BlockSig is the set of signals to block when we are trying to block
 * signals.  This includes all signals we normally expect to get, but NOT
 * signals that should never be turned off.
 *
 * StartupBlockSig is the set of signals to block during startup packet
 * collection; it's essentially BlockSig minus SIGTERM, SIGQUIT, SIGALRM.
 *
 * UnBlockSig is the set of signals to block when we don't want to block
 * signals (is this ever nonzero??)
 */
void
pqinitmask(void)
{






















































}

/*
 * Set up a postmaster signal handler for signal "signo"
 *
 * Returns the previous handler.
 *
 * This is used only in the postmaster, which has its own odd approach to
 * signal handling.  For signals with handlers, we block all signals for the
 * duration of signal handler execution.  We also do not set the SA_RESTART
 * flag; this should be safe given the tiny range of code in which the
 * postmaster ever unblocks signals.
 *
 * pqinitmask() must have been invoked previously.
 *
 * On Windows, this function is just an alias for pqsignal()
 * (and note that it's calling the code in src/backend/port/win32/signal.c,
 * not src/port/pqsignal.c).  On that platform, the postmaster's signal
 * handlers still have to block signals for themselves.
 */
pqsigfunc
pqsignal_pm(int signo, pqsigfunc func)
{





























}