/*-------------------------------------------------------------------------
 *
 * tablespace.c
 *	  Commands to manipulate table spaces
 *
 * Tablespaces in PostgreSQL are designed to allow users to determine
 * where the data file(s) for a given database object reside on the file
 * system.
 *
 * A tablespace represents a directory on the file system. At tablespace
 * creation time, the directory must be empty. To simplify things and
 * remove the possibility of having file name conflicts, we isolate
 * files within a tablespace into database-specific subdirectories.
 *
 * To support file access via the information given in RelFileNode, we
 * maintain a symbolic-link map in $PGDATA/pg_tblspc. The symlinks are
 * named by tablespace OIDs and point to the actual tablespace directories.
 * There is also a per-cluster version directory in each tablespace.
 * Thus the full path to an arbitrary file is
 *			$PGDATA/pg_tblspc/spcoid/PG_MAJORVER_CATVER/dboid/relfilenode
 * e.g.
 *			$PGDATA/pg_tblspc/20981/PG_9.0_201002161/719849/83292814
 *
 * There are two tablespaces created at initdb time: pg_global (for shared
 * tables) and pg_default (for everything else).  For backwards compatibility
 * and to remain functional on platforms without symlinks, these tablespaces
 * are accessed specially: they are respectively
 *			$PGDATA/global/relfilenode
 *			$PGDATA/base/dboid/relfilenode
 *
 * To allow CREATE DATABASE to give a new database a default tablespace
 * that's different from the template database's default, we make the
 * provision that a zero in pg_class.reltablespace means the database's
 * default tablespace.  Without this, CREATE DATABASE would have to go in
 * and munge the system catalogs of the new database.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/tablespace.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "access/heapam.h"
#include "access/reloptions.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_tablespace.h"
#include "commands/comment.h"
#include "commands/seclabel.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "common/file_perm.h"
#include "miscadmin.h"
#include "postmaster/bgwriter.h"
#include "storage/fd.h"
#include "storage/lmgr.h"
#include "storage/standby.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/varlena.h"

/* GUC variables */
char *default_tablespace = NULL;
char *temp_tablespaces = NULL;
bool allow_in_place_tablespaces = false;

static void
create_tablespace_directories(const char *location, const Oid tablespaceoid);
static bool
destroy_tablespace_directories(Oid tablespaceoid, bool redo);

/*
 * Each database using a table space is isolated into its own name space
 * by a subdirectory named for the database OID.  On first creation of an
 * object in the tablespace, create the subdirectory.  If the subdirectory
 * already exists, fall through quietly.
 *
 * isRedo indicates that we are creating an object during WAL replay.
 * In this case we will cope with the possibility of the tablespace
 * directory not being there either --- this could happen if we are
 * replaying an operation on a table in a subsequently-dropped tablespace.
 * We handle this by making a directory in the place where the tablespace
 * symlink would normally be.  This isn't an exact replay of course, but
 * it's the best we can do given the available information.
 *
 * If tablespaces are not supported, we still need it in case we have to
 * re-create a database subdirectory (of $PGDATA/base) during WAL replay.
 */
void
TablespaceCreateDbspace(Oid spcNode, Oid dbNode, bool isRedo)
{

















































































}

/*
 * Create a table space
 *
 * Only superusers can create a tablespace. This seems a reasonable restriction
 * since we're determining the system layout and, anyway, we probably have
 * root if we're doing this kind of activity
 */
Oid
CreateTableSpace(CreateTableSpaceStmt *stmt)
{









































































































































































}

/*
 * Drop a table space
 *
 * Be careful to check that the tablespace is empty.
 */
void
DropTableSpace(DropTableSpaceStmt *stmt)
{





















































































































































}

/*
 * create_tablespace_directories
 *
 *	Attempt to create filesystem infrastructure linking $PGDATA/pg_tblspc/
 *	to the specified directory
 */
static void
create_tablespace_directories(const char *location, const Oid tablespaceoid)
{























































































}

/*
 * destroy_tablespace_directories
 *
 * Attempt to remove filesystem infrastructure for the tablespace.
 *
 * 'redo' indicates we are redoing a drop from XLOG; in that case we should
 * not throw an ERROR for problems, just LOG them.  The worst consequence of
 * not removing files here would be failure to release some disk space, which
 * does not justify throwing an error that would require manual intervention
 * to get the database running again.
 *
 * Returns true if successful, false if some subdirectory is not empty
 */
static bool
destroy_tablespace_directories(Oid tablespaceoid, bool redo)
{











































































































































}

/*
 * Check if a directory is empty.
 *
 * This probably belongs somewhere else, but not sure where...
 */
bool
directory_is_empty(const char *path)
{

















}

/*
 *	remove_tablespace_symlink
 *
 * This function removes symlinks in pg_tblspc.  On Windows, junction points
 * act like directories so we must be able to apply rmdir.  This function
 * works like the symlink removal code in destroy_tablespace_directories,
 * except that failure to remove is always an ERROR.  But if the file doesn't
 * exist at all, that's OK.
 */
void
remove_tablespace_symlink(const char *linkloc)
{




































}

/*
 * Rename a tablespace
 */
ObjectAddress
RenameTableSpace(const char *oldname, const char *newname)
{








































































}

/*
 * Alter table space options
 */
Oid
AlterTableSpaceOptions(AlterTableSpaceOptionsStmt *stmt)
{































































}

/*
 * Routines for handling the GUC variable 'default_tablespace'.
 */

/* check_hook: validate new default_tablespace */
bool
check_default_tablespace(char **newval, void **extra, GucSource source)
{


























}

/*
 * GetDefaultTablespace -- get the OID of the current default tablespace
 *
 * Temporary objects have different default tablespaces, hence the
 * relpersistence parameter must be specified.  Also, for partitioned tables,
 * we disallow specifying the database default, so that needs to be specified
 * too.
 *
 * May return InvalidOid to indicate "use the database's default tablespace".
 *
 * Note that caller is expected to check appropriate permissions for any
 * result other than InvalidOid.
 *
 * This exists to hide (and possibly optimize the use of) the
 * default_tablespace GUC variable.
 */
Oid
GetDefaultTablespace(char relpersistence, bool partitioned)
{







































}

/*
 * Routines for handling the GUC variable 'temp_tablespaces'.
 */

typedef struct
{
  /* Array of OIDs to be passed to SetTempTablespaces() */
  int numSpcs;
  Oid tblSpcs[FLEXIBLE_ARRAY_MEMBER];
} temp_tablespaces_extra;

/* check_hook: validate new temp_tablespaces */
bool
check_temp_tablespaces(char **newval, void **extra, GucSource source)
{







































































































}

/* assign_hook: do extra actions as needed */
void
assign_temp_tablespaces(const char *newval, void *extra)
{

















}

/*
 * PrepareTempTablespaces -- prepare to use temp tablespaces
 *
 * If we have not already done so in the current transaction, parse the
 * temp_tablespaces GUC variable and tell fd.c which tablespace(s) to use
 * for temp files.
 */
void
PrepareTempTablespaces(void)
{























































































}

/*
 * get_tablespace_oid - given a tablespace name, look up the OID
 *
 * If missing_ok is false, throw an error if tablespace name not found.  If
 * true, just return InvalidOid.
 */
Oid
get_tablespace_oid(const char *tablespacename, bool missing_ok)
{




































}

/*
 * get_tablespace_name - given a tablespace OID, look up the name
 *
 * Returns a palloc'd string, or NULL if no such tablespace.
 */
char *
get_tablespace_name(Oid spc_oid)
{































}

/*
 * TABLESPACE resource manager's routines
 */
void
tblspc_redo(XLogReaderState *record)
{





















































}