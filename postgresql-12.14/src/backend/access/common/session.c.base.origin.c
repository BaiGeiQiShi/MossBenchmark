/*-------------------------------------------------------------------------
 *
 * session.c
 *		Encapsulation of user session.
 *
 * This is intended to contain data that needs to be shared between backends
 * performing work for a client session.  In particular such a session is
 * shared between the leader and worker processes for parallel queries.  At
 * some later point it might also become useful infrastructure for separating
 * backends from client connections, e.g. for the purpose of pooling.
 *
 * Currently this infrastructure is used to share:
 * - typemod registry for ephemeral row-types, i.e. BlessTupleDesc etc.
 *
 * Portions Copyright (c) 2017-2019, PostgreSQL Global Development Group
 *
 * src/backend/access/common/session.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/session.h"
#include "storage/lwlock.h"
#include "storage/shm_toc.h"
#include "utils/memutils.h"
#include "utils/typcache.h"

/* Magic number for per-session DSM TOC. */
#define SESSION_MAGIC 0xabb0fbc9

/*
 * We want to create a DSA area to store shared state that has the same
 * lifetime as a session.  So far, it's only used to hold the shared record
 * type registry.  We don't want it to have to create any DSM segments just
 * yet in common cases, so we'll give it enough space to hold a very small
 * SharedRecordTypmodRegistry.
 */
#define SESSION_DSA_SIZE 0x30000

/*
 * Magic numbers for state sharing in the per-session DSM area.
 */
#define SESSION_KEY_DSA UINT64CONST(0xFFFFFFFFFFFF0001)
#define SESSION_KEY_RECORD_TYPMOD_REGISTRY UINT64CONST(0xFFFFFFFFFFFF0002)

/* This backend's current session. */
Session *CurrentSession = NULL;

/*
 * Set up CurrentSession to point to an empty Session object.
 */
void
InitializeSession(void)
{

}

/*
 * Initialize the per-session DSM segment if it isn't already initialized, and
 * return its handle so that worker processes can attach to it.
 *
 * Unlike the per-context DSM segment, this segment and its contents are
 * reused for future parallel queries.
 *
 * Return DSM_HANDLE_INVALID if a segment can't be allocated due to lack of
 * resources.
 */
dsm_handle
GetSessionDsmHandle(void)
{







































































}

/*
 * Attach to a per-session DSM segment provided by a parallel leader.
 */
void
AttachSession(dsm_handle handle)
{


































}

/*
 * Detach from the current session DSM segment.  It's not strictly necessary
 * to do this explicitly since we'll detach automatically at backend exit, but
 * if we ever reuse parallel workers it will become important for workers to
 * detach from one session before attaching to another.  Note that this runs
 * detach hooks.
 */
void
DetachSession(void)
{





}