/* -------------------------------------------------------------------------
 *
 * seclabel.c
 *	  routines to support security label feature.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * -------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/relation.h"
#include "access/table.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/pg_seclabel.h"
#include "catalog/pg_shseclabel.h"
#include "commands/seclabel.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/rel.h"

typedef struct
{
  const char *provider_name;
  check_object_relabel_type hook;
} LabelProvider;

static List *label_provider_list = NIL;

/*
 * ExecSecLabelStmt --
 *
 * Apply a security label to a database object.
 *
 * Returns the ObjectAddress of the object to which the policy was applied.
 */
ObjectAddress
ExecSecLabelStmt(SecLabelStmt *stmt)
{
  LabelProvider *provider = NULL;
  ObjectAddress address;
  Relation relation;
  ListCell *lc;

  /*
   * Find the named label provider, or if none specified, check whether
   * there's exactly one, and if so use it.
   */
  if (stmt->provider == NULL)
  {
    if (label_provider_list == NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("no security label providers have been loaded")));
    }
    if (lnext(list_head(label_provider_list)) != NULL)
    {

    }
    provider = (LabelProvider *)linitial(label_provider_list);
  }
  else
  {
    foreach (lc, label_provider_list)
    {
      LabelProvider *lp = lfirst(lc);






    }
    if (provider == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("security label provider \"%s\" is not loaded", stmt->provider)));
    }
  }

  /*
   * Translate the parser representation which identifies this object into
   * an ObjectAddress. get_object_address() will throw an error if the
   * object does not exist, and will also acquire a lock on the target to
   * guard against concurrent modifications.
   */
  address = get_object_address(stmt->objtype, stmt->object, &relation, ShareUpdateExclusiveLock, false);

  /* Require ownership of the target object. */
  check_object_ownership(GetUserId(), stmt->objtype, address, stmt->object, relation);

  /* Perform other integrity checks as needed. */
  switch (stmt->objtype)
  {
  case OBJECT_COLUMN:;;

    /*
     * Allow security labels only on columns of tables, views,
     * materialized views, composite types, and foreign tables (which
     * are the only relkinds for which pg_dump will dump labels).
     */





  default:;;;

  }

  /* Provider gets control here, may throw ERROR to veto new label. */


  /* Apply new label. */


  /*
   * If get_object_address() opened the relation for us, we close it to keep
   * the reference count correct - but we retain any locks acquired by
   * get_object_address() until commit time, to guard against concurrent
   * activity.
   */






}

/*
 * GetSharedSecurityLabel returns the security label for a shared object for
 * a given provider, or NULL if there is no such label.
 */
static char *
GetSharedSecurityLabel(const ObjectAddress *object, const char *provider)
{






























}

/*
 * GetSecurityLabel returns the security label for a shared or database object
 * for a given provider, or NULL if there is no such label.
 */
char *
GetSecurityLabel(const ObjectAddress *object, const char *provider)
{






































}

/*
 * SetSharedSecurityLabel is a helper function of SetSecurityLabel to
 * handle shared database objects.
 */
static void
SetSharedSecurityLabel(const ObjectAddress *object, const char *provider, const char *label)
{


























































}

/*
 * SetSecurityLabel attempts to set the security label for the specified
 * provider on the specified object to the given value.  NULL means that any
 * existing label should be deleted.
 */
void
SetSecurityLabel(const ObjectAddress *object, const char *provider, const char *label)
{




































































}

/*
 * DeleteSharedSecurityLabel is a helper function of DeleteSecurityLabel
 * to handle shared database objects.
 */
void
DeleteSharedSecurityLabel(Oid objectId, Oid classId)
{
  Relation pg_shseclabel;
  ScanKeyData skey[2];
  SysScanDesc scan;
  HeapTuple oldtup;

  ScanKeyInit(&skey[0], Anum_pg_shseclabel_objoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(objectId));
  ScanKeyInit(&skey[1], Anum_pg_shseclabel_classoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(classId));

  pg_shseclabel = table_open(SharedSecLabelRelationId, RowExclusiveLock);

  scan = systable_beginscan(pg_shseclabel, SharedSecLabelObjectIndexId, true, NULL, 2, skey);
  while (HeapTupleIsValid(oldtup = systable_getnext(scan)))
  {

  }
  systable_endscan(scan);

  table_close(pg_shseclabel, RowExclusiveLock);
}

/*
 * DeleteSecurityLabel removes all security labels for an object (and any
 * sub-objects, if applicable).
 */
void
DeleteSecurityLabel(const ObjectAddress *object)
{
  Relation pg_seclabel;
  ScanKeyData skey[3];
  SysScanDesc scan;
  HeapTuple oldtup;
  int nkeys;

  /* Shared objects have their own security label catalog. */
  if (IsSharedRelation(object->classId))
  {



  }

  ScanKeyInit(&skey[0], Anum_pg_seclabel_objoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->objectId));
  ScanKeyInit(&skey[1], Anum_pg_seclabel_classoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->classId));
  if (object->objectSubId != 0)
  {
    ScanKeyInit(&skey[2], Anum_pg_seclabel_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(object->objectSubId));
    nkeys = 3;
  }
  else
  {
    nkeys = 2;
  }

  pg_seclabel = table_open(SecLabelRelationId, RowExclusiveLock);

  scan = systable_beginscan(pg_seclabel, SecLabelObjectIndexId, true, NULL, nkeys, skey);
  while (HeapTupleIsValid(oldtup = systable_getnext(scan)))
  {

  }
  systable_endscan(scan);

  table_close(pg_seclabel, RowExclusiveLock);
}

void
register_label_provider(const char *provider_name, check_object_relabel_type hook)
{









}