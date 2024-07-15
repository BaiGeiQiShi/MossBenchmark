                                                                            
   
                     
                              
   
                                                                         
                                                                        
   
                  
                                          
   
                                                                            
   

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
    if (missing_ok)
    {
      return NULL;
    }

    elog(ERROR, "cache lookup failed for subscription %u", subid);
  }

  subform = (Form_pg_subscription)GETSTRUCT(tup);

  sub = (Subscription *)palloc(sizeof(Subscription));
  sub->oid = subid;
  sub->dbid = subform->subdbid;
  sub->name = pstrdup(NameStr(subform->subname));
  sub->owner = subform->subowner;
  sub->enabled = subform->subenabled;

                    
  datum = SysCacheGetAttr(SUBSCRIPTIONOID, tup, Anum_pg_subscription_subconninfo, &isnull);
  Assert(!isnull);
  sub->conninfo = TextDatumGetCString(datum);

                    
  datum = SysCacheGetAttr(SUBSCRIPTIONOID, tup, Anum_pg_subscription_subslotname, &isnull);
  if (!isnull)
  {
    sub->slotname = pstrdup(NameStr(*DatumGetName(datum)));
  }
  else
  {
    sub->slotname = NULL;
  }

                      
  datum = SysCacheGetAttr(SUBSCRIPTIONOID, tup, Anum_pg_subscription_subsynccommit, &isnull);
  Assert(!isnull);
  sub->synccommit = TextDatumGetCString(datum);

                        
  datum = SysCacheGetAttr(SUBSCRIPTIONOID, tup, Anum_pg_subscription_subpublications, &isnull);
  Assert(!isnull);
  sub->publications = textarray_to_stringlist(DatumGetArrayTypeP(datum));

  ReleaseSysCache(tup);

  return sub;
}

   
                                                             
                                                                
   
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
    nsubs++;
  }

  systable_endscan(scan);

  table_close(rel, NoLock);

  return nsubs;
}

   
                                                 
   
void
FreeSubscription(Subscription *sub)
{
  pfree(sub->name);
  pfree(sub->conninfo);
  if (sub->slotname)
  {
    pfree(sub->slotname);
  }
  list_free_deep(sub->publications);
  pfree(sub);
}

   
                                                                     
   
                                                                            
                      
   
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

   
                                                                      
   
                                                                            
                
   
char *
get_subscription_name(Oid subid, bool missing_ok)
{
  HeapTuple tup;
  char *subname;
  Form_pg_subscription subform;

  tup = SearchSysCache1(SUBSCRIPTIONOID, ObjectIdGetDatum(subid));

  if (!HeapTupleIsValid(tup))
  {
    if (!missing_ok)
    {
      elog(ERROR, "cache lookup failed for subscription %u", subid);
    }
    return NULL;
  }

  subform = (Form_pg_subscription)GETSTRUCT(tup);
  subname = pstrdup(NameStr(subform->subname));

  ReleaseSysCache(tup);

  return subname;
}

   
                                          
   
                                                           
   
static List *
textarray_to_stringlist(ArrayType *textarray)
{
  Datum *elems;
  int nelems, i;
  List *res = NIL;

  deconstruct_array(textarray, TEXTOID, -1, false, 'i', &elems, NULL, &nelems);

  if (nelems == 0)
  {
    return NIL;
  }

  for (i = 0; i < nelems; i++)
  {
    res = lappend(res, makeString(TextDatumGetCString(elems[i])));
  }

  return res;
}

   
                                                  
   
void
AddSubscriptionRelState(Oid subid, Oid relid, char state, XLogRecPtr sublsn)
{
  Relation rel;
  HeapTuple tup;
  bool nulls[Natts_pg_subscription_rel];
  Datum values[Natts_pg_subscription_rel];

  LockSharedObject(SubscriptionRelationId, subid, 0, AccessShareLock);

  rel = table_open(SubscriptionRelRelationId, RowExclusiveLock);

                                     
  tup = SearchSysCacheCopy2(SUBSCRIPTIONRELMAP, ObjectIdGetDatum(relid), ObjectIdGetDatum(subid));
  if (HeapTupleIsValid(tup))
  {
    elog(ERROR, "subscription table %u in subscription %u already exists", relid, subid);
  }

                       
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));
  values[Anum_pg_subscription_rel_srsubid - 1] = ObjectIdGetDatum(subid);
  values[Anum_pg_subscription_rel_srrelid - 1] = ObjectIdGetDatum(relid);
  values[Anum_pg_subscription_rel_srsubstate - 1] = CharGetDatum(state);
  if (sublsn != InvalidXLogRecPtr)
  {
    values[Anum_pg_subscription_rel_srsublsn - 1] = LSNGetDatum(sublsn);
  }
  else
  {
    nulls[Anum_pg_subscription_rel_srsublsn - 1] = true;
  }

  tup = heap_form_tuple(RelationGetDescr(rel), values, nulls);

                                  
  CatalogTupleInsert(rel, tup);

  heap_freetuple(tup);

                
  table_close(rel, NoLock);
}

   
                                             
   
void
UpdateSubscriptionRelState(Oid subid, Oid relid, char state, XLogRecPtr sublsn)
{
  Relation rel;
  HeapTuple tup;
  bool nulls[Natts_pg_subscription_rel];
  Datum values[Natts_pg_subscription_rel];
  bool replaces[Natts_pg_subscription_rel];

  LockSharedObject(SubscriptionRelationId, subid, 0, AccessShareLock);

  rel = table_open(SubscriptionRelRelationId, RowExclusiveLock);

                                     
  tup = SearchSysCacheCopy2(SUBSCRIPTIONRELMAP, ObjectIdGetDatum(relid), ObjectIdGetDatum(subid));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "subscription table %u in subscription %u does not exist", relid, subid);
  }

                         
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));
  memset(replaces, false, sizeof(replaces));

  replaces[Anum_pg_subscription_rel_srsubstate - 1] = true;
  values[Anum_pg_subscription_rel_srsubstate - 1] = CharGetDatum(state);

  replaces[Anum_pg_subscription_rel_srsublsn - 1] = true;
  if (sublsn != InvalidXLogRecPtr)
  {
    values[Anum_pg_subscription_rel_srsublsn - 1] = LSNGetDatum(sublsn);
  }
  else
  {
    nulls[Anum_pg_subscription_rel_srsublsn - 1] = true;
  }

  tup = heap_modify_tuple(tup, RelationGetDescr(rel), values, nulls, replaces);

                           
  CatalogTupleUpdate(rel, &tup->t_self, tup);

                
  table_close(rel, NoLock);
}

   
                                    
   
                                                                       
   
char
GetSubscriptionRelState(Oid subid, Oid relid, XLogRecPtr *sublsn, bool missing_ok)
{
  Relation rel;
  HeapTuple tup;
  char substate;
  bool isnull;
  Datum d;

  rel = table_open(SubscriptionRelRelationId, AccessShareLock);

                                
  tup = SearchSysCache2(SUBSCRIPTIONRELMAP, ObjectIdGetDatum(relid), ObjectIdGetDatum(subid));

  if (!HeapTupleIsValid(tup))
  {
    if (missing_ok)
    {
      table_close(rel, AccessShareLock);
      *sublsn = InvalidXLogRecPtr;
      return SUBREL_STATE_UNKNOWN;
    }

    elog(ERROR, "subscription table %u in subscription %u does not exist", relid, subid);
  }

                      
  d = SysCacheGetAttr(SUBSCRIPTIONRELMAP, tup, Anum_pg_subscription_rel_srsubstate, &isnull);
  Assert(!isnull);
  substate = DatumGetChar(d);
  d = SysCacheGetAttr(SUBSCRIPTIONRELMAP, tup, Anum_pg_subscription_rel_srsublsn, &isnull);
  if (isnull)
  {
    *sublsn = InvalidXLogRecPtr;
  }
  else
  {
    *sublsn = DatumGetLSN(d);
  }

               
  ReleaseSysCache(tup);
  table_close(rel, AccessShareLock);

  return substate;
}

   
                                                                     
                                                        
   
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

                                               
  scan = table_beginscan_catalog(rel, nkeys, skey);
  while (HeapTupleIsValid(tup = heap_getnext(scan, ForwardScanDirection)))
  {
    CatalogTupleDelete(rel, &tup->t_self);
  }
  table_endscan(scan);

  table_close(rel, RowExclusiveLock);
}

   
                                       
   
                                                         
   
List *
GetSubscriptionRelations(Oid subid)
{
  List *res = NIL;
  Relation rel;
  HeapTuple tup;
  int nkeys = 0;
  ScanKeyData skey[2];
  SysScanDesc scan;

  rel = table_open(SubscriptionRelRelationId, AccessShareLock);

  ScanKeyInit(&skey[nkeys++], Anum_pg_subscription_rel_srsubid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(subid));

  scan = systable_beginscan(rel, InvalidOid, false, NULL, nkeys, skey);

  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    Form_pg_subscription_rel subrel;
    SubscriptionRelState *relstate;
    Datum d;
    bool isnull;

    subrel = (Form_pg_subscription_rel)GETSTRUCT(tup);

    relstate = (SubscriptionRelState *)palloc(sizeof(SubscriptionRelState));
    relstate->relid = subrel->srrelid;
    relstate->state = subrel->srsubstate;
    d = SysCacheGetAttr(SUBSCRIPTIONRELMAP, tup, Anum_pg_subscription_rel_srsublsn, &isnull);
    if (isnull)
    {
      relstate->lsn = InvalidXLogRecPtr;
    }
    else
    {
      relstate->lsn = DatumGetLSN(d);
    }

    res = lappend(res, relstate);
  }

               
  systable_endscan(scan);
  table_close(rel, AccessShareLock);

  return res;
}

   
                                                                     
   
                                                         
   
List *
GetSubscriptionNotReadyRelations(Oid subid)
{
  List *res = NIL;
  Relation rel;
  HeapTuple tup;
  int nkeys = 0;
  ScanKeyData skey[2];
  SysScanDesc scan;

  rel = table_open(SubscriptionRelRelationId, AccessShareLock);

  ScanKeyInit(&skey[nkeys++], Anum_pg_subscription_rel_srsubid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(subid));

  ScanKeyInit(&skey[nkeys++], Anum_pg_subscription_rel_srsubstate, BTEqualStrategyNumber, F_CHARNE, CharGetDatum(SUBREL_STATE_READY));

  scan = systable_beginscan(rel, InvalidOid, false, NULL, nkeys, skey);

  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    Form_pg_subscription_rel subrel;
    SubscriptionRelState *relstate;
    Datum d;
    bool isnull;

    subrel = (Form_pg_subscription_rel)GETSTRUCT(tup);

    relstate = (SubscriptionRelState *)palloc(sizeof(SubscriptionRelState));
    relstate->relid = subrel->srrelid;
    relstate->state = subrel->srsubstate;
    d = SysCacheGetAttr(SUBSCRIPTIONRELMAP, tup, Anum_pg_subscription_rel_srsublsn, &isnull);
    if (isnull)
    {
      relstate->lsn = InvalidXLogRecPtr;
    }
    else
    {
      relstate->lsn = DatumGetLSN(d);
    }

    res = lappend(res, relstate);
  }

               
  systable_endscan(scan);
  table_close(rel, AccessShareLock);

  return res;
}
