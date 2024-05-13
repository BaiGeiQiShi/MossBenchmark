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
  TableScanDesc scan;
  Relation rel;
  Oid src_dboid;
  Oid src_owner;
  int src_encoding;
  char *src_collate;
  char *src_ctype;
  bool src_istemplate;
  bool src_allowconn;
  Oid src_lastsysoid;
  TransactionId src_frozenxid;
  MultiXactId src_minmxid;
  Oid src_deftablespace;
  volatile Oid dst_deftablespace;
  Relation pg_database_rel;
  HeapTuple tuple;
  Datum new_record[Natts_pg_database];
  bool new_record_nulls[Natts_pg_database];
  Oid dboid;
  Oid datdba;
  ListCell *option;
  DefElem *dtablespacename = NULL;
  DefElem *downer = NULL;
  DefElem *dtemplate = NULL;
  DefElem *dencoding = NULL;
  DefElem *dcollate = NULL;
  DefElem *dctype = NULL;
  DefElem *distemplate = NULL;
  DefElem *dallowconnections = NULL;
  DefElem *dconnlimit = NULL;
  char *dbname = stmt->dbname;
  char *dbowner = NULL;
  const char *dbtemplate = NULL;
  char *dbcollate = NULL;
  char *dbctype = NULL;
  char *canonname;
  int encoding = -1;
  bool dbistemplate = false;
  bool dballowconnections = true;
  int dbconnlimit = -1;
  int notherbackends;
  int npreparedxacts;
  createdb_failure_params fparms;

  /* Extract options from the statement node tree */
  foreach (option, stmt->options)
  {
    DefElem *defel = (DefElem *)lfirst(option);

    if (strcmp(defel->defname, "tablespace") == 0)
    {





    }
    else if (strcmp(defel->defname, "owner") == 0)
    {





    }
    else if (strcmp(defel->defname, "template") == 0)
    {
      if (dtemplate)
      {

      }
      dtemplate = defel;
    }
    else if (strcmp(defel->defname, "encoding") == 0)
    {
      if (dencoding)
      {

      }
      dencoding = defel;
    }
    else if (strcmp(defel->defname, "lc_collate") == 0)
    {





    }
    else if (strcmp(defel->defname, "lc_ctype") == 0)
    {





    }
    else if (strcmp(defel->defname, "is_template") == 0)
    {
      if (distemplate)
      {

      }
      distemplate = defel;
    }
    else if (strcmp(defel->defname, "allow_connections") == 0)
    {
      if (dallowconnections)
      {

      }
      dallowconnections = defel;
    }
















  }

  if (downer && downer->arg)
  {

  }
  if (dtemplate && dtemplate->arg)
  {
    dbtemplate = defGetString(dtemplate);
  }
  if (dencoding && dencoding->arg)
  {
    const char *encoding_name;

    if (IsA(dencoding->arg, Integer))
    {






    }
    else
    {
      encoding_name = defGetString(dencoding);
      encoding = pg_valid_server_encoding(encoding_name);
      if (encoding < 0)
      {

      }
    }
  }
  if (dcollate && dcollate->arg)
  {

  }
  if (dctype && dctype->arg)
  {

  }
  if (distemplate && distemplate->arg)
  {
    dbistemplate = defGetBoolean(distemplate);
  }
  if (dallowconnections && dallowconnections->arg)
  {
    dballowconnections = defGetBoolean(dallowconnections);
  }
  if (dconnlimit && dconnlimit->arg)
  {





  }

  /* obtain OID of proposed owner */
  if (dbowner)
  {

  }
  else
  {
    datdba = GetUserId();
  }

  /*
   * To create a database, must have createdb privilege and must be able to
   * become the target role (this does not imply that the target role itself
   * must have createdb privilege).  The latter provision guards against
   * "giveaway" attacks.  Note that a superuser will always have both of
   * these privileges a fortiori.
   */
  if (!have_createdb_privilege())
  {

  }

  check_is_member_of_role(GetUserId(), datdba);

  /*
   * Lookup database (template) to be cloned, and obtain share lock on it.
   * ShareLock allows two CREATE DATABASEs to work from the same template
   * concurrently, while ensuring no one is busy dropping it in parallel
   * (which would be Very Bad since we'd likely get an incomplete copy
   * without knowing it).  This also prevents any new connections from being
   * made to the source until we finish copying it, so we can be sure it
   * won't change underneath us.
   */
  if (!dbtemplate)
  {
    dbtemplate = "template1"; /* Default template database name */
  }

  if (!get_db_info(dbtemplate, ShareLock, &src_dboid, &src_owner, &src_encoding, &src_istemplate, &src_allowconn, &src_lastsysoid, &src_frozenxid, &src_minmxid, &src_deftablespace, &src_collate, &src_ctype))
  {

  }

  /*
   * Permission check: to copy a DB that's not marked datistemplate, you
   * must be superuser or the owner thereof.
   */
  if (!src_istemplate)
  {




  }

  /* If encoding or locales are defaulted, use source's setting */
  if (encoding < 0)
  {
    encoding = src_encoding;
  }
  if (dbcollate == NULL)
  {
    dbcollate = src_collate;
  }
  if (dbctype == NULL)
  {
    dbctype = src_ctype;
  }

  /* Some encodings are client only */
  if (!PG_VALID_BE_ENCODING(encoding))
  {

  }

  /* Check that the chosen locales are valid, and get canonical spellings */
  if (!check_locale(LC_COLLATE, dbcollate, &canonname))
  {

  }
  dbcollate = canonname;
  if (!check_locale(LC_CTYPE, dbctype, &canonname))
  {

  }
  dbctype = canonname;

  check_encoding_locale_matches(encoding, dbcollate, dbctype);

  /*
   * Check that the new encoding and locale settings match the source
   * database.  We insist on this because we simply copy the source data ---
   * any non-ASCII data would be wrongly encoded, and any indexes sorted
   * according to the source locale would be wrong.
   *
   * However, we assume that template0 doesn't contain any non-ASCII data
   * nor any indexes that depend on collation or ctype, so template0 can be
   * used as template for creating a database with any encoding or locale.
   */
  if (strcmp(dbtemplate, "template0") != 0)
  {
    if (encoding != src_encoding)
    {

    }

    if (strcmp(dbcollate, src_collate) != 0)
    {

    }

    if (strcmp(dbctype, src_ctype) != 0)
    {

    }
  }

  /* Resolve default tablespace for new database */
  if (dtablespacename && dtablespacename->arg)
  {
    char *tablespacename;
    AclResult aclresult;



    /* check permissions */






    /* pg_global must never be the default tablespace */





    /*
     * If we are trying to change the default tablespace of the template,
     * we require that the template not have any files in the new default
     * tablespace.  This is necessary because otherwise the copied
     * database would contain pg_class rows that refer to its default
     * tablespace both explicitly (by OID) and implicitly (as zero), which
     * would cause problems.  For example another CREATE DATABASE using
     * the copied database as template, and trying to change its default
     * tablespace again, would yield outright incorrect results (it would
     * improperly move tables to the new default tablespace that should
     * stay in the same tablespace).
     */













  }
  else
  {
    /* Use template database's default tablespace */
    dst_deftablespace = src_deftablespace;
    /* Note there is no additional permission check in this path */
  }

  /*
   * If built with appropriate switch, whine when regression-testing
   * conventions for database names are violated.  But don't complain during
   * initdb.
   */
#ifdef ENFORCE_REGRESSION_TEST_NAME_RESTRICTIONS
  if (IsUnderPostmaster && strstr(dbname, "regression") == NULL)
  {
    elog(WARNING, "databases created by regression test cases should have names including \"regression\"");
  }
#endif

  /*
   * Check for db name conflict.  This is just to give a more friendly error
   * message than "unique index violation".  There's a race condition but
   * we're willing to accept the less friendly message in that case.
   */
  if (OidIsValid(get_database_oid(dbname, true)))
  {

  }

  /*
   * The source DB can't have any active backends, except this one
   * (exception is to allow CREATE DB while connected to template1).
   * Otherwise we might copy inconsistent data.
   *
   * This should be last among the basic error checks, because it involves
   * potential waiting; we may as well throw an error first if we're gonna
   * throw one.
   */
  if (CountOtherDBBackends(src_dboid, &notherbackends, &npreparedxacts))
  {

  }

  /*
   * Select an OID for the new database, checking that it doesn't have a
   * filename conflict with anything already existing in the tablespace
   * directories.
   */
  pg_database_rel = table_open(DatabaseRelationId, RowExclusiveLock);

  do
  {
    dboid = GetNewOidWithIndex(pg_database_rel, DatabaseOidIndexId, Anum_pg_database_oid);
  } while (check_db_file_conflict(dboid));

  /*
   * Insert a new tuple into pg_database.  This establishes our ownership of
   * the new database name (anyone else trying to insert the same name will
   * block on the unique index, and fail after we commit).
   */

  /* Form tuple */
  MemSet(new_record, 0, sizeof(new_record));
  MemSet(new_record_nulls, false, sizeof(new_record_nulls));

  new_record[Anum_pg_database_oid - 1] = ObjectIdGetDatum(dboid);
  new_record[Anum_pg_database_datname - 1] = DirectFunctionCall1(namein, CStringGetDatum(dbname));
  new_record[Anum_pg_database_datdba - 1] = ObjectIdGetDatum(datdba);
  new_record[Anum_pg_database_encoding - 1] = Int32GetDatum(encoding);
  new_record[Anum_pg_database_datcollate - 1] = DirectFunctionCall1(namein, CStringGetDatum(dbcollate));
  new_record[Anum_pg_database_datctype - 1] = DirectFunctionCall1(namein, CStringGetDatum(dbctype));
  new_record[Anum_pg_database_datistemplate - 1] = BoolGetDatum(dbistemplate);
  new_record[Anum_pg_database_datallowconn - 1] = BoolGetDatum(dballowconnections);
  new_record[Anum_pg_database_datconnlimit - 1] = Int32GetDatum(dbconnlimit);
  new_record[Anum_pg_database_datlastsysoid - 1] = ObjectIdGetDatum(src_lastsysoid);
  new_record[Anum_pg_database_datfrozenxid - 1] = TransactionIdGetDatum(src_frozenxid);
  new_record[Anum_pg_database_datminmxid - 1] = TransactionIdGetDatum(src_minmxid);
  new_record[Anum_pg_database_dattablespace - 1] = ObjectIdGetDatum(dst_deftablespace);

  /*
   * We deliberately set datacl to default (NULL), rather than copying it
   * from the template database.  Copying it would be a bad idea when the
   * owner is not the same as the template's owner.
   */
  new_record_nulls[Anum_pg_database_datacl - 1] = true;

  tuple = heap_form_tuple(RelationGetDescr(pg_database_rel), new_record, new_record_nulls);

  CatalogTupleInsert(pg_database_rel, tuple);

  /*
   * Now generate additional catalog entries associated with the new DB
   */

  /* Register owner dependency */
  recordDependencyOnOwner(DatabaseRelationId, dboid, datdba);

  /* Create pg_shdepend entries for objects within database */
  copyTemplateDependencies(src_dboid, dboid);

  /* Post creation hook for new database */
  InvokeObjectPostCreateHook(DatabaseRelationId, dboid, 0);

  /*
   * Force a checkpoint before starting the copy. This will force all dirty
   * buffers, including those of unlogged tables, out to disk, to ensure
   * source database is up-to-date on disk for the copy.
   * FlushDatabaseBuffers() would suffice for that, but we also want to
   * process any pending unlink requests. Otherwise, if a checkpoint
   * happened while we're copying files, a file might be deleted just when
   * we're about to copy it, causing the lstat() call in copydir() to fail
   * with ENOENT.
   */
  RequestCheckpoint(CHECKPOINT_IMMEDIATE | CHECKPOINT_FORCE | CHECKPOINT_WAIT | CHECKPOINT_FLUSH_ALL);

  /*
   * Once we start copying subdirectories, we need to be able to clean 'em
   * up if we fail.  Use an ENSURE block to make sure this happens.  (This
   * is not a 100% solution, because of the possibility of failure during
   * transaction commit after we leave this routine, but it should handle
   * most scenarios.)
   */
  fparms.src_dboid = src_dboid;
  fparms.dest_dboid = dboid;
  PG_ENSURE_ERROR_CLEANUP(createdb_failure_callback, PointerGetDatum(&fparms));
  {
    /*
     * Iterate through all tablespaces of the template database, and copy
     * each one to the new database.
     */
    rel = table_open(TableSpaceRelationId, AccessShareLock);
    scan = table_beginscan_catalog(rel, 0, NULL);
    while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
    {
      Form_pg_tablespace spaceform = (Form_pg_tablespace)GETSTRUCT(tuple);
      Oid srctablespace = spaceform->oid;
      Oid dsttablespace;
      char *srcpath;
      char *dstpath;
      struct stat st;

      /* No need to copy global tablespace */
      if (srctablespace == GLOBALTABLESPACE_OID)
      {
        continue;
      }

      srcpath = GetDatabasePath(src_dboid, srctablespace);

      if (stat(srcpath, &st) < 0 || !S_ISDIR(st.st_mode) || directory_is_empty(srcpath))
      {
        /* Assume we can ignore it */


      }

      if (srctablespace == src_deftablespace)
      {
        dsttablespace = dst_deftablespace;
      }
      else
      {

      }

      dstpath = GetDatabasePath(dboid, dsttablespace);

      /*
       * Copy this subdirectory to the new location
       *
       * We don't need to copy subdirectories
       */
      copydir(srcpath, dstpath, false);

      /* Record the filesystem change in XLOG */
      {
        xl_dbase_create_rec xlrec;

        xlrec.db_id = dboid;
        xlrec.tablespace_id = dsttablespace;
        xlrec.src_db_id = src_dboid;
        xlrec.src_tablespace_id = srctablespace;

        XLogBeginInsert();
        XLogRegisterData((char *)&xlrec, sizeof(xl_dbase_create_rec));

        (void)XLogInsert(RM_DBASE_ID, XLOG_DBASE_CREATE | XLR_SPECIAL_REL_UPDATE);
      }
    }
    table_endscan(scan);
    table_close(rel, AccessShareLock);

    /*
     * We force a checkpoint before committing.  This effectively means
     * that committed XLOG_DBASE_CREATE operations will never need to be
     * replayed (at least not in ordinary crash recovery; we still have to
     * make the XLOG entry for the benefit of PITR operations). This
     * avoids two nasty scenarios:
     *
     * #1: When PITR is off, we don't XLOG the contents of newly created
     * indexes; therefore the drop-and-recreate-whole-directory behavior
     * of DBASE_CREATE replay would lose such indexes.
     *
     * #2: Since we have to recopy the source database during DBASE_CREATE
     * replay, we run the risk of copying changes in it that were
     * committed after the original CREATE DATABASE command but before the
     * system crash that led to the replay.  This is at least unexpected
     * and at worst could lead to inconsistencies, eg duplicate table
     * names.
     *
     * (Both of these were real bugs in releases 8.0 through 8.0.3.)
     *
     * In PITR replay, the first of these isn't an issue, and the second
     * is only a risk if the CREATE DATABASE and subsequent template
     * database change both occur while a base backup is being taken.
     * There doesn't seem to be much we can do about that except document
     * it as a limitation.
     *
     * Perhaps if we ever implement CREATE DATABASE in a less cheesy way,
     * we can avoid this.
     */
    RequestCheckpoint(CHECKPOINT_IMMEDIATE | CHECKPOINT_FORCE | CHECKPOINT_WAIT);

    /*
     * Close pg_database, but keep lock till commit.
     */
    table_close(pg_database_rel, NoLock);

    /*
     * Force synchronous commit, thus minimizing the window between
     * creation of the database files and committal of the transaction. If
     * we crash before committing, we'll have a DB that's taking up disk
     * space but is not in pg_database, which is not good.
     */
    ForceSyncCommit();
  }
  PG_END_ENSURE_ERROR_CLEANUP(createdb_failure_callback, PointerGetDatum(&fparms));

  return dboid;
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
  int ctype_encoding = pg_get_encoding_from_locale(ctype, true);
  int collate_encoding = pg_get_encoding_from_locale(collate, true);

  if (!(ctype_encoding == encoding || ctype_encoding == PG_SQL_ASCII || ctype_encoding == -1 ||
#ifdef WIN32
          encoding == PG_UTF8 ||
#endif
          (encoding == PG_SQL_ASCII && superuser())))
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("encoding \"%s\" does not match locale \"%s\"", pg_encoding_to_char(encoding), ctype), errdetail("The chosen LC_CTYPE setting requires encoding \"%s\".", pg_encoding_to_char(ctype_encoding))));

  if (!(collate_encoding == encoding || collate_encoding == PG_SQL_ASCII || collate_encoding == -1 ||
#ifdef WIN32
          encoding == PG_UTF8 ||
#endif
          (encoding == PG_SQL_ASCII && superuser())))
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("encoding \"%s\" does not match locale \"%s\"", pg_encoding_to_char(encoding), collate), errdetail("The chosen LC_COLLATE setting requires encoding \"%s\".", pg_encoding_to_char(collate_encoding))));
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
  Oid db_id;
  bool db_istemplate;
  Relation pgdbrel;
  HeapTuple tup;
  int notherbackends;
  int npreparedxacts;
  int nslots, nslots_active;
  int nsubscriptions;

  /*
   * Look up the target database's OID, and get exclusive lock on it. We
   * need this to ensure that no new backend starts up in the target
   * database while we are deleting it (see postinit.c), and that no one is
   * using it as a CREATE DATABASE template or trying to delete it for
   * themselves.
   */
  pgdbrel = table_open(DatabaseRelationId, RowExclusiveLock);

  if (!get_db_info(dbname, AccessExclusiveLock, &db_id, NULL, NULL, &db_istemplate, NULL, NULL, NULL, NULL, NULL, NULL, NULL))
  {
    if (!missing_ok)
    {

    }
    else
    {
      /* Close pg_database, release the lock, since we changed nothing */
      table_close(pgdbrel, RowExclusiveLock);
      ereport(NOTICE, (errmsg("database \"%s\" does not exist, skipping", dbname)));
      return;
    }
  }

  /*
   * Permission checks
   */
  if (!pg_database_ownercheck(db_id, GetUserId()))
  {

  }

  /* DROP hook for the database being removed */
  InvokeObjectDropHook(DatabaseRelationId, db_id, 0);

  /*
   * Disallow dropping a DB that is marked istemplate.  This is just to
   * prevent people from accidentally dropping template0 or template1; they
   * can do so if they're really determined ...
   */
  if (db_istemplate)
  {

  }

  /* Obviously can't drop my own database */
  if (db_id == MyDatabaseId)
  {

  }

  /*
   * Check whether there are active logical slots that refer to the
   * to-be-dropped database. The database lock we are holding prevents the
   * creation of new slots using the database or existing slots becoming
   * active.
   */
  (void)ReplicationSlotsCountDBSlots(db_id, &nslots, &nslots_active);
  if (nslots_active)
  {

  }

  /*
   * Check for other backends in the target database.  (Because we hold the
   * database lock, no new ones can start after this.)
   *
   * As in CREATE DATABASE, check this after other error conditions.
   */
  if (CountOtherDBBackends(db_id, &notherbackends, &npreparedxacts))
  {

  }

  /*
   * Check if there are subscriptions defined in the target database.
   *
   * We can't drop them automatically because they might be holding
   * resources in other databases/instances.
   */
  if ((nsubscriptions = CountDBSubscriptions(db_id)) > 0)
  {

  }

  /*
   * Remove the database's tuple from pg_database.
   */
  tup = SearchSysCache1(DATABASEOID, ObjectIdGetDatum(db_id));
  if (!HeapTupleIsValid(tup))
  {

  }

  CatalogTupleDelete(pgdbrel, &tup->t_self);

  ReleaseSysCache(tup);

  /*
   * Delete any comments or security labels associated with the database.
   */
  DeleteSharedComments(db_id, DatabaseRelationId);
  DeleteSharedSecurityLabel(db_id, DatabaseRelationId);

  /*
   * Remove settings associated with this database
   */
  DropSetting(db_id, InvalidOid);

  /*
   * Remove shared dependency references for the database.
   */
  dropDatabaseDependencies(db_id);

  /*
   * Drop db-specific replication slots.
   */
  ReplicationSlotsDropDBSlots(db_id);

  /*
   * Drop pages for this database that are in the shared buffer cache. This
   * is important to ensure that no remaining backend tries to write out a
   * dirty buffer to the dead database later...
   */
  DropDatabaseBuffers(db_id);

  /*
   * Tell the stats collector to forget it immediately, too.
   */
  pgstat_drop_database(db_id);

  /*
   * Tell checkpointer to forget any pending fsync and unlink requests for
   * files in the database; else the fsyncs will fail at next checkpoint, or
   * worse, it will delete files that belong to a newly created database
   * with the same OID.
   */
  ForgetDatabaseSyncRequests(db_id);

  /*
   * Force a checkpoint to make sure the checkpointer has received the
   * message sent by ForgetDatabaseSyncRequests. On Windows, this also
   * ensures that background procs don't hold any open files, which would
   * cause rmdir() to fail.
   */
  RequestCheckpoint(CHECKPOINT_IMMEDIATE | CHECKPOINT_FORCE | CHECKPOINT_WAIT);

  /*
   * Remove all tablespace subdirs belonging to the database.
   */
  remove_dbtablespaces(db_id);

  /*
   * Close pg_database, but keep lock till commit.
   */
  table_close(pgdbrel, NoLock);

  /*
   * Force synchronous commit, thus minimizing the window between removal of
   * the database files and committal of the transaction. If we crash before
   * committing, we'll have a DB that's gone on disk but still there
   * according to pg_database, which is not good.
   */
  ForceSyncCommit();
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
  Oid datid = get_database_oid(stmt->dbname, false);

  /*
   * Obtain a lock on the database and make sure it didn't go away in the
   * meantime.
   */
  shdepLockAndCheckObject(DatabaseRelationId, datid);

  if (!pg_database_ownercheck(datid, GetUserId()))
  {

  }

  AlterSetting(datid, InvalidOid, stmt->setstmt);

  UnlockSharedObject(DatabaseRelationId, datid, 0, AccessShareLock);

  return datid;
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
  bool result = false;
  Relation relation;

  AssertArg(name);

  /* Caller may wish to grab a better lock on pg_database beforehand... */
  relation = table_open(DatabaseRelationId, AccessShareLock);

  /*
   * Loop covers the rare case where the database is renamed before we can
   * lock it.  We try again just in case we can find a new one of the same
   * name.
   */
  for (;;)
  {
    ScanKeyData scanKey;
    SysScanDesc scan;
    HeapTuple tuple;
    Oid dbOid;

    /*
     * there's no syscache for database-indexed-by-name, so must do it the
     * hard way
     */
    ScanKeyInit(&scanKey, Anum_pg_database_datname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(name));

    scan = systable_beginscan(relation, DatabaseNameIndexId, true, NULL, 1, &scanKey);

    tuple = systable_getnext(scan);

    if (!HeapTupleIsValid(tuple))
    {
      /* definitely no database of that name */
      systable_endscan(scan);
      break;
    }

    dbOid = ((Form_pg_database)GETSTRUCT(tuple))->oid;

    systable_endscan(scan);

    /*
     * Now that we have a database OID, we can try to lock the DB.
     */
    if (lockmode != NoLock)
    {
      LockSharedObject(DatabaseRelationId, dbOid, 0, lockmode);
    }

    /*
     * And now, re-fetch the tuple by OID.  If it's still there and still
     * the same name, we win; else, drop the lock and loop back to try
     * again.
     */
    tuple = SearchSysCache1(DATABASEOID, ObjectIdGetDatum(dbOid));
    if (HeapTupleIsValid(tuple))
    {
      Form_pg_database dbform = (Form_pg_database)GETSTRUCT(tuple);

      if (strcmp(name, NameStr(dbform->datname)) == 0)
      {
        /* oid of the database */
        if (dbIdP)
        {
          *dbIdP = dbOid;
        }
        /* oid of the owner */
        if (ownerIdP)
        {
          *ownerIdP = dbform->datdba;
        }
        /* character encoding */
        if (encodingP)
        {
          *encodingP = dbform->encoding;
        }
        /* allowed as template? */
        if (dbIsTemplateP)
        {
          *dbIsTemplateP = dbform->datistemplate;
        }
        /* allowing connections? */
        if (dbAllowConnP)
        {
          *dbAllowConnP = dbform->datallowconn;
        }
        /* last system OID used in database */
        if (dbLastSysOidP)
        {
          *dbLastSysOidP = dbform->datlastsysoid;
        }
        /* limit of frozen XIDs */
        if (dbFrozenXidP)
        {
          *dbFrozenXidP = dbform->datfrozenxid;
        }
        /* minimum MultixactId */
        if (dbMinMultiP)
        {
          *dbMinMultiP = dbform->datminmxid;
        }
        /* default tablespace for this database */
        if (dbTablespace)
        {
          *dbTablespace = dbform->dattablespace;
        }
        /* default locale settings for this database */
        if (dbCollate)
        {
          *dbCollate = pstrdup(NameStr(dbform->datcollate));
        }
        if (dbCtype)
        {
          *dbCtype = pstrdup(NameStr(dbform->datctype));
        }
        ReleaseSysCache(tuple);
        result = true;
        break;
      }
      /* can only get here if it was just renamed */

    }

    if (lockmode != NoLock)
    {

    }
  }

  table_close(relation, AccessShareLock);

  return result;
}

/* Check if current user has createdb privileges */
static bool
have_createdb_privilege(void)
{
  bool result = false;
  HeapTuple utup;

  /* Superusers can always do everything */
  if (superuser())
  {
    return true;
  }








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
  Relation rel;
  TableScanDesc scan;
  HeapTuple tuple;

  rel = table_open(TableSpaceRelationId, AccessShareLock);
  scan = table_beginscan_catalog(rel, 0, NULL);
  while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    Form_pg_tablespace spcform = (Form_pg_tablespace)GETSTRUCT(tuple);
    Oid dsttablespace = spcform->oid;
    char *dstpath;
    struct stat st;

    /* Don't mess with the global tablespace */
    if (dsttablespace == GLOBALTABLESPACE_OID)
    {
      continue;
    }

    dstpath = GetDatabasePath(db_id, dsttablespace);

    if (lstat(dstpath, &st) < 0 || !S_ISDIR(st.st_mode))
    {
      /* Assume we can ignore it */


    }

    if (!rmtree(dstpath, true))
    {

    }

    /* Record the filesystem change in XLOG */
    {
      xl_dbase_drop_rec xlrec;

      xlrec.db_id = db_id;
      xlrec.tablespace_id = dsttablespace;

      XLogBeginInsert();
      XLogRegisterData((char *)&xlrec, sizeof(xl_dbase_drop_rec));

      (void)XLogInsert(RM_DBASE_ID, XLOG_DBASE_DROP | XLR_SPECIAL_REL_UPDATE);
    }

    pfree(dstpath);
  }

  table_endscan(scan);
  table_close(rel, AccessShareLock);
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
  bool result = false;
  Relation rel;
  TableScanDesc scan;
  HeapTuple tuple;

  rel = table_open(TableSpaceRelationId, AccessShareLock);
  scan = table_beginscan_catalog(rel, 0, NULL);
  while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    Form_pg_tablespace spcform = (Form_pg_tablespace)GETSTRUCT(tuple);
    Oid dsttablespace = spcform->oid;
    char *dstpath;
    struct stat st;

    /* Don't mess with the global tablespace */
    if (dsttablespace == GLOBALTABLESPACE_OID)
    {
      continue;
    }

    dstpath = GetDatabasePath(db_id, dsttablespace);

    if (lstat(dstpath, &st) == 0)
    {
      /* Found a conflicting file (or directory, whatever) */



    }

    pfree(dstpath);
  }

  table_endscan(scan);
  table_close(rel, AccessShareLock);

  return result;
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
  Relation pg_database;
  ScanKeyData entry[1];
  SysScanDesc scan;
  HeapTuple dbtuple;
  Oid oid;

  /*
   * There's no syscache for pg_database indexed by name, so we must look
   * the hard way.
   */
  pg_database = table_open(DatabaseRelationId, AccessShareLock);
  ScanKeyInit(&entry[0], Anum_pg_database_datname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(dbname));
  scan = systable_beginscan(pg_database, DatabaseNameIndexId, true, NULL, 1, entry);

  dbtuple = systable_getnext(scan);

  /* We assume that there can be at most one matching tuple */
  if (HeapTupleIsValid(dbtuple))
  {
    oid = ((Form_pg_database)GETSTRUCT(dbtuple))->oid;
  }
  else
  {
    oid = InvalidOid;
  }

  systable_endscan(scan);
  table_close(pg_database, AccessShareLock);

  if (!OidIsValid(oid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database \"%s\" does not exist", dbname)));
  }

  return oid;
}

/*
 * get_database_name - given a database OID, look up the name
 *
 * Returns a palloc'd string, or NULL if no such database.
 */
char *
get_database_name(Oid dbid)
{
  HeapTuple dbtuple;
  char *result;

  dbtuple = SearchSysCache1(DATABASEOID, ObjectIdGetDatum(dbid));
  if (HeapTupleIsValid(dbtuple))
  {
    result = pstrdup(NameStr(((Form_pg_database)GETSTRUCT(dbtuple))->datname));
    ReleaseSysCache(dbtuple);
  }
  else
  {

  }

  return result;
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