/*-------------------------------------------------------------------------
 *
 * pg_subscription.c
 *		replication subscriptions
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		src/backend/catalog/pg_subscription.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "miscadmin.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/xact.h"

#include "catalog/indexing.h"
#include "catalog/pg_type.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_subscription_rel.h"

#include "nodes/makefuncs.h"

#include "storage/lmgr.h"

#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/pg_lsn.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static List *
textarray_to_stringlist(ArrayType *textarray);

/*
 * Fetch the subscription from the syscache.
 */
Subscription *
GetSubscription(Oid subid, bool missing_ok)
{
  HeapTuple tup;
  Subscription *sub;
  Form_pg_subscription subform;
  Datum datum;
  bool isnull;

  tup = SearchSysCache1(SUBSCRIPTIONOID, ObjectIdGetDatum(subid));

  if (!HeapTupleIsValid(tup))
  {






  }

  subform = (Form_pg_subscription)GETSTRUCT(tup);

  sub = (Subscription *)palloc(sizeof(Subscription));
  sub->oid = subid;
  sub->dbid = subform->subdbid;
  sub->name = pstrdup(NameStr(subform->subname));
  sub->owner = subform->subowner;
  sub->enabled = subform->subenabled;

  /* Get conninfo */
  datum = SysCacheGetAttr(SUBSCRIPTIONOID, tup, Anum_pg_subscription_subconninfo, &isnull);
  Assert(!isnull);
  sub->conninfo = TextDatumGetCString(datum);

  /* Get slotname */
  datum = SysCacheGetAttr(SUBSCRIPTIONOID, tup, Anum_pg_subscription_subslotname, &isnull);
  if (!isnull)
  {
    sub->slotname = pstrdup(NameStr(*DatumGetName(datum)));
  }
  else
  {
    sub->slotname = NULL;
  }

  /* Get synccommit */
  datum = SysCacheGetAttr(SUBSCRIPTIONOID, tup, Anum_pg_subscription_subsynccommit, &isnull);
  Assert(!isnull);
  sub->synccommit = TextDatumGetCString(datum);

  /* Get publications */
  datum = SysCacheGetAttr(SUBSCRIPTIONOID, tup, Anum_pg_subscription_subpublications, &isnull);
  Assert(!isnull);
  sub->publications = textarray_to_stringlist(DatumGetArrayTypeP(datum));

  ReleaseSysCache(tup);

  return sub;
}

/*
 * Return number of subscriptions defined in given database.
 * Used by dropdb() to check if database can indeed be dropped.
 */
int
CountDBSubscriptions(Oid dbid)
{
  int nsubs = 0;
  Relation rel;
  ScanKeyData scankey;
  SysScanDesc scan;
  HeapTuple tup;

  rel = table_open(SubscriptionRelationId, RowExclusiveLock);

  ScanKeyInit(&scankey, Anum_pg_subscription_subdbid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(dbid));

  scan = systable_beginscan(rel, InvalidOid, false, NULL, 1, &scankey);

  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {

  }

  systable_endscan(scan);

  table_close(rel, NoLock);

  return nsubs;
}

/*
 * Free memory allocated by subscription struct.
 */
void
FreeSubscription(Subscription *sub)
{








}

/*
 * get_subscription_oid - given a subscription name, look up the OID
 *
 * If missing_ok is false, throw an error if name not found.  If true, just
 * return InvalidOid.
 */
Oid
get_subscription_oid(const char *subname, bool missing_ok)
{
  Oid oid;

  oid = GetSysCacheOid2(SUBSCRIPTIONNAME, Anum_pg_subscription_oid, MyDatabaseId, CStringGetDatum(subname));
  if (!OidIsValid(oid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("subscription \"%s\" does not exist", subname)));
  }
  return oid;
}

/*
 * get_subscription_name - given a subscription OID, look up the name
 *
 * If missing_ok is false, throw an error if name not found.  If true, just
 * return NULL.
 */
char *
get_subscription_name(Oid subid, bool missing_ok)
{
  HeapTuple tup;
  char *subname;
  Form_pg_subscription subform;

  tup = SearchSysCache1(SUBSCRIPTIONOID, ObjectIdGetDatum(subid));

  if (!HeapTupleIsValid(tup))
  {





  }

  subform = (Form_pg_subscription)GETSTRUCT(tup);
  subname = pstrdup(NameStr(subform->subname));

  ReleaseSysCache(tup);

  return subname;
}

/*
 * Convert text array to list of strings.
 *
 * Note: the resulting list of strings is pallocated here.
 */
static List *
textarray_to_stringlist(ArrayType *textarray)
{
  Datum *elems;
  int nelems, i;
  List *res = NIL;

  deconstruct_array(textarray, TEXTOID, -1, false, 'i', &elems, NULL, &nelems);

  if (nelems == 0)
  {

  }

  for (i = 0; i < nelems; i++)
  {
    res = lappend(res, makeString(TextDatumGetCString(elems[i])));
  }

  return res;
}

/*
 * Add new state record for a subscription table.
 */
void
AddSubscriptionRelState(Oid subid, Oid relid, char state, XLogRecPtr sublsn)
{








































}

/*
 * Update the state of a subscription table.
 */
void
UpdateSubscriptionRelState(Oid subid, Oid relid, char state, XLogRecPtr sublsn)
{










































}

/*
 * Get state of subscription table.
 *
 * Returns SUBREL_STATE_UNKNOWN when not found and missing_ok is true.
 */
char
GetSubscriptionRelState(Oid subid, Oid relid, XLogRecPtr *sublsn, bool missing_ok)
{










































}

/*
 * Drop subscription relation mapping. These can be for a particular
 * subscription, or for a particular relation, or both.
 */
void
RemoveSubscriptionRel(Oid subid, Oid relid)
{
  Relation rel;
  TableScanDesc scan;
  ScanKeyData skey[2];
  HeapTuple tup;
  int nkeys = 0;

  rel = table_open(SubscriptionRelRelationId, RowExclusiveLock);

  if (OidIsValid(subid))
  {
    ScanKeyInit(&skey[nkeys++], Anum_pg_subscription_rel_srsubid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(subid));
  }

  if (OidIsValid(relid))
  {
    ScanKeyInit(&skey[nkeys++], Anum_pg_subscription_rel_srrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));
  }

  /* Do the search and delete what we found. */
  scan = table_beginscan_catalog(rel, nkeys, skey);
  while (HeapTupleIsValid(tup = heap_getnext(scan, ForwardScanDirection)))
  {

  }
  table_endscan(scan);

  table_close(rel, RowExclusiveLock);
}

/*
 * Get all relations for subscription.
 *
 * Returned list is palloc'ed in current memory context.
 */
List *
GetSubscriptionRelations(Oid subid)
{











































}

/*
 * Get all relations for subscription that are not in a ready state.
 *
 * Returned list is palloc'ed in current memory context.
 */
List *
GetSubscriptionNotReadyRelations(Oid subid)
{













































}
