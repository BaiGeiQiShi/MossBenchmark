/*-------------------------------------------------------------------------
 *
 * dbcommands.c
 *		Database management commands (create/drop database).
 *
 * Note: database creation/destruction commands use exclusive locks on
 * the database objects (as expressed by LockSharedObject()) to avoid
 * stepping on each others' toes.  Formerly we used table-level locks
 * on pg_database, but that's too coarse-grained.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/dbcommands.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "access/xloginsert.h"
#include "access/xlogutils.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_database.h"
#include "catalog/pg_db_role_setting.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_tablespace.h"
#include "commands/comment.h"
#include "commands/dbcommands.h"
#include "commands/dbcommands_xlog.h"
#include "commands/defrem.h"
#include "commands/seclabel.h"
#include "commands/tablespace.h"
#include "common/file_perm.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "postmaster/bgwriter.h"
#include "replication/slot.h"
#include "storage/copydir.h"
#include "storage/fd.h"
#include "storage/lmgr.h"
#include "storage/ipc.h"
#include "storage/md.h"
#include "storage/procarray.h"
#include "storage/smgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/pg_locale.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

typedef struct
{
  Oid src_dboid;  /* source (template) DB */
  Oid dest_dboid; /* DB we are trying to create */
} createdb_failure_params;

typedef struct
{
  Oid dest_dboid; /* DB we are trying to move */
  Oid dest_tsoid; /* tablespace we are trying to move to */
} movedb_failure_params;

/* non-export function prototypes */
static void
createdb_failure_callback(int code, Datum arg);
static void
movedb(const char *dbname, const char *tblspcname);
static void
movedb_failure_callback(int code, Datum arg);
static bool
get_db_info(const char *name, LOCKMODE lockmode, Oid *dbIdP, Oid *ownerIdP, int *encodingP, bool *dbIsTemplateP, bool *dbAllowConnP, Oid *dbLastSysOidP, TransactionId *dbFrozenXidP, MultiXactId *dbMinMultiP, Oid *dbTablespace, char **dbCollate, char **dbCtype);
static bool
have_createdb_privilege(void);
static void
remove_dbtablespaces(Oid db_id);
static bool
check_db_file_conflict(Oid db_id);
static int
errdetail_busy_db(int notherbackends, int npreparedxacts);

/*
 * CREATE DATABASE
 */
Oid
createdb(ParseState *pstate, const CreatedbStmt *stmt)
{














































































































































































































































































































































































































































































































































































































}

/*
 * Check whether chosen encoding matches chosen locale settings.  This
 * restriction is necessary because libc's locale-specific code usually
 * fails when presented with data in an encoding it's not expecting. We
 * allow mismatch in four cases:
 *
 * 1. locale encoding = SQL_ASCII, which means that the locale is C/POSIX
 * which works with any encoding.
 *
 * 2. locale encoding = -1, which means that we couldn't determine the
 * locale's encoding and have to trust the user to get it right.
 *
 * 3. selected encoding is UTF8 and platform is win32. This is because
 * UTF8 is a pseudo codepage that is supported in all locales since it's
 * converted to UTF16 before being used.
 *
 * 4. selected encoding is SQL_ASCII, but only if you're a superuser. This
 * is risky but we have historically allowed it --- notably, the
 * regression tests require it.
 *
 * Note: if you change this policy, fix initdb to match.
 */
void
check_encoding_locale_matches(int encoding, const char *collate, const char *ctype)
{
















}

/* Error cleanup callback for createdb */
static void
createdb_failure_callback(int code, Datum arg)
{











}

/*
 * DROP DATABASE
 */
void
dropdb(const char *dbname, bool missing_ok)
{













































































































































































}

/*
 * Rename database
 */
ObjectAddress
RenameDatabase(const char *oldname, const char *newname)
{



























































































}

/*
 * ALTER DATABASE SET TABLESPACE
 */
static void
movedb(const char *dbname, const char *tblspcname)
{
































































































































































































































































































}

/* Error cleanup callback for movedb */
static void
movedb_failure_callback(int code, Datum arg)
{







}

/*
 * ALTER DATABASE name ...
 */
Oid
AlterDatabase(ParseState *pstate, AlterDatabaseStmt *stmt, bool isTopLevel)
{


































































































































































}

/*
 * ALTER DATABASE name SET ...
 */
Oid
AlterDatabaseSet(AlterDatabaseSetStmt *stmt)
{


















}

/*
 * ALTER DATABASE name OWNER TO newowner
 */
ObjectAddress
AlterDatabaseOwner(const char *dbname, Oid newOwnerId)
{




































































































}

/*
 * Helper functions
 */

/*
 * Look up info about the database named "name".  If the database exists,
 * obtain the specified lock type on it, fill in any of the remaining
 * parameters that aren't NULL, and return true.  If no such database,
 * return false.
 */
static bool
get_db_info(const char *name, LOCKMODE lockmode, Oid *dbIdP, Oid *ownerIdP, int *encodingP, bool *dbIsTemplateP, bool *dbAllowConnP, Oid *dbLastSysOidP, TransactionId *dbFrozenXidP, MultiXactId *dbMinMultiP, Oid *dbTablespace, char **dbCollate, char **dbCtype)
{




































































































































}

/* Check if current user has createdb privileges */
static bool
have_createdb_privilege(void)
{
















}

/*
 * Remove tablespace directories
 *
 * We don't know what tablespaces db_id is using, so iterate through all
 * tablespaces removing <tablespace>/db_id
 */
static void
remove_dbtablespaces(Oid db_id)
{



















































}

/*
 * Check for existing files that conflict with a proposed new DB OID;
 * return true if there are any
 *
 * If there were a subdirectory in any tablespace matching the proposed new
 * OID, we'd get a create failure due to the duplicate name ... and then we'd
 * try to remove that already-existing subdirectory during the cleanup in
 * remove_dbtablespaces.  Nuking existing files seems like a bad idea, so
 * instead we make this extra check before settling on the OID of the new
 * database.  This exactly parallels what GetNewRelFileNode() does for table
 * relfilenode values.
 */
static bool
check_db_file_conflict(Oid db_id)
{





































}

/*
 * Issue a suitable errdetail message for a busy database
 */
static int
errdetail_busy_db(int notherbackends, int npreparedxacts)
{


















}

/*
 * get_database_oid - given a database name, look up the OID
 *
 * If missing_ok is false, throw an error if database name not found.  If
 * true, just return InvalidOid.
 */
Oid
get_database_oid(const char *dbname, bool missing_ok)
{



































}

/*
 * get_database_name - given a database OID, look up the name
 *
 * Returns a palloc'd string, or NULL if no such database.
 */
char *
get_database_name(Oid dbid)
{















}

/*
 * recovery_create_dbdir()
 *
 * During recovery, there's a case where we validly need to recover a missing
 * tablespace directory so that recovery can continue.  This happens when
 * recovery wants to create a database but the holding tablespace has been
 * removed before the server stopped.  Since we expect that the directory will
 * be gone before reaching recovery consistency, and we have no knowledge about
 * the tablespace other than its OID here, we create a real directory under
 * pg_tblspc here instead of restoring the symlink.
 *
 * If only_tblspc is true, then the requested directory must be in pg_tblspc/
 */
static void
recovery_create_dbdir(char *path, bool only_tblspc)
{

























}

/*
 * DATABASE resource manager's routines
 */
void
dbase_redo(XLogReaderState *record)
{


































































































































}