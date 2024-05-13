/*-------------------------------------------------------------------------
 *
 * portalmem.c
 *	  backend portal memory management
 *
 * Portals are objects representing the execution state of a query.
 * This module provides memory management services for portals, but it
 * doesn't actually run the executor for them.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/mmgr/portalmem.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/xact.h"
#include "catalog/pg_type.h"
#include "commands/portalcmds.h"
#include "miscadmin.h"
#include "storage/ipc.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "utils/timestamp.h"

/*
 * Estimate of the maximum number of open portals a user would have,
 * used in initially sizing the PortalHashTable in EnablePortalManager().
 * Since the hash table can expand, there's no need to make this overly
 * generous, and keeping it small avoids unnecessary overhead in the
 * hash_seq_search() calls executed during transaction end.
 */
#define PORTALS_PER_USER 16

/* ----------------
 *		Global state
 * ----------------
 */

#define MAX_PORTALNAME_LEN NAMEDATALEN

typedef struct portalhashent
{
  char portalname[MAX_PORTALNAME_LEN];
  Portal portal;
} PortalHashEnt;

static HTAB *PortalHashTable = NULL;

#define PortalHashTableLookup(NAME, PORTAL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    PortalHashEnt *hentry;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    hentry = (PortalHashEnt *)hash_search(PortalHashTable, (NAME), HASH_FIND, NULL);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
    if (hentry)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
      PORTAL = hentry->portal;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      PORTAL = NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  } while (0)

#define PortalHashTableInsert(PORTAL, NAME)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    PortalHashEnt *hentry;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    bool found;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    hentry = (PortalHashEnt *)hash_search(PortalHashTable, (NAME), HASH_ENTER, &found);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
    if (found)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
      elog(ERROR, "duplicate portal name");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    hentry->portal = PORTAL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    /* To avoid duplicate storage, make PORTAL->name point to htab entry */                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
    PORTAL->name = hentry->portalname;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  } while (0)

#define PortalHashTableDelete(PORTAL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    PortalHashEnt *hentry;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    hentry = (PortalHashEnt *)hash_search(PortalHashTable, PORTAL->name, HASH_REMOVE, NULL);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
    if (hentry == NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
      elog(WARNING, "trying to delete portal name that does not exist");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  } while (0)

static MemoryContext TopPortalContext = NULL;

/* ----------------------------------------------------------------
 *				   public portal interface functions
 * ----------------------------------------------------------------
 */

/*
 * EnablePortalManager
 *		Enables the portal management module at backend startup.
 */
void
EnablePortalManager(void)
{














}

/*
 * GetPortalByName
 *		Returns a portal given a portal name, or NULL if name not found.
 */
Portal
GetPortalByName(const char *name)
{












}

/*
 * PortalGetPrimaryStmt
 *		Get the "primary" stmt within a portal, ie, the one marked
 *canSetTag.
 *
 * Returns NULL if no such stmt.  If multiple PlannedStmt structs within the
 * portal are marked canSetTag, returns the first one.  Neither of these
 * cases should occur in present usages of this function.
 */
PlannedStmt *
PortalGetPrimaryStmt(Portal portal)
{












}

/*
 * CreatePortal
 *		Returns a new portal given a name.
 *
 * allowDup: if true, automatically drop any pre-existing portal of the
 * same name (if false, an error is raised).
 *
 * dupSilent: if true, don't even emit a WARNING.
 */
Portal
CreatePortal(const char *name, bool allowDup, bool dupSilent)
{















































}

/*
 * CreateNewPortal
 *		Create a new portal, assigning it a random nonconflicting name.
 */
Portal
CreateNewPortal(void)
{
















}

/*
 * PortalDefineQuery
 *		A simple subroutine to establish a portal's query.
 *
 * Notes: as of PG 8.4, caller MUST supply a sourceText string; it is not
 * allowed anymore to pass NULL.  (If you really don't have source text,
 * you can pass a constant string, perhaps "(query not available)".)
 *
 * commandTag shall be NULL if and only if the original query string
 * (before rewriting) was an empty string.  Also, the passed commandTag must
 * be a pointer to a constant string, since it is not copied.
 *
 * If cplan is provided, then it is a cached plan containing the stmts, and
 * the caller must have done GetCachedPlan(), causing a refcount increment.
 * The refcount will be released when the portal is destroyed.
 *
 * If cplan is NULL, then it is the caller's responsibility to ensure that
 * the passed plan trees have adequate lifetime.  Typically this is done by
 * copying them into the portal's context.
 *
 * The caller is also responsible for ensuring that the passed prepStmtName
 * (if not NULL) and sourceText have adequate lifetime.
 *
 * NB: this function mustn't do much beyond storing the passed values; in
 * particular don't do anything that risks elog(ERROR).  If that were to
 * happen here before storing the cplan reference, we'd leak the plancache
 * refcount that the caller is trying to hand off to us.
 */
void
PortalDefineQuery(Portal portal, const char *prepStmtName, const char *sourceText, const char *commandTag, List *stmts, CachedPlan *cplan)
{












}

/*
 * PortalReleaseCachedPlan
 *		Release a portal's reference to its cached plan, if any.
 */
static void
PortalReleaseCachedPlan(Portal portal)
{












}

/*
 * PortalCreateHoldStore
 *		Create the tuplestore for a portal.
 */
void
PortalCreateHoldStore(Portal portal)
{























}

/*
 * PinPortal
 *		Protect a portal from dropping.
 *
 * A pinned portal is still unpinned and dropped at transaction or
 * subtransaction abort.
 */
void
PinPortal(Portal portal)
{






}

void
UnpinPortal(Portal portal)
{






}

/*
 * MarkPortalActive
 *		Transition a portal from READY to ACTIVE state.
 *
 * NOTE: never set portal->status = PORTAL_ACTIVE directly; call this instead.
 */
void
MarkPortalActive(Portal portal)
{








}

/*
 * MarkPortalDone
 *		Transition a portal from ACTIVE to DONE state.
 *
 * NOTE: never set portal->status = PORTAL_DONE directly; call this instead.
 */
void
MarkPortalDone(Portal portal)
{

















}

/*
 * MarkPortalFailed
 *		Transition a portal into FAILED state.
 *
 * NOTE: never set portal->status = PORTAL_FAILED directly; call this instead.
 */
void
MarkPortalFailed(Portal portal)
{

















}

/*
 * PortalDrop
 *		Destroy the portal.
 */
void
PortalDrop(Portal portal, bool isTopCommit)
{





























































































































}

/*
 * Delete all declared cursors.
 *
 * Used by commands: CLOSE ALL, DISCARD ALL
 */
void
PortalHashTableDeleteAll(void)
{

























}

/*
 * "Hold" a portal.  Prepare it for access by later transactions.
 */
static void
HoldPortal(Portal portal)
{
























}

/*
 * Pre-commit processing for portals.
 *
 * Holdable cursors created in this transaction need to be converted to
 * materialized form, since we are going to close down the executor and
 * release locks.  Non-holdable portals created in this transaction are
 * simply removed.  Portals remaining from prior transactions should be
 * left untouched.
 *
 * Returns true if any portals changed state (possibly causing user-defined
 * code to be run), false if not.
 */
bool
PreCommit_Portals(bool isPrepare)
{






























































































}

/*
 * Abort processing for portals.
 *
 * At this point we run the cleanup hook if present, but we can't release the
 * portal's memory until the cleanup call.
 */
void
AtAbort_Portals(void)
{














































































}

/*
 * Post-abort cleanup for portals.
 *
 * Delete all portals not held over from prior transactions.  */
void
AtCleanup_Portals(void)
{




















































}

/*
 * Portal-related cleanup when we return to the main loop on error.
 *
 * This is different from the cleanup at transaction abort.  Auto-held portals
 * are cleaned up on error but not on transaction abort.
 */
void
PortalErrorCleanup(void)
{















}

/*
 * Pre-subcommit processing for portals.
 *
 * Reassign portals created or used in the current subtransaction to the
 * parent subtransaction.
 */
void
AtSubCommit_Portals(SubTransactionId mySubid, SubTransactionId parentSubid, int parentLevel, ResourceOwner parentXactOwner)
{























}

/*
 * Subtransaction abort handling for portals.
 *
 * Deactivate portals created or used during the failed subtransaction.
 * Note that per AtSubCommit_Portals, this will catch portals created/used
 * in descendants of the subtransaction too.
 *
 * We don't destroy any portals here; that's done in AtSubCleanup_Portals.
 */
void
AtSubAbort_Portals(SubTransactionId mySubid, SubTransactionId parentSubid, ResourceOwner myXactOwner, ResourceOwner parentXactOwner)
{






































































































}

/*
 * Post-subabort cleanup for portals.
 *
 * Drop all portals created in the failed subtransaction (but note that
 * we will not drop any that were reassigned to the parent above).
 */
void
AtSubCleanup_Portals(SubTransactionId mySubid)
{





































}

/* Find all available cursors */
Datum
pg_cursor(PG_FUNCTION_ARGS)
{












































































}

bool
ThereAreNoReadyPortals(void)
{
















}

/*
 * Hold all pinned portals.
 *
 * When initiating a COMMIT or ROLLBACK inside a procedure, this must be
 * called to protect internally-generated cursors from being dropped during
 * the transaction shutdown.  Currently, SPI calls this automatically; PLs
 * that initiate COMMIT or ROLLBACK some other way are on the hook to do it
 * themselves.  (Note that we couldn't do this in, say, AtAbort_Portals
 * because we need to run user-defined code while persisting a portal.
 * It's too late to do that once transaction abort has started.)
 *
 * We protect such portals by converting them to held cursors.  We mark them
 * as "auto-held" so that exception exit knows to clean them up.  (In normal,
 * non-exception code paths, the PL needs to clean such portals itself, since
 * transaction end won't do it anymore; but that should be normal practice
 * anyway.)
 */
void
HoldPinnedPortals(void)
{



































}

/*
 * Drop the outer active snapshots for all portals, so that no snapshots
 * remain active.
 *
 * Like HoldPinnedPortals, this must be called when initiating a COMMIT or
 * ROLLBACK inside a procedure.  This has to be separate from that since it
 * should not be run until we're done with steps that are likely to fail.
 *
 * It's tempting to fold this into PreCommit_Portals, but to do so, we'd
 * need to clean up snapshot management in VACUUM and perhaps other places.
 */
void
ForgetPortalSnapshots(void)
{




































}