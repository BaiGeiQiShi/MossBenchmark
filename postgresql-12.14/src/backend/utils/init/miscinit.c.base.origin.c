/*-------------------------------------------------------------------------
 *
 * miscinit.c
 *	  miscellaneous initialization support stuff
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/init/miscinit.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/param.h>
#include <signal.h>
#include <time.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif

#include "access/htup_details.h"
#include "catalog/pg_authid.h"
#include "common/file_perm.h"
#include "libpq/libpq.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "postmaster/postmaster.h"
#include "storage/fd.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/pg_shmem.h"
#include "storage/pmsignal.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/inval.h"
#include "utils/memutils.h"
#include "utils/pidfile.h"
#include "utils/syscache.h"
#include "utils/varlena.h"

#define DIRECTORY_LOCK_FILE "postmaster.pid"

ProcessingMode Mode = InitProcessing;

/* List of lock files to be removed at proc exit */
static List *lock_files = NIL;

static Latch LocalLatchData;

/* ----------------------------------------------------------------
 *		ignoring system indexes support stuff
 *
 * NOTE: "ignoring system indexes" means we do not use the system indexes
 * for lookups (either in hardwired catalog accesses or in planner-generated
 * plans).  We do, however, still update the indexes when a catalog
 * modification is made.
 * ----------------------------------------------------------------
 */

bool IgnoreSystemIndexes = false;

/* ----------------------------------------------------------------
 *				database path / name support stuff
 * ----------------------------------------------------------------
 */

void
SetDatabasePath(const char *path)
{



}

/*
 * Validate the proposed data directory.
 *
 * Also initialize file and directory create modes and mode mask.
 */
void
checkDataDir(void)
{













































































}

/*
 * Set data directory, but make sure it's an absolute path.  Use this,
 * never set DataDir directly.
 */
void
SetDataDir(const char *dir)
{












}

/*
 * Change working directory to DataDir.  Most of the postmaster and backend
 * code assumes that we are in DataDir so it can use relative paths to access
 * stuff in and under the data directory.  For convenience during path
 * setup, however, we don't force the chdir to occur during SetDataDir.
 */
void
ChangeToDataDir(void)
{






}

/* ----------------------------------------------------------------
 *	User ID state
 *
 * We have to track several different values associated with the concept
 * of "user ID".
 *
 * AuthenticatedUserId is determined at connection start and never changes.
 *
 * SessionUserId is initially the same as AuthenticatedUserId, but can be
 * changed by SET SESSION AUTHORIZATION (if AuthenticatedUserIsSuperuser).
 * This is the ID reported by the SESSION_USER SQL function.
 *
 * OuterUserId is the current user ID in effect at the "outer level" (outside
 * any transaction or function).  This is initially the same as SessionUserId,
 * but can be changed by SET ROLE to any role that SessionUserId is a
 * member of.  (XXX rename to something like CurrentRoleId?)
 *
 * CurrentUserId is the current effective user ID; this is the one to use
 * for all normal permissions-checking purposes.  At outer level this will
 * be the same as OuterUserId, but it changes during calls to SECURITY
 * DEFINER functions, as well as locally in some specialized commands.
 *
 * SecurityRestrictionContext holds flags indicating reason(s) for changing
 * CurrentUserId.  In some cases we need to lock down operations that are
 * not directly controlled by privilege settings, and this provides a
 * convenient way to do it.
 * ----------------------------------------------------------------
 */
static Oid AuthenticatedUserId = InvalidOid;
static Oid SessionUserId = InvalidOid;
static Oid OuterUserId = InvalidOid;
static Oid CurrentUserId = InvalidOid;

/* We also have to remember the superuser state of some of these levels */
static bool AuthenticatedUserIsSuperuser = false;
static bool SessionUserIsSuperuser = false;

static int SecurityRestrictionContext = 0;

/* We also remember if a SET ROLE is currently active */
static bool SetRoleIsActive = false;

/*
 * Initialize the basic environment for a postmaster child
 *
 * Should be called as early as possible after the child's startup.
 */
void
InitPostmasterChild(void)
{













































}

/*
 * Initialize the basic environment for a standalone process.
 *
 * argv0 has to be suitable to find the program's executable.
 */
void
InitStandaloneProcess(const char *argv0)
{






















}

void
SwitchToSharedLatch(void)
{
















}

void
SwitchBackToLocalLatch(void)
{











}

/*
 * GetUserId - get the current effective user ID.
 *
 * Note: there's no SetUserId() anymore; use SetUserIdAndSecContext().
 */
Oid
GetUserId(void)
{


}

/*
 * GetOuterUserId/SetOuterUserId - get/set the outer-level user ID.
 */
Oid
GetOuterUserId(void)
{


}

static void
SetOuterUserId(Oid userid)
{






}

/*
 * GetSessionUserId/SetSessionUserId - get/set the session user ID.
 */
Oid
GetSessionUserId(void)
{


}

static void
SetSessionUserId(Oid userid, bool is_superuser)
{









}

/*
 * GetAuthenticatedUserId - get the authenticated user ID
 */
Oid
GetAuthenticatedUserId(void)
{


}

/*
 * GetUserIdAndSecContext/SetUserIdAndSecContext - get/set the current user ID
 * and the SecurityRestrictionContext flags.
 *
 * Currently there are three valid bits in SecurityRestrictionContext:
 *
 * SECURITY_LOCAL_USERID_CHANGE indicates that we are inside an operation
 * that is temporarily changing CurrentUserId via these functions.  This is
 * needed to indicate that the actual value of CurrentUserId is not in sync
 * with guc.c's internal state, so SET ROLE has to be disallowed.
 *
 * SECURITY_RESTRICTED_OPERATION indicates that we are inside an operation
 * that does not wish to trust called user-defined functions at all.  The
 * policy is to use this before operations, e.g. autovacuum and REINDEX, that
 * enumerate relations of a database or schema and run functions associated
 * with each found relation.  The relation owner is the new user ID.  Set this
 * as soon as possible after locking the relation.  Restore the old user ID as
 * late as possible before closing the relation; restoring it shortly after
 * close is also tolerable.  If a command has both relation-enumerating and
 * non-enumerating modes, e.g. ANALYZE, both modes set this bit.  This bit
 * prevents not only SET ROLE, but various other changes of session state that
 * normally is unprotected but might possibly be used to subvert the calling
 * session later.  An example is replacing an existing prepared statement with
 * new code, which will then be executed with the outer session's permissions
 * when the prepared statement is next used.  These restrictions are fairly
 * draconian, but the functions called in relation-enumerating operations are
 * really supposed to be side-effect-free anyway.
 *
 * SECURITY_NOFORCE_RLS indicates that we are inside an operation which should
 * ignore the FORCE ROW LEVEL SECURITY per-table indication.  This is used to
 * ensure that FORCE RLS does not mistakenly break referential integrity
 * checks.  Note that this is intentionally only checked when running as the
 * owner of the table (which should always be the case for referential
 * integrity checks).
 *
 * Unlike GetUserId, GetUserIdAndSecContext does *not* Assert that the current
 * value of CurrentUserId is valid; nor does SetUserIdAndSecContext require
 * the new value to be valid.  In fact, these routines had better not
 * ever throw any kind of error.  This is because they are used by
 * StartTransaction and AbortTransaction to save/restore the settings,
 * and during the first transaction within a backend, the value to be saved
 * and perhaps restored is indeed invalid.  We have to be able to get
 * through AbortTransaction without asserting in case InitPostgres fails.
 */
void
GetUserIdAndSecContext(Oid *userid, int *sec_context)
{


}

void
SetUserIdAndSecContext(Oid userid, int sec_context)
{


}

/*
 * InLocalUserIdChange - are we inside a local change of CurrentUserId?
 */
bool
InLocalUserIdChange(void)
{

}

/*
 * InSecurityRestrictedOperation - are we inside a security-restricted command?
 */
bool
InSecurityRestrictedOperation(void)
{

}

/*
 * InNoForceRLSOperation - are we ignoring FORCE ROW LEVEL SECURITY ?
 */
bool
InNoForceRLSOperation(void)
{

}

/*
 * These are obsolete versions of Get/SetUserIdAndSecContext that are
 * only provided for bug-compatibility with some rather dubious code in
 * pljava.  We allow the userid to be set, but only when not inside a
 * security restriction context.
 */
void
GetUserIdAndContext(Oid *userid, bool *sec_def_context)
{


}

void
SetUserIdAndContext(Oid userid, bool sec_def_context)
{














}

/*
 * Check whether specified role has explicit REPLICATION privilege
 */
bool
has_rolreplication(Oid roleid)
{










}

/*
 * Initialize user identity during normal backend startup
 */
void
InitializeSessionUserId(const char *rolename, Oid roleid)
{























































































}

/*
 * Initialize user identity during special backend startup
 */
void
InitializeSessionUserIdStandalone(void)
{













}

/*
 * Change session auth ID while running
 *
 * Only a superuser may set auth ID to something other than himself.  Note
 * that in case of multiple SETs in a single session, the original userid's
 * superuserness is what matters.  But we set the GUC variable is_superuser
 * to indicate whether the *current* session userid is a superuser.
 *
 * Note: this is not an especially clean place to do the permission check.
 * It's OK because the check does not require catalog access and can't
 * fail during an end-of-transaction GUC reversion, but we may someday
 * have to push it up into assign_session_authorization.
 */
void
SetSessionAuthorization(Oid userid, bool is_superuser)
{











}

/*
 * Report current role id
 *		This follows the semantics of SET ROLE, ie return the
 *outer-level ID not the current effective ID, and return InvalidOid when the
 *setting is logically SET ROLE NONE.
 */
Oid
GetCurrentRoleId(void)
{








}

/*
 * Change Role ID while running (SET ROLE)
 *
 * If roleid is InvalidOid, we are doing SET ROLE NONE: revert to the
 * session user authorization.  In this case the is_superuser argument
 * is ignored.
 *
 * When roleid is not InvalidOid, the caller must have checked whether
 * the session user has permission to become that role.  (We cannot check
 * here because this routine must be able to execute in a failed transaction
 * to restore a prior value of the ROLE GUC variable.)
 */
void
SetCurrentRoleId(Oid roleid, bool is_superuser)
{



























}

/*
 * Get user name from user oid, returns NULL for nonexistent roleid if noerr
 * is true.
 */
char *
GetUserNameFromId(Oid roleid, bool noerr)
{


















}

/*-------------------------------------------------------------------------
 *				Interlock-file support
 *
 * These routines are used to create both a data-directory lockfile
 * ($DATADIR/postmaster.pid) and Unix-socket-file lockfiles ($SOCKFILE.lock).
 * Both kinds of files contain the same info initially, although we can add
 * more information to a data-directory lockfile after it's created, using
 * AddToDataDirLockFile().  See pidfile.h for documentation of the contents
 * of these lockfiles.
 *
 * On successful lockfile creation, a proc_exit callback to remove the
 * lockfile is automatically created.
 *-------------------------------------------------------------------------
 */

/*
 * proc_exit callback to remove lockfiles.
 */
static void
UnlinkLockFiles(int status, Datum arg)
{





















}

/*
 * Create a lockfile.
 *
 * filename is the path name of the lockfile to create.
 * amPostmaster is used to determine how to encode the output PID.
 * socketDir is the Unix socket directory path to include (possibly empty).
 * isDDLock and refName are used to determine what error message to produce.
 */
static void
CreateLockFile(const char *filename, bool amPostmaster, const char *socketDir, bool isDDLock, const char *refName)
{

































































































































































































































































}

/*
 * Create the data directory lockfile.
 *
 * When this is called, we must have already switched the working
 * directory to DataDir, so we can just use a relative path.  This
 * helps ensure that we are locking the directory we should be.
 *
 * Note that the socket directory path line is initially written as empty.
 * postmaster.c will rewrite it upon creating the first Unix socket.
 */
void
CreateDataDirLockFile(bool amPostmaster)
{

}

/*
 * Create a lockfile for the specified Unix socket file.
 */
void
CreateSocketLockFile(const char *socketfile, bool amPostmaster, const char *socketDir)
{




}

/*
 * TouchSocketLockFiles -- mark socket lock files as recently accessed
 *
 * This routine should be called every so often to ensure that the socket
 * lock files have a recent mod or access date.  That saves them
 * from being removed by overenthusiastic /tmp-directory-cleaner daemons.
 * (Another reason we should never have put the socket file in /tmp...)
 */
void
TouchSocketLockFiles(void)
{




































}

/*
 * Add (or replace) a line in the data directory lock file.
 * The given string should not include a trailing newline.
 *
 * Note: because we don't truncate the file, if we were to rewrite a line
 * with less data than it had before, there would be garbage after the last
 * line.  While we could fix that by adding a truncate call, that would make
 * the file update non-atomic, which we'd rather avoid.  Therefore, callers
 * should endeavor never to shorten a line once it's been written.
 */
void
AddToDataDirLockFile(int target_line, const char *str)
{




































































































}

/*
 * Recheck that the data directory lock file still exists with expected
 * content.  Return true if the lock file appears OK, false if it isn't.
 *
 * We call this periodically in the postmaster.  The idea is that if the
 * lock file has been removed or replaced by another postmaster, we should
 * do a panic database shutdown.  Therefore, we should return true if there
 * is any doubt: we do not want to cause a panic shutdown unnecessarily.
 * Transient failures like EINTR or ENFILE should not cause us to fail.
 * (If there really is something wrong, we'll detect it on a future recheck.)
 */
bool
RecheckDataDirLockFile(void)
{














































}

/*-------------------------------------------------------------------------
 *				Version checking support
 *-------------------------------------------------------------------------
 */

/*
 * Determine whether the PG_VERSION file in directory `path' indicates
 * a data version compatible with the version of this program.
 *
 * If compatible, return. Otherwise, ereport(FATAL).
 */
void
ValidatePgVersion(const char *path)
{









































}

/*-------------------------------------------------------------------------
 *				Library preload support
 *-------------------------------------------------------------------------
 */

/*
 * GUC variables: lists of library names to be preloaded at postmaster
 * start and at backend start
 */
char *session_preload_libraries_string = NULL;
char *shared_preload_libraries_string = NULL;
char *local_preload_libraries_string = NULL;

/* Flag telling that we are loading shared_preload_libraries */
bool process_shared_preload_libraries_in_progress = false;

/*
 * load the shared libraries listed in 'libraries'
 *
 * 'gucname': name of GUC variable, for error reports
 * 'restricted': if true, force libraries to be in $libdir/plugins/
 */
static void
load_libraries(const char *libraries, const char *gucname, bool restricted)
{












































}

/*
 * process any libraries that should be preloaded at postmaster start
 */
void
process_shared_preload_libraries(void)
{



}

/*
 * process any libraries that should be preloaded at backend start
 */
void
process_session_preload_libraries(void)
{


}

void
pg_bindtextdomain(const char *domain)
{










}