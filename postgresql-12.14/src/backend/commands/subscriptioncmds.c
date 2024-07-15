                                                                            
   
                      
                                                
   
                                                                         
                                                                        
   
                  
                       
   
                                                                            
   

#include "postgres.h"

#include "miscadmin.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"

#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/objectaddress.h"
#include "catalog/pg_type.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_subscription_rel.h"

#include "commands/defrem.h"
#include "commands/event_trigger.h"
#include "commands/subscriptioncmds.h"

#include "executor/executor.h"

#include "nodes/makefuncs.h"

#include "replication/logicallauncher.h"
#include "replication/origin.h"
#include "replication/slot.h"
#include "replication/walreceiver.h"
#include "replication/walsender.h"
#include "replication/worker_internal.h"

#include "storage/lmgr.h"

#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/syscache.h"

static List *
fetch_table_list(WalReceiverConn *wrconn, List *publications);

   
                                                                              
   
                                                                          
                                                                           
                     
   
static void
parse_subscription_options(List *options, bool *connect, bool *enabled_given, bool *enabled, bool *create_slot, bool *slot_name_given, char **slot_name, bool *copy_data, char **synchronous_commit, bool *refresh)
{
  ListCell *lc;
  bool connect_given = false;
  bool create_slot_given = false;
  bool copy_data_given = false;
  bool refresh_given = false;

                                                            
  Assert(!connect || (enabled && create_slot && copy_data));

  if (connect)
  {
    *connect = true;
  }
  if (enabled)
  {
    *enabled_given = false;
    *enabled = true;
  }
  if (create_slot)
  {
    *create_slot = true;
  }
  if (slot_name)
  {
    *slot_name_given = false;
    *slot_name = NULL;
  }
  if (copy_data)
  {
    *copy_data = true;
  }
  if (synchronous_commit)
  {
    *synchronous_commit = NULL;
  }
  if (refresh)
  {
    *refresh = true;
  }

                     
  foreach (lc, options)
  {
    DefElem *defel = (DefElem *)lfirst(lc);

    if (strcmp(defel->defname, "connect") == 0 && connect)
    {
      if (connect_given)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }

      connect_given = true;
      *connect = defGetBoolean(defel);
    }
    else if (strcmp(defel->defname, "enabled") == 0 && enabled)
    {
      if (*enabled_given)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }

      *enabled_given = true;
      *enabled = defGetBoolean(defel);
    }
    else if (strcmp(defel->defname, "create_slot") == 0 && create_slot)
    {
      if (create_slot_given)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }

      create_slot_given = true;
      *create_slot = defGetBoolean(defel);
    }
    else if (strcmp(defel->defname, "slot_name") == 0 && slot_name)
    {
      if (*slot_name_given)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }

      *slot_name_given = true;
      *slot_name = defGetString(defel);

                                                                
      if (strcmp(*slot_name, "none") == 0)
      {
        *slot_name = NULL;
      }
      else
      {
        ReplicationSlotValidateName(*slot_name, ERROR);
      }
    }
    else if (strcmp(defel->defname, "copy_data") == 0 && copy_data)
    {
      if (copy_data_given)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }

      copy_data_given = true;
      *copy_data = defGetBoolean(defel);
    }
    else if (strcmp(defel->defname, "synchronous_commit") == 0 && synchronous_commit)
    {
      if (*synchronous_commit)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }

      *synchronous_commit = defGetString(defel);

                                                                        
      (void)set_config_option("synchronous_commit", *synchronous_commit, PGC_BACKEND, PGC_S_TEST, GUC_ACTION_SET, false, 0, false);
    }
    else if (strcmp(defel->defname, "refresh") == 0 && refresh)
    {
      if (refresh_given)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("conflicting or redundant options")));
      }

      refresh_given = true;
      *refresh = defGetBoolean(defel);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("unrecognized subscription parameter: \"%s\"", defel->defname)));
    }
  }

     
                                                                    
                            
     
  if (connect && !*connect)
  {
                                                       
    if (enabled && *enabled_given && *enabled)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
                                                                                            
                         errmsg("%s and %s are mutually exclusive options", "connect = false", "enabled = true")));
    }

    if (create_slot && create_slot_given && *create_slot)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s and %s are mutually exclusive options", "connect = false", "create_slot = true")));
    }

    if (copy_data && copy_data_given && *copy_data)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s and %s are mutually exclusive options", "connect = false", "copy_data = true")));
    }

                                               
    *enabled = false;
    *create_slot = false;
    *copy_data = false;
  }

     
                                                                             
               
     
  if (slot_name && *slot_name_given && !*slot_name)
  {
    if (enabled && *enabled_given && *enabled)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
                                                                                            
                         errmsg("%s and %s are mutually exclusive options", "slot_name = NONE", "enabled = true")));
    }

    if (create_slot && create_slot_given && *create_slot)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("%s and %s are mutually exclusive options", "slot_name = NONE", "create_slot = true")));
    }

    if (enabled && !*enabled_given && *enabled)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR),
                                                                                            
                         errmsg("subscription with %s must also set %s", "slot_name = NONE", "enabled = false")));
    }

    if (create_slot && !create_slot_given && *create_slot)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("subscription with %s must also set %s", "slot_name = NONE", "create_slot = false")));
    }
  }
}

   
                                                                           
   
static Datum
publicationListToArray(List *publist)
{
  ArrayType *arr;
  Datum *datums;
  int j = 0;
  ListCell *cell;
  MemoryContext memcxt;
  MemoryContext oldcxt;

                                                        
  memcxt = AllocSetContextCreate(CurrentMemoryContext, "publicationListToArray to array", ALLOCSET_DEFAULT_SIZES);
  oldcxt = MemoryContextSwitchTo(memcxt);

  datums = (Datum *)palloc(sizeof(Datum) * list_length(publist));

  foreach (cell, publist)
  {
    char *name = strVal(lfirst(cell));
    ListCell *pcell;

                               
    foreach (pcell, publist)
    {
      char *pname = strVal(lfirst(pcell));

      if (pcell == cell)
      {
        break;
      }

      if (strcmp(name, pname) == 0)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("publication name \"%s\" used more than once", pname)));
      }
    }

    datums[j++] = CStringGetTextDatum(name);
  }

  MemoryContextSwitchTo(oldcxt);

  arr = construct_array(datums, list_length(publist), TEXTOID, -1, false, 'i');

  MemoryContextDelete(memcxt);

  return PointerGetDatum(arr);
}

   
                            
   
ObjectAddress
CreateSubscription(CreateSubscriptionStmt *stmt, bool isTopLevel)
{
  Relation rel;
  ObjectAddress myself;
  Oid subid;
  bool nulls[Natts_pg_subscription];
  Datum values[Natts_pg_subscription];
  Oid owner = GetUserId();
  HeapTuple tup;
  bool connect;
  bool enabled_given;
  bool enabled;
  bool copy_data;
  char *synchronous_commit;
  char *conninfo;
  char *slotname;
  bool slotname_given;
  char originname[NAMEDATALEN];
  bool create_slot;
  List *publications;

     
                              
     
                                                              
     
  parse_subscription_options(stmt->options, &connect, &enabled_given, &enabled, &create_slot, &slotname_given, &slotname, &copy_data, &synchronous_commit, NULL);

     
                                                                          
                                                                            
                                                                  
                       
     
  if (create_slot)
  {
    PreventInTransactionBlock(isTopLevel, "CREATE SUBSCRIPTION ... WITH (create_slot = true)");
  }

  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), (errmsg("must be superuser to create subscriptions"))));
  }

     
                                                                     
                                                      
     
#ifdef ENFORCE_REGRESSION_TEST_NAME_RESTRICTIONS
  if (strncmp(stmt->subname, "regress_", 8) != 0)
  {
    elog(WARNING, "subscriptions created by regression test cases should have names starting with \"regress_\"");
  }
#endif

  rel = table_open(SubscriptionRelationId, RowExclusiveLock);

                             
  subid = GetSysCacheOid2(SUBSCRIPTIONNAME, Anum_pg_subscription_oid, MyDatabaseId, CStringGetDatum(stmt->subname));
  if (OidIsValid(subid))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("subscription \"%s\" already exists", stmt->subname)));
  }

  if (!slotname_given && slotname == NULL)
  {
    slotname = stmt->subname;
  }

                                                                   
  if (synchronous_commit == NULL)
  {
    synchronous_commit = "off";
  }

  conninfo = stmt->conninfo;
  publications = stmt->publication;

                                                  
  load_file("libpqwalreceiver", false);

                                         
  walrcv_check_conninfo(conninfo);

                                        
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));

  subid = GetNewOidWithIndex(rel, SubscriptionObjectIndexId, Anum_pg_subscription_oid);
  values[Anum_pg_subscription_oid - 1] = ObjectIdGetDatum(subid);
  values[Anum_pg_subscription_subdbid - 1] = ObjectIdGetDatum(MyDatabaseId);
  values[Anum_pg_subscription_subname - 1] = DirectFunctionCall1(namein, CStringGetDatum(stmt->subname));
  values[Anum_pg_subscription_subowner - 1] = ObjectIdGetDatum(owner);
  values[Anum_pg_subscription_subenabled - 1] = BoolGetDatum(enabled);
  values[Anum_pg_subscription_subconninfo - 1] = CStringGetTextDatum(conninfo);
  if (slotname)
  {
    values[Anum_pg_subscription_subslotname - 1] = DirectFunctionCall1(namein, CStringGetDatum(slotname));
  }
  else
  {
    nulls[Anum_pg_subscription_subslotname - 1] = true;
  }
  values[Anum_pg_subscription_subsynccommit - 1] = CStringGetTextDatum(synchronous_commit);
  values[Anum_pg_subscription_subpublications - 1] = publicationListToArray(publications);

  tup = heap_form_tuple(RelationGetDescr(rel), values, nulls);

                                  
  CatalogTupleInsert(rel, tup);
  heap_freetuple(tup);

  recordDependencyOnOwner(SubscriptionRelationId, subid, owner);

  snprintf(originname, sizeof(originname), "pg_%u", subid);
  replorigin_create(originname);

     
                                                                          
           
     
  if (connect)
  {
    XLogRecPtr lsn;
    char *err;
    WalReceiverConn *wrconn;
    List *tables;
    ListCell *lc;
    char table_state;

                                          
    wrconn = walrcv_connect(conninfo, true, stmt->subname, &err);
    if (!wrconn)
    {
      ereport(ERROR, (errmsg("could not connect to the publisher: %s", err)));
    }

    PG_TRY();
    {
         
                                                                     
              
         
      table_state = copy_data ? SUBREL_STATE_INIT : SUBREL_STATE_READY;

         
                                                                        
               
         
      tables = fetch_table_list(wrconn, publications);
      foreach (lc, tables)
      {
        RangeVar *rv = (RangeVar *)lfirst(lc);
        Oid relid;

        relid = RangeVarGetRelid(rv, AccessShareLock, false);

                                          
        CheckSubscriptionRelkind(get_rel_relkind(relid), rv->schemaname, rv->relname);

        AddSubscriptionRelState(subid, relid, table_state, InvalidXLogRecPtr);
      }

         
                                                                      
                                                                    
                    
         
      if (create_slot)
      {
        Assert(slotname);

        walrcv_create_slot(wrconn, slotname, false, CRS_NOEXPORT_SNAPSHOT, &lsn);
        ereport(NOTICE, (errmsg("created replication slot \"%s\" on publisher", slotname)));
      }
    }
    PG_CATCH();
    {
                                                    
      walrcv_disconnect(wrconn);
      PG_RE_THROW();
    }
    PG_END_TRY();

                                               
    walrcv_disconnect(wrconn);
  }
  else
  {
    ereport(WARNING,
                                                      
        (errmsg("tables were not subscribed, you will have to run %s to subscribe the tables", "ALTER SUBSCRIPTION ... REFRESH PUBLICATION")));
  }

  table_close(rel, RowExclusiveLock);

  if (enabled)
  {
    ApplyLauncherWakeupAtCommit();
  }

  ObjectAddressSet(myself, SubscriptionRelationId, subid);

  InvokeObjectPostCreateHook(SubscriptionRelationId, subid, 0);

  return myself;
}

static void
AlterSubscription_refresh(Subscription *sub, bool copy_data)
{
  char *err;
  List *pubrel_names;
  List *subrel_states;
  Oid *subrel_local_oids;
  Oid *pubrel_local_oids;
  WalReceiverConn *wrconn;
  ListCell *lc;
  int off;

                                                  
  load_file("libpqwalreceiver", false);

                                        
  wrconn = walrcv_connect(sub->conninfo, true, sub->name, &err);
  if (!wrconn)
  {
    ereport(ERROR, (errmsg("could not connect to the publisher: %s", err)));
  }

                                          
  pubrel_names = fetch_table_list(wrconn, sub->publications);

                                                           
  walrcv_disconnect(wrconn);

                             
  subrel_states = GetSubscriptionRelations(sub->oid);

     
                                                                         
                                                                          
                
     
  subrel_local_oids = palloc(list_length(subrel_states) * sizeof(Oid));
  off = 0;
  foreach (lc, subrel_states)
  {
    SubscriptionRelState *relstate = (SubscriptionRelState *)lfirst(lc);

    subrel_local_oids[off++] = relstate->relid;
  }
  qsort(subrel_local_oids, list_length(subrel_states), sizeof(Oid), oid_cmp);

     
                                                                        
                                                                          
     
                                                                         
     
  off = 0;
  pubrel_local_oids = palloc(list_length(pubrel_names) * sizeof(Oid));

  foreach (lc, pubrel_names)
  {
    RangeVar *rv = (RangeVar *)lfirst(lc);
    Oid relid;

    relid = RangeVarGetRelid(rv, AccessShareLock, false);

                                      
    CheckSubscriptionRelkind(get_rel_relkind(relid), rv->schemaname, rv->relname);

    pubrel_local_oids[off++] = relid;

    if (!bsearch(&relid, subrel_local_oids, list_length(subrel_states), sizeof(Oid), oid_cmp))
    {
      AddSubscriptionRelState(sub->oid, relid, copy_data ? SUBREL_STATE_INIT : SUBREL_STATE_READY, InvalidXLogRecPtr);
      ereport(DEBUG1, (errmsg("table \"%s.%s\" added to subscription \"%s\"", rv->schemaname, rv->relname, sub->name)));
    }
  }

     
                                                                             
                             
     
  qsort(pubrel_local_oids, list_length(pubrel_names), sizeof(Oid), oid_cmp);

  for (off = 0; off < list_length(subrel_states); off++)
  {
    Oid relid = subrel_local_oids[off];

    if (!bsearch(&relid, pubrel_local_oids, list_length(pubrel_names), sizeof(Oid), oid_cmp))
    {
      RemoveSubscriptionRel(sub->oid, relid);

      logicalrep_worker_stop_at_commit(sub->oid, relid);

      ereport(DEBUG1, (errmsg("table \"%s.%s\" removed from subscription \"%s\"", get_namespace_name(get_rel_namespace(relid)), get_rel_name(relid), sub->name)));
    }
  }
}

   
                                    
   
ObjectAddress
AlterSubscription(AlterSubscriptionStmt *stmt)
{
  Relation rel;
  ObjectAddress myself;
  bool nulls[Natts_pg_subscription];
  bool replaces[Natts_pg_subscription];
  Datum values[Natts_pg_subscription];
  HeapTuple tup;
  Oid subid;
  bool update_tuple = false;
  Subscription *sub;
  Form_pg_subscription form;

  rel = table_open(SubscriptionRelationId, RowExclusiveLock);

                                 
  tup = SearchSysCacheCopy2(SUBSCRIPTIONNAME, MyDatabaseId, CStringGetDatum(stmt->subname));

  if (!HeapTupleIsValid(tup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("subscription \"%s\" does not exist", stmt->subname)));
  }

  form = (Form_pg_subscription)GETSTRUCT(tup);
  subid = form->oid;

                     
  if (!pg_subscription_ownercheck(subid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_SUBSCRIPTION, stmt->subname);
  }

  sub = GetSubscription(subid, false);

                                                                     
  LockSharedObject(SubscriptionRelationId, subid, 0, AccessExclusiveLock);

                         
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));
  memset(replaces, false, sizeof(replaces));

  switch (stmt->kind)
  {
  case ALTER_SUBSCRIPTION_OPTIONS:
  {
    char *slotname;
    bool slotname_given;
    char *synchronous_commit;

    parse_subscription_options(stmt->options, NULL, NULL, NULL, NULL, &slotname_given, &slotname, NULL, &synchronous_commit, NULL);

    if (slotname_given)
    {
      if (sub->enabled && !slotname)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot set %s for enabled subscription", "slot_name = NONE")));
      }

      if (slotname)
      {
        values[Anum_pg_subscription_subslotname - 1] = DirectFunctionCall1(namein, CStringGetDatum(slotname));
      }
      else
      {
        nulls[Anum_pg_subscription_subslotname - 1] = true;
      }
      replaces[Anum_pg_subscription_subslotname - 1] = true;
    }

    if (synchronous_commit)
    {
      values[Anum_pg_subscription_subsynccommit - 1] = CStringGetTextDatum(synchronous_commit);
      replaces[Anum_pg_subscription_subsynccommit - 1] = true;
    }

    update_tuple = true;
    break;
  }

  case ALTER_SUBSCRIPTION_ENABLED:
  {
    bool enabled, enabled_given;

    parse_subscription_options(stmt->options, NULL, &enabled_given, &enabled, NULL, NULL, NULL, NULL, NULL, NULL);
    Assert(enabled_given);

    if (!sub->slotname && enabled)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot enable subscription that does not have a slot name")));
    }

    values[Anum_pg_subscription_subenabled - 1] = BoolGetDatum(enabled);
    replaces[Anum_pg_subscription_subenabled - 1] = true;

    if (enabled)
    {
      ApplyLauncherWakeupAtCommit();
    }

    update_tuple = true;
    break;
  }

  case ALTER_SUBSCRIPTION_CONNECTION:
                                                    
    load_file("libpqwalreceiver", false);
                                           
    walrcv_check_conninfo(stmt->conninfo);

    values[Anum_pg_subscription_subconninfo - 1] = CStringGetTextDatum(stmt->conninfo);
    replaces[Anum_pg_subscription_subconninfo - 1] = true;
    update_tuple = true;
    break;

  case ALTER_SUBSCRIPTION_PUBLICATION:
  {
    bool copy_data;
    bool refresh;

    parse_subscription_options(stmt->options, NULL, NULL, NULL, NULL, NULL, NULL, &copy_data, NULL, &refresh);

    values[Anum_pg_subscription_subpublications - 1] = publicationListToArray(stmt->publication);
    replaces[Anum_pg_subscription_subpublications - 1] = true;

    update_tuple = true;

                                      
    if (refresh)
    {
      if (!sub->enabled)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("ALTER SUBSCRIPTION with refresh is not allowed for disabled subscriptions"), errhint("Use ALTER SUBSCRIPTION ... SET PUBLICATION ... WITH (refresh = false).")));
      }

                                                                
      sub->publications = stmt->publication;

      AlterSubscription_refresh(sub, copy_data);
    }

    break;
  }

  case ALTER_SUBSCRIPTION_REFRESH:
  {
    bool copy_data;

    if (!sub->enabled)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("ALTER SUBSCRIPTION ... REFRESH is not allowed for disabled subscriptions")));
    }

    parse_subscription_options(stmt->options, NULL, NULL, NULL, NULL, NULL, NULL, &copy_data, NULL, NULL);

    AlterSubscription_refresh(sub, copy_data);

    break;
  }

  default:
    elog(ERROR, "unrecognized ALTER SUBSCRIPTION kind %d", stmt->kind);
  }

                                     
  if (update_tuple)
  {
    tup = heap_modify_tuple(tup, RelationGetDescr(rel), values, nulls, replaces);

    CatalogTupleUpdate(rel, &tup->t_self, tup);

    heap_freetuple(tup);
  }

  table_close(rel, RowExclusiveLock);

  ObjectAddressSet(myself, SubscriptionRelationId, subid);

  InvokeObjectPostAlterHook(SubscriptionRelationId, subid, 0);

  return myself;
}

   
                       
   
void
DropSubscription(DropSubscriptionStmt *stmt, bool isTopLevel)
{
  Relation rel;
  ObjectAddress myself;
  HeapTuple tup;
  Oid subid;
  Datum datum;
  bool isnull;
  char *subname;
  char *conninfo;
  char *slotname;
  List *subworkers;
  ListCell *lc;
  char originname[NAMEDATALEN];
  char *err = NULL;
  RepOriginId originid;
  WalReceiverConn *wrconn;
  StringInfoData cmd;
  Form_pg_subscription form;

     
                                                                      
                                                                          
     
  rel = table_open(SubscriptionRelationId, AccessExclusiveLock);

  tup = SearchSysCache2(SUBSCRIPTIONNAME, MyDatabaseId, CStringGetDatum(stmt->subname));

  if (!HeapTupleIsValid(tup))
  {
    table_close(rel, NoLock);

    if (!stmt->missing_ok)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("subscription \"%s\" does not exist", stmt->subname)));
    }
    else
    {
      ereport(NOTICE, (errmsg("subscription \"%s\" does not exist, skipping", stmt->subname)));
    }

    return;
  }

  form = (Form_pg_subscription)GETSTRUCT(tup);
  subid = form->oid;

                     
  if (!pg_subscription_ownercheck(subid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_SUBSCRIPTION, stmt->subname);
  }

                                                    
  InvokeObjectDropHook(SubscriptionRelationId, subid, 0);

     
                                                                             
                               
     
  LockSharedObject(SubscriptionRelationId, subid, 0, AccessExclusiveLock);

                   
  datum = SysCacheGetAttr(SUBSCRIPTIONOID, tup, Anum_pg_subscription_subname, &isnull);
  Assert(!isnull);
  subname = pstrdup(NameStr(*DatumGetName(datum)));

                    
  datum = SysCacheGetAttr(SUBSCRIPTIONOID, tup, Anum_pg_subscription_subconninfo, &isnull);
  Assert(!isnull);
  conninfo = TextDatumGetCString(datum);

                    
  datum = SysCacheGetAttr(SUBSCRIPTIONOID, tup, Anum_pg_subscription_subslotname, &isnull);
  if (!isnull)
  {
    slotname = pstrdup(NameStr(*DatumGetName(datum)));
  }
  else
  {
    slotname = NULL;
  }

     
                                                                             
                                                                          
                                                                      
                       
     
                                                                             
                                                                           
                                                
     
  if (slotname)
  {
    PreventInTransactionBlock(isTopLevel, "DROP SUBSCRIPTION");
  }

  ObjectAddressSet(myself, SubscriptionRelationId, subid);
  EventTriggerSQLDropAddObject(&myself, true, true);

                                      
  CatalogTupleDelete(rel, &tup->t_self);

  ReleaseSysCache(tup);

     
                                                    
     
                                                                            
                              
     
                                                                           
                                                                           
                                                                           
                                                                           
                                                                         
                              
     
                                                                           
                                                   
     
  LWLockAcquire(LogicalRepWorkerLock, LW_SHARED);
  subworkers = logicalrep_workers_find(subid, false);
  LWLockRelease(LogicalRepWorkerLock);
  foreach (lc, subworkers)
  {
    LogicalRepWorker *w = (LogicalRepWorker *)lfirst(lc);

    logicalrep_worker_stop(w->subid, w->relid);
  }
  list_free(subworkers);

                             
  deleteSharedDependencyRecordsFor(SubscriptionRelationId, subid, 0);

                                                              
  RemoveSubscriptionRel(subid, InvalidOid);

                                             
  snprintf(originname, sizeof(originname), "pg_%u", subid);
  originid = replorigin_by_name(originname, true);
  if (originid != InvalidRepOriginId)
  {
    replorigin_drop(originid, false);
  }

     
                                                                         
           
     
  if (!slotname)
  {
    table_close(rel, NoLock);
    return;
  }

     
                                                                         
                             
     
  load_file("libpqwalreceiver", false);

  initStringInfo(&cmd);
  appendStringInfo(&cmd, "DROP_REPLICATION_SLOT %s WAIT", quote_identifier(slotname));

  wrconn = walrcv_connect(conninfo, true, subname, &err);
  if (wrconn == NULL)
  {
    ereport(ERROR, (errmsg("could not connect to publisher when attempting to "
                           "drop the replication slot \"%s\"",
                        slotname),
                       errdetail("The error was: %s", err),
                                                                   
                       errhint("Use %s to disassociate the subscription from the slot.", "ALTER SUBSCRIPTION ... SET (slot_name = NONE)")));
  }

  PG_TRY();
  {
    WalRcvExecResult *res;

    res = walrcv_exec(wrconn, cmd.data, 0, NULL);

    if (res->status != WALRCV_OK_COMMAND)
    {
      ereport(ERROR, (errmsg("could not drop the replication slot \"%s\" on publisher", slotname), errdetail("The error was: %s", res->err)));
    }
    else
    {
      ereport(NOTICE, (errmsg("dropped replication slot \"%s\" on publisher", slotname)));
    }

    walrcv_clear_result(res);
  }
  PG_CATCH();
  {
                                                 
    walrcv_disconnect(wrconn);
    PG_RE_THROW();
  }
  PG_END_TRY();

  walrcv_disconnect(wrconn);

  pfree(cmd.data);

  table_close(rel, NoLock);
}

   
                                                        
   
static void
AlterSubscriptionOwner_internal(Relation rel, HeapTuple tup, Oid newOwnerId)
{
  Form_pg_subscription form;

  form = (Form_pg_subscription)GETSTRUCT(tup);

  if (form->subowner == newOwnerId)
  {
    return;
  }

  if (!pg_subscription_ownercheck(form->oid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_SUBSCRIPTION, NameStr(form->subname));
  }

                                     
  if (!superuser_arg(newOwnerId))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to change owner of subscription \"%s\"", NameStr(form->subname)), errhint("The owner of a subscription must be a superuser.")));
  }

  form->subowner = newOwnerId;
  CatalogTupleUpdate(rel, &tup->t_self, tup);

                                         
  changeDependencyOnOwner(SubscriptionRelationId, form->oid, newOwnerId);

  InvokeObjectPostAlterHook(SubscriptionRelationId, form->oid, 0);
}

   
                                        
   
ObjectAddress
AlterSubscriptionOwner(const char *name, Oid newOwnerId)
{
  Oid subid;
  HeapTuple tup;
  Relation rel;
  ObjectAddress address;
  Form_pg_subscription form;

  rel = table_open(SubscriptionRelationId, RowExclusiveLock);

  tup = SearchSysCacheCopy2(SUBSCRIPTIONNAME, MyDatabaseId, CStringGetDatum(name));

  if (!HeapTupleIsValid(tup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("subscription \"%s\" does not exist", name)));
  }

  form = (Form_pg_subscription)GETSTRUCT(tup);
  subid = form->oid;

  AlterSubscriptionOwner_internal(rel, tup, newOwnerId);

  ObjectAddressSet(address, SubscriptionRelationId, subid);

  heap_freetuple(tup);

  table_close(rel, RowExclusiveLock);

  return address;
}

   
                                       
   
void
AlterSubscriptionOwner_oid(Oid subid, Oid newOwnerId)
{
  HeapTuple tup;
  Relation rel;

  rel = table_open(SubscriptionRelationId, RowExclusiveLock);

  tup = SearchSysCacheCopy1(SUBSCRIPTIONOID, ObjectIdGetDatum(subid));

  if (!HeapTupleIsValid(tup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("subscription with OID %u does not exist", subid)));
  }

  AlterSubscriptionOwner_internal(rel, tup, newOwnerId);

  heap_freetuple(tup);

  table_close(rel, RowExclusiveLock);
}

   
                                                                        
                         
   
static List *
fetch_table_list(WalReceiverConn *wrconn, List *publications)
{
  WalRcvExecResult *res;
  StringInfoData cmd;
  TupleTableSlot *slot;
  Oid tableRow[2] = {TEXTOID, TEXTOID};
  ListCell *lc;
  bool first;
  List *tablelist = NIL;

  Assert(list_length(publications) > 0);

  initStringInfo(&cmd);
  appendStringInfoString(&cmd, "SELECT DISTINCT t.schemaname, t.tablename\n"
                               "  FROM pg_catalog.pg_publication_tables t\n"
                               " WHERE t.pubname IN (");
  first = true;
  foreach (lc, publications)
  {
    char *pubname = strVal(lfirst(lc));

    if (first)
    {
      first = false;
    }
    else
    {
      appendStringInfoString(&cmd, ", ");
    }

    appendStringInfoString(&cmd, quote_literal_cstr(pubname));
  }
  appendStringInfoChar(&cmd, ')');

  res = walrcv_exec(wrconn, cmd.data, 2, tableRow);
  pfree(cmd.data);

  if (res->status != WALRCV_OK_TUPLES)
  {
    ereport(ERROR, (errmsg("could not receive list of replicated tables from the publisher: %s", res->err)));
  }

                       
  slot = MakeSingleTupleTableSlot(res->tupledesc, &TTSOpsMinimalTuple);
  while (tuplestore_gettupleslot(res->tuplestore, true, false, slot))
  {
    char *nspname;
    char *relname;
    bool isnull;
    RangeVar *rv;

    nspname = TextDatumGetCString(slot_getattr(slot, 1, &isnull));
    Assert(!isnull);
    relname = TextDatumGetCString(slot_getattr(slot, 2, &isnull));
    Assert(!isnull);

    rv = makeRangeVar(pstrdup(nspname), pstrdup(relname), -1);
    tablelist = lappend(tablelist, rv);

    ExecClearTuple(slot);
  }
  ExecDropSingleTupleTableSlot(slot);

  walrcv_clear_result(res);

  return tablelist;
}
