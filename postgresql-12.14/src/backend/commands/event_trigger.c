                                                                            
   
                   
                                            
   
                                                                         
                                                                        
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_event_trigger.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_type.h"
#include "commands/dbcommands.h"
#include "commands/event_trigger.h"
#include "commands/extension.h"
#include "commands/trigger.h"
#include "funcapi.h"
#include "parser/parse_func.h"
#include "pgstat.h"
#include "lib/ilist.h"
#include "miscadmin.h"
#include "tcop/deparse_utility.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/evtcache.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "tcop/utility.h"

typedef struct EventTriggerQueryState
{
                                               
  MemoryContext cxt;

                
  slist_head SQLDropList;
  bool in_sql_drop;

                     
  Oid table_rewrite_oid;                                            
                                       
  int table_rewrite_reason;                        

                                      
  bool commandCollectionInhibited;
  CollectedCommand *currentCommand;
  List *commandList;                                  
                                            
  struct EventTriggerQueryState *previous;
} EventTriggerQueryState;

static EventTriggerQueryState *currentEventTriggerState = NULL;

typedef struct
{
  const char *obtypename;
  bool supported;
} event_trigger_support_data;

typedef enum
{
  EVENT_TRIGGER_COMMAND_TAG_OK,
  EVENT_TRIGGER_COMMAND_TAG_NOT_SUPPORTED,
  EVENT_TRIGGER_COMMAND_TAG_NOT_RECOGNIZED
} event_trigger_command_tag_check_result;

                                        
static const event_trigger_support_data event_trigger_support[] = {{"ACCESS METHOD", true}, {"AGGREGATE", true}, {"CAST", true}, {"CONSTRAINT", true}, {"COLLATION", true}, {"CONVERSION", true}, {"DATABASE", false}, {"DOMAIN", true}, {"EXTENSION", true}, {"EVENT TRIGGER", false}, {"FOREIGN DATA WRAPPER", true}, {"FOREIGN TABLE", true}, {"FUNCTION", true}, {"INDEX", true}, {"LANGUAGE", true}, {"MATERIALIZED VIEW", true}, {"OPERATOR", true}, {"OPERATOR CLASS", true}, {"OPERATOR FAMILY", true}, {"POLICY", true}, {"PROCEDURE", true}, {"PUBLICATION", true}, {"ROLE", false}, {"ROUTINE", true}, {"RULE", true}, {"SCHEMA", true}, {"SEQUENCE", true}, {"SERVER", true}, {"STATISTICS", true}, {"SUBSCRIPTION", true}, {"TABLE", true}, {"TABLESPACE", false}, {"TRANSFORM", true}, {"TRIGGER", true}, {"TEXT SEARCH CONFIGURATION", true}, {"TEXT SEARCH DICTIONARY", true}, {"TEXT SEARCH PARSER", true}, {"TEXT SEARCH TEMPLATE", true}, {"TYPE", true}, {"USER MAPPING", true}, {"VIEW", true}, {NULL, false}};

                                 
typedef struct SQLDropObject
{
  ObjectAddress address;
  const char *schemaname;
  const char *objname;
  const char *objidentity;
  const char *objecttype;
  List *addrnames;
  List *addrargs;
  bool original;
  bool normal;
  bool istemp;
  slist_node next;
} SQLDropObject;

static void
AlterEventTriggerOwner_internal(Relation rel, HeapTuple tup, Oid newOwnerId);
static event_trigger_command_tag_check_result
check_ddl_tag(const char *tag);
static event_trigger_command_tag_check_result
check_table_rewrite_ddl_tag(const char *tag);
static void
error_duplicate_filter_variable(const char *defname);
static Datum
filter_list_to_array(List *filterlist);
static Oid
insert_event_trigger_tuple(const char *trigname, const char *eventname, Oid evtOwner, Oid funcoid, List *tags);
static void
validate_ddl_tags(const char *filtervar, List *taglist);
static void
validate_table_rewrite_tags(const char *filtervar, List *taglist);
static void
EventTriggerInvoke(List *fn_oid_list, EventTriggerData *trigdata);
static const char *
stringify_grant_objtype(ObjectType objtype);
static const char *
stringify_adefprivs_objtype(ObjectType objtype);

   
                            
   
Oid
CreateEventTrigger(CreateEventTrigStmt *stmt)
{
  HeapTuple tuple;
  Oid funcoid;
  Oid funcrettype;
  Oid fargtypes[1];            
  Oid evtowner = GetUserId();
  ListCell *lc;
  List *tags = NULL;

     
                                                                           
                                                                             
                                  
     
  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to create event trigger \"%s\"", stmt->trigname), errhint("Must be superuser to create an event trigger.")));
  }

                            
  if (strcmp(stmt->eventname, "ddl_command_start") != 0 && strcmp(stmt->eventname, "ddl_command_end") != 0 && strcmp(stmt->eventname, "sql_drop") != 0 && strcmp(stmt->eventname, "table_rewrite") != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("unrecognized event name \"%s\"", stmt->eventname)));
  }

                                   
  foreach (lc, stmt->whenclause)
  {
    DefElem *def = (DefElem *)lfirst(lc);

    if (strcmp(def->defname, "tag") == 0)
    {
      if (tags != NULL)
      {
        error_duplicate_filter_variable(def->defname);
      }
      tags = (List *)def->arg;
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("unrecognized filter variable \"%s\"", def->defname)));
    }
  }

                                  
  if ((strcmp(stmt->eventname, "ddl_command_start") == 0 || strcmp(stmt->eventname, "ddl_command_end") == 0 || strcmp(stmt->eventname, "sql_drop") == 0) && tags != NULL)
  {
    validate_ddl_tags("tag", tags);
  }
  else if (strcmp(stmt->eventname, "table_rewrite") == 0 && tags != NULL)
  {
    validate_table_rewrite_tags("tag", tags);
  }

     
                                                                         
                     
     
  tuple = SearchSysCache1(EVENTTRIGGERNAME, CStringGetDatum(stmt->trigname));
  if (HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("event trigger \"%s\" already exists", stmt->trigname)));
  }

                                               
  funcoid = LookupFuncName(stmt->funcname, 0, fargtypes, false);
  funcrettype = get_func_rettype(funcoid);
  if (funcrettype != EVTTRIGGEROID)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("function %s must return type %s", NameListToString(stmt->funcname), "event_trigger")));
  }

                               
  return insert_event_trigger_tuple(stmt->trigname, stmt->eventname, evtowner, funcoid, tags);
}

   
                              
   
static void
validate_ddl_tags(const char *filtervar, List *taglist)
{
  ListCell *lc;

  foreach (lc, taglist)
  {
    const char *tag = strVal(lfirst(lc));
    event_trigger_command_tag_check_result result;

    result = check_ddl_tag(tag);
    if (result == EVENT_TRIGGER_COMMAND_TAG_NOT_RECOGNIZED)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("filter value \"%s\" not recognized for filter variable \"%s\"", tag, filtervar)));
    }
    if (result == EVENT_TRIGGER_COMMAND_TAG_NOT_SUPPORTED)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                                                              
                         errmsg("event triggers are not supported for %s", tag)));
    }
  }
}

static event_trigger_command_tag_check_result
check_ddl_tag(const char *tag)
{
  const char *obtypename;
  const event_trigger_support_data *etsd;

     
                                              
     
  if (pg_strcasecmp(tag, "CREATE TABLE AS") == 0 || pg_strcasecmp(tag, "SELECT INTO") == 0 || pg_strcasecmp(tag, "REFRESH MATERIALIZED VIEW") == 0 || pg_strcasecmp(tag, "ALTER DEFAULT PRIVILEGES") == 0 || pg_strcasecmp(tag, "ALTER LARGE OBJECT") == 0 || pg_strcasecmp(tag, "COMMENT") == 0 || pg_strcasecmp(tag, "GRANT") == 0 || pg_strcasecmp(tag, "REVOKE") == 0 || pg_strcasecmp(tag, "DROP OWNED") == 0 || pg_strcasecmp(tag, "IMPORT FOREIGN SCHEMA") == 0 || pg_strcasecmp(tag, "SECURITY LABEL") == 0)
  {
    return EVENT_TRIGGER_COMMAND_TAG_OK;
  }

     
                                                          
     
  if (pg_strncasecmp(tag, "CREATE ", 7) == 0)
  {
    obtypename = tag + 7;
  }
  else if (pg_strncasecmp(tag, "ALTER ", 6) == 0)
  {
    obtypename = tag + 6;
  }
  else if (pg_strncasecmp(tag, "DROP ", 5) == 0)
  {
    obtypename = tag + 5;
  }
  else
  {
    return EVENT_TRIGGER_COMMAND_TAG_NOT_RECOGNIZED;
  }

     
                                                              
     
  for (etsd = event_trigger_support; etsd->obtypename != NULL; etsd++)
  {
    if (pg_strcasecmp(etsd->obtypename, obtypename) == 0)
    {
      break;
    }
  }
  if (etsd->obtypename == NULL)
  {
    return EVENT_TRIGGER_COMMAND_TAG_NOT_RECOGNIZED;
  }
  if (!etsd->supported)
  {
    return EVENT_TRIGGER_COMMAND_TAG_NOT_SUPPORTED;
  }
  return EVENT_TRIGGER_COMMAND_TAG_OK;
}

   
                                                      
   
static void
validate_table_rewrite_tags(const char *filtervar, List *taglist)
{
  ListCell *lc;

  foreach (lc, taglist)
  {
    const char *tag = strVal(lfirst(lc));
    event_trigger_command_tag_check_result result;

    result = check_table_rewrite_ddl_tag(tag);
    if (result == EVENT_TRIGGER_COMMAND_TAG_NOT_SUPPORTED)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
                                                                              
                         errmsg("event triggers are not supported for %s", tag)));
    }
  }
}

static event_trigger_command_tag_check_result
check_table_rewrite_ddl_tag(const char *tag)
{
  if (pg_strcasecmp(tag, "ALTER TABLE") == 0 || pg_strcasecmp(tag, "ALTER TYPE") == 0)
  {
    return EVENT_TRIGGER_COMMAND_TAG_OK;
  }

  return EVENT_TRIGGER_COMMAND_TAG_NOT_SUPPORTED;
}

   
                                               
   
static void
error_duplicate_filter_variable(const char *defname)
{
  ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("filter variable \"%s\" specified more than once", defname)));
}

   
                                                                
   
static Oid
insert_event_trigger_tuple(const char *trigname, const char *eventname, Oid evtOwner, Oid funcoid, List *taglist)
{
  Relation tgrel;
  Oid trigoid;
  HeapTuple tuple;
  Datum values[Natts_pg_trigger];
  bool nulls[Natts_pg_trigger];
  NameData evtnamedata, evteventdata;
  ObjectAddress myself, referenced;

                              
  tgrel = table_open(EventTriggerRelationId, RowExclusiveLock);

                                       
  trigoid = GetNewOidWithIndex(tgrel, EventTriggerOidIndexId, Anum_pg_event_trigger_oid);
  values[Anum_pg_event_trigger_oid - 1] = ObjectIdGetDatum(trigoid);
  memset(nulls, false, sizeof(nulls));
  namestrcpy(&evtnamedata, trigname);
  values[Anum_pg_event_trigger_evtname - 1] = NameGetDatum(&evtnamedata);
  namestrcpy(&evteventdata, eventname);
  values[Anum_pg_event_trigger_evtevent - 1] = NameGetDatum(&evteventdata);
  values[Anum_pg_event_trigger_evtowner - 1] = ObjectIdGetDatum(evtOwner);
  values[Anum_pg_event_trigger_evtfoid - 1] = ObjectIdGetDatum(funcoid);
  values[Anum_pg_event_trigger_evtenabled - 1] = CharGetDatum(TRIGGER_FIRES_ON_ORIGIN);
  if (taglist == NIL)
  {
    nulls[Anum_pg_event_trigger_evttags - 1] = true;
  }
  else
  {
    values[Anum_pg_event_trigger_evttags - 1] = filter_list_to_array(taglist);
  }

                          
  tuple = heap_form_tuple(tgrel->rd_att, values, nulls);
  CatalogTupleInsert(tgrel, tuple);
  heap_freetuple(tuple);

                        
  recordDependencyOnOwner(EventTriggerRelationId, trigoid, evtOwner);

                                         
  myself.classId = EventTriggerRelationId;
  myself.objectId = trigoid;
  myself.objectSubId = 0;
  referenced.classId = ProcedureRelationId;
  referenced.objectId = funcoid;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                    
  recordDependencyOnCurrentExtension(&myself, false);

                                                
  InvokeObjectPostCreateHook(EventTriggerRelationId, trigoid, 0);

                               
  table_close(tgrel, RowExclusiveLock);

  return trigoid;
}

   
                                                                            
                                                                          
                                                                            
                                              
   
                                                                        
                                                                    
                                                                            
                                 
   
static Datum
filter_list_to_array(List *filterlist)
{
  ListCell *lc;
  Datum *data;
  int i = 0, l = list_length(filterlist);

  data = (Datum *)palloc(l * sizeof(Datum));

  foreach (lc, filterlist)
  {
    const char *value = strVal(lfirst(lc));
    char *result, *p;

    result = pstrdup(value);
    for (p = result; *p; p++)
    {
      *p = pg_ascii_toupper((unsigned char)*p);
    }
    data[i++] = PointerGetDatum(cstring_to_text(result));
    pfree(result);
  }

  return PointerGetDatum(construct_array(data, l, TEXTOID, -1, false, 'i'));
}

   
                                   
   
void
RemoveEventTriggerById(Oid trigOid)
{
  Relation tgrel;
  HeapTuple tup;

  tgrel = table_open(EventTriggerRelationId, RowExclusiveLock);

  tup = SearchSysCache1(EVENTTRIGGEROID, ObjectIdGetDatum(trigOid));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for event trigger %u", trigOid);
  }

  CatalogTupleDelete(tgrel, &tup->t_self);

  ReleaseSysCache(tup);

  table_close(tgrel, RowExclusiveLock);
}

   
                                                                
   
Oid
AlterEventTrigger(AlterEventTrigStmt *stmt)
{
  Relation tgrel;
  HeapTuple tup;
  Oid trigoid;
  Form_pg_event_trigger evtForm;
  char tgenabled = stmt->tgenabled;

  tgrel = table_open(EventTriggerRelationId, RowExclusiveLock);

  tup = SearchSysCacheCopy1(EVENTTRIGGERNAME, CStringGetDatum(stmt->trigname));
  if (!HeapTupleIsValid(tup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("event trigger \"%s\" does not exist", stmt->trigname)));
  }

  evtForm = (Form_pg_event_trigger)GETSTRUCT(tup);
  trigoid = evtForm->oid;

  if (!pg_event_trigger_ownercheck(trigoid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_EVENT_TRIGGER, stmt->trigname);
  }

                                                  
  evtForm->evtenabled = tgenabled;

  CatalogTupleUpdate(tgrel, &tup->t_self, tup);

  InvokeObjectPostAlterHook(EventTriggerRelationId, trigoid, 0);

                
  heap_freetuple(tup);
  table_close(tgrel, RowExclusiveLock);

  return trigoid;
}

   
                                           
   
ObjectAddress
AlterEventTriggerOwner(const char *name, Oid newOwnerId)
{
  Oid evtOid;
  HeapTuple tup;
  Form_pg_event_trigger evtForm;
  Relation rel;
  ObjectAddress address;

  rel = table_open(EventTriggerRelationId, RowExclusiveLock);

  tup = SearchSysCacheCopy1(EVENTTRIGGERNAME, CStringGetDatum(name));

  if (!HeapTupleIsValid(tup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("event trigger \"%s\" does not exist", name)));
  }

  evtForm = (Form_pg_event_trigger)GETSTRUCT(tup);
  evtOid = evtForm->oid;

  AlterEventTriggerOwner_internal(rel, tup, newOwnerId);

  ObjectAddressSet(address, EventTriggerRelationId, evtOid);

  heap_freetuple(tup);

  table_close(rel, RowExclusiveLock);

  return address;
}

   
                                      
   
void
AlterEventTriggerOwner_oid(Oid trigOid, Oid newOwnerId)
{
  HeapTuple tup;
  Relation rel;

  rel = table_open(EventTriggerRelationId, RowExclusiveLock);

  tup = SearchSysCacheCopy1(EVENTTRIGGEROID, ObjectIdGetDatum(trigOid));

  if (!HeapTupleIsValid(tup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("event trigger with OID %u does not exist", trigOid)));
  }

  AlterEventTriggerOwner_internal(rel, tup, newOwnerId);

  heap_freetuple(tup);

  table_close(rel, RowExclusiveLock);
}

   
                                                            
   
static void
AlterEventTriggerOwner_internal(Relation rel, HeapTuple tup, Oid newOwnerId)
{
  Form_pg_event_trigger form;

  form = (Form_pg_event_trigger)GETSTRUCT(tup);

  if (form->evtowner == newOwnerId)
  {
    return;
  }

  if (!pg_event_trigger_ownercheck(form->oid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_EVENT_TRIGGER, NameStr(form->evtname));
  }

                                     
  if (!superuser_arg(newOwnerId))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to change owner of event trigger \"%s\"", NameStr(form->evtname)), errhint("The owner of an event trigger must be a superuser.")));
  }

  form->evtowner = newOwnerId;
  CatalogTupleUpdate(rel, &tup->t_self, tup);

                                         
  changeDependencyOnOwner(EventTriggerRelationId, form->oid, newOwnerId);

  InvokeObjectPostAlterHook(EventTriggerRelationId, form->oid, 0);
}

   
                                                                             
   
                                                                    
                                 
   
Oid
get_event_trigger_oid(const char *trigname, bool missing_ok)
{
  Oid oid;

  oid = GetSysCacheOid1(EVENTTRIGGERNAME, Anum_pg_event_trigger_oid, CStringGetDatum(trigname));
  if (!OidIsValid(oid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("event trigger \"%s\" does not exist", trigname)));
  }
  return oid;
}

   
                                                                             
                                                                              
                  
   
static bool
filter_event_trigger(const char **tag, EventTriggerCacheItem *item)
{
     
                                                                            
                      
     
  if (SessionReplicationRole == SESSION_REPLICATION_ROLE_REPLICA)
  {
    if (item->enabled == TRIGGER_FIRES_ON_ORIGIN)
    {
      return false;
    }
  }
  else
  {
    if (item->enabled == TRIGGER_FIRES_ON_REPLICA)
    {
      return false;
    }
  }

                                              
  if (item->ntags != 0 && bsearch(tag, item->tag, item->ntags, sizeof(char *), pg_qsort_strcmp) == NULL)
  {
    return false;
  }

                                                                 
  return true;
}

   
                                                                                
                                                                     
                                                     
   
static List *
EventTriggerCommonSetup(Node *parsetree, EventTriggerEvent event, const char *eventstr, EventTriggerData *trigdata)
{
  const char *tag;
  List *cachelist;
  ListCell *lc;
  List *runlist = NIL;

     
                                                                           
                                                                         
                                                                      
                                                                           
                                                                             
                                                                             
                                        
     
                                                                           
                                                                            
                                                                         
                           
     
#ifdef USE_ASSERT_CHECKING
  {
    const char *dbgtag;

    dbgtag = CreateCommandTag(parsetree);
    if (event == EVT_DDLCommandStart || event == EVT_DDLCommandEnd || event == EVT_SQLDrop)
    {
      if (check_ddl_tag(dbgtag) != EVENT_TRIGGER_COMMAND_TAG_OK)
      {
        elog(ERROR, "unexpected command tag \"%s\"", dbgtag);
      }
    }
    else if (event == EVT_TableRewrite)
    {
      if (check_table_rewrite_ddl_tag(dbgtag) != EVENT_TRIGGER_COMMAND_TAG_OK)
      {
        elog(ERROR, "unexpected command tag \"%s\"", dbgtag);
      }
    }
  }
#endif

                                                                     
  cachelist = EventCacheLookup(event);
  if (cachelist == NIL)
  {
    return NIL;
  }

                            
  tag = CreateCommandTag(parsetree);

     
                                                                          
                                                                            
                                                                           
                                                                             
                       
     
  foreach (lc, cachelist)
  {
    EventTriggerCacheItem *item = lfirst(lc);

    if (filter_event_trigger(&tag, item))
    {
                                              
      runlist = lappend_oid(runlist, item->fnoid);
    }
  }

                                                                
  if (runlist == NIL)
  {
    return NIL;
  }

  trigdata->type = T_EventTriggerData;
  trigdata->event = eventstr;
  trigdata->parsetree = parsetree;
  trigdata->tag = tag;

  return runlist;
}

   
                                    
   
void
EventTriggerDDLCommandStart(Node *parsetree)
{
  List *runlist;
  EventTriggerData trigdata;

     
                                                                           
                                      
     
                                                                      
                                                                           
                                                                        
            
     
                                                                         
                                                                           
                                                                         
                                                                  
                                                                         
                                                                           
     
  if (!IsUnderPostmaster)
  {
    return;
  }

  runlist = EventTriggerCommonSetup(parsetree, EVT_DDLCommandStart, "ddl_command_start", &trigdata);
  if (runlist == NIL)
  {
    return;
  }

                         
  EventTriggerInvoke(runlist, &trigdata);

                
  list_free(runlist);

     
                                                                           
              
     
  CommandCounterIncrement();
}

   
                                  
   
void
EventTriggerDDLCommandEnd(Node *parsetree)
{
  List *runlist;
  EventTriggerData trigdata;

     
                                                                      
                                                
     
  if (!IsUnderPostmaster)
  {
    return;
  }

     
                                                                           
                                                                         
                                                                           
                                                                           
                                                                     
                                                                       
                                                                           
                             
     
  if (!currentEventTriggerState)
  {
    return;
  }

  runlist = EventTriggerCommonSetup(parsetree, EVT_DDLCommandEnd, "ddl_command_end", &trigdata);
  if (runlist == NIL)
  {
    return;
  }

     
                                                                          
               
     
  CommandCounterIncrement();

                         
  EventTriggerInvoke(runlist, &trigdata);

                
  list_free(runlist);
}

   
                           
   
void
EventTriggerSQLDrop(Node *parsetree)
{
  List *runlist;
  EventTriggerData trigdata;

     
                                                                      
                                                
     
  if (!IsUnderPostmaster)
  {
    return;
  }

     
                                                                         
                                                                      
                                                                           
                                                                           
               
     
  if (!currentEventTriggerState || slist_is_empty(&currentEventTriggerState->SQLDropList))
  {
    return;
  }

  runlist = EventTriggerCommonSetup(parsetree, EVT_SQLDrop, "sql_drop", &trigdata);

     
                                                                            
                                                                            
                                                                          
                                                                        
     
  if (runlist == NIL)
  {
    return;
  }

     
                                                                          
               
     
  CommandCounterIncrement();

     
                                                                        
                                                                          
                                                                             
                                                                            
                 
     
  currentEventTriggerState->in_sql_drop = true;

                         
  PG_TRY();
  {
    EventTriggerInvoke(runlist, &trigdata);
  }
  PG_CATCH();
  {
    currentEventTriggerState->in_sql_drop = false;
    PG_RE_THROW();
  }
  PG_END_TRY();
  currentEventTriggerState->in_sql_drop = false;

                
  list_free(runlist);
}

   
                                
   
void
EventTriggerTableRewrite(Node *parsetree, Oid tableOid, int reason)
{
  List *runlist;
  EventTriggerData trigdata;

     
                                                                           
                                      
     
                                                                      
                                                                           
                                                                        
            
     
                                                                         
                                                                           
                                                                         
                                                                  
                                                                         
                                                                           
     
  if (!IsUnderPostmaster)
  {
    return;
  }

     
                                                                           
                                                                         
                                                                 
                                                                           
                                                   
     
  if (!currentEventTriggerState)
  {
    return;
  }

  runlist = EventTriggerCommonSetup(parsetree, EVT_TableRewrite, "table_rewrite", &trigdata);
  if (runlist == NIL)
  {
    return;
  }

     
                                                                          
                                                                          
                                                                    
                                                                         
                                 
     
  currentEventTriggerState->table_rewrite_oid = tableOid;
  currentEventTriggerState->table_rewrite_reason = reason;

                         
  PG_TRY();
  {
    EventTriggerInvoke(runlist, &trigdata);
  }
  PG_CATCH();
  {
    currentEventTriggerState->table_rewrite_oid = InvalidOid;
    currentEventTriggerState->table_rewrite_reason = 0;
    PG_RE_THROW();
  }
  PG_END_TRY();

  currentEventTriggerState->table_rewrite_oid = InvalidOid;
  currentEventTriggerState->table_rewrite_reason = 0;

                
  list_free(runlist);

     
                                                                           
              
     
  CommandCounterIncrement();
}

   
                                                          
   
static void
EventTriggerInvoke(List *fn_oid_list, EventTriggerData *trigdata)
{
  MemoryContext context;
  MemoryContext oldcontext;
  ListCell *lc;
  bool first = true;

                                                                   
  check_stack_depth();

     
                                                                            
                                    
     
  context = AllocSetContextCreate(CurrentMemoryContext, "event trigger context", ALLOCSET_DEFAULT_SIZES);
  oldcontext = MemoryContextSwitchTo(context);

                                
  foreach (lc, fn_oid_list)
  {
    LOCAL_FCINFO(fcinfo, 0);
    Oid fnoid = lfirst_oid(lc);
    FmgrInfo flinfo;
    PgStat_FunctionCallUsage fcusage;

    elog(DEBUG1, "EventTriggerInvoke %u", fnoid);

       
                                                                       
                                                                       
                                                                          
                                             
       
    if (first)
    {
      first = false;
    }
    else
    {
      CommandCounterIncrement();
    }

                              
    fmgr_info(fnoid, &flinfo);

                                                                        
    InitFunctionCallInfoData(*fcinfo, &flinfo, 0, InvalidOid, (Node *)trigdata, NULL);
    pgstat_init_function_usage(fcinfo, &fcusage);
    FunctionCallInvoke(fcinfo);
    pgstat_end_function_usage(&fcusage, true);

                         
    MemoryContextReset(context);
  }

                                                                
  MemoryContextSwitchTo(oldcontext);
  MemoryContextDelete(context);
}

   
                                               
   
bool
EventTriggerSupportsObjectType(ObjectType obtype)
{
  switch (obtype)
  {
  case OBJECT_DATABASE:
  case OBJECT_TABLESPACE:
  case OBJECT_ROLE:
                                       
    return false;
  case OBJECT_EVENT_TRIGGER:
                                                         
    return false;
  case OBJECT_ACCESS_METHOD:
  case OBJECT_AGGREGATE:
  case OBJECT_AMOP:
  case OBJECT_AMPROC:
  case OBJECT_ATTRIBUTE:
  case OBJECT_CAST:
  case OBJECT_COLUMN:
  case OBJECT_COLLATION:
  case OBJECT_CONVERSION:
  case OBJECT_DEFACL:
  case OBJECT_DEFAULT:
  case OBJECT_DOMAIN:
  case OBJECT_DOMCONSTRAINT:
  case OBJECT_EXTENSION:
  case OBJECT_FDW:
  case OBJECT_FOREIGN_SERVER:
  case OBJECT_FOREIGN_TABLE:
  case OBJECT_FUNCTION:
  case OBJECT_INDEX:
  case OBJECT_LANGUAGE:
  case OBJECT_LARGEOBJECT:
  case OBJECT_MATVIEW:
  case OBJECT_OPCLASS:
  case OBJECT_OPERATOR:
  case OBJECT_OPFAMILY:
  case OBJECT_POLICY:
  case OBJECT_PROCEDURE:
  case OBJECT_PUBLICATION:
  case OBJECT_PUBLICATION_REL:
  case OBJECT_ROUTINE:
  case OBJECT_RULE:
  case OBJECT_SCHEMA:
  case OBJECT_SEQUENCE:
  case OBJECT_SUBSCRIPTION:
  case OBJECT_STATISTIC_EXT:
  case OBJECT_TABCONSTRAINT:
  case OBJECT_TABLE:
  case OBJECT_TRANSFORM:
  case OBJECT_TRIGGER:
  case OBJECT_TSCONFIGURATION:
  case OBJECT_TSDICTIONARY:
  case OBJECT_TSPARSER:
  case OBJECT_TSTEMPLATE:
  case OBJECT_TYPE:
  case OBJECT_USER_MAPPING:
  case OBJECT_VIEW:
    return true;

       
                                                                
                                                                       
       
  }

                                                          
  return false;
}

   
                                                
   
bool
EventTriggerSupportsObjectClass(ObjectClass objclass)
{
  switch (objclass)
  {
  case OCLASS_DATABASE:
  case OCLASS_TBLSPACE:
  case OCLASS_ROLE:
                                       
    return false;
  case OCLASS_EVENT_TRIGGER:
                                                         
    return false;
  case OCLASS_CLASS:
  case OCLASS_PROC:
  case OCLASS_TYPE:
  case OCLASS_CAST:
  case OCLASS_COLLATION:
  case OCLASS_CONSTRAINT:
  case OCLASS_CONVERSION:
  case OCLASS_DEFAULT:
  case OCLASS_LANGUAGE:
  case OCLASS_LARGEOBJECT:
  case OCLASS_OPERATOR:
  case OCLASS_OPCLASS:
  case OCLASS_OPFAMILY:
  case OCLASS_AM:
  case OCLASS_AMOP:
  case OCLASS_AMPROC:
  case OCLASS_REWRITE:
  case OCLASS_TRIGGER:
  case OCLASS_SCHEMA:
  case OCLASS_STATISTIC_EXT:
  case OCLASS_TSPARSER:
  case OCLASS_TSDICT:
  case OCLASS_TSTEMPLATE:
  case OCLASS_TSCONFIG:
  case OCLASS_FDW:
  case OCLASS_FOREIGN_SERVER:
  case OCLASS_USER_MAPPING:
  case OCLASS_DEFACL:
  case OCLASS_EXTENSION:
  case OCLASS_POLICY:
  case OCLASS_PUBLICATION:
  case OCLASS_PUBLICATION_REL:
  case OCLASS_SUBSCRIPTION:
  case OCLASS_TRANSFORM:
    return true;

       
                                                                
                                                                   
       
  }

                                                          
  return false;
}

   
                                                                              
                                                                                
                                                                                
                                             
   
bool
EventTriggerBeginCompleteQuery(void)
{
  EventTriggerQueryState *state;
  MemoryContext cxt;

     
                                                                             
                                                                            
                  
     
  if (!trackDroppedObjectsNeeded())
  {
    return false;
  }

  cxt = AllocSetContextCreate(TopMemoryContext, "event trigger state", ALLOCSET_DEFAULT_SIZES);
  state = MemoryContextAlloc(cxt, sizeof(EventTriggerQueryState));
  state->cxt = cxt;
  slist_init(&(state->SQLDropList));
  state->in_sql_drop = false;
  state->table_rewrite_oid = InvalidOid;

  state->commandCollectionInhibited = currentEventTriggerState ? currentEventTriggerState->commandCollectionInhibited : false;
  state->currentCommand = NULL;
  state->commandList = NIL;
  state->previous = currentEventTriggerState;
  currentEventTriggerState = state;

  return true;
}

   
                                                                                
        
   
                                                                              
                              
   
                                                                              
                                                                              
                                      
   
void
EventTriggerEndCompleteQuery(void)
{
  EventTriggerQueryState *prevstate;

  prevstate = currentEventTriggerState->previous;

                                                                   
  MemoryContextDelete(currentEventTriggerState->cxt);

  currentEventTriggerState = prevstate;
}

   
                                                            
   
                                                                        
   
bool
trackDroppedObjectsNeeded(void)
{
     
                                                                        
            
     
  return list_length(EventCacheLookup(EVT_SQLDrop)) > 0 || list_length(EventCacheLookup(EVT_TableRewrite)) > 0 || list_length(EventCacheLookup(EVT_DDLCommandEnd)) > 0;
}

   
                                                                       
   
                                                                         
                                                                          
                                                                            
                                                                      
                                                                          
                                                                             
                                                                         
                                                                        
                                          
   
                                                                          
                          
   

   
                                                                
   
void
EventTriggerSQLDropAddObject(const ObjectAddress *object, bool original, bool normal)
{
  SQLDropObject *obj;
  MemoryContext oldcxt;

  if (!currentEventTriggerState)
  {
    return;
  }

  Assert(EventTriggerSupportsObjectClass(getObjectClass(object)));

                                               
  if (object->classId == NamespaceRelationId && (isAnyTempNamespace(object->objectId) && !isTempNamespace(object->objectId)))
  {
    return;
  }

  oldcxt = MemoryContextSwitchTo(currentEventTriggerState->cxt);

  obj = palloc0(sizeof(SQLDropObject));
  obj->address = *object;
  obj->original = original;
  obj->normal = normal;

     
                                                                         
                                                               
                                                            
                       
     
  if (is_objectclass_supported(object->classId))
  {
    Relation catalog;
    HeapTuple tuple;

    catalog = table_open(obj->address.classId, AccessShareLock);
    tuple = get_catalog_object_by_oid(catalog, get_object_attnum_oid(object->classId), obj->address.objectId);

    if (tuple)
    {
      AttrNumber attnum;
      Datum datum;
      bool isnull;

      attnum = get_object_attnum_namespace(obj->address.classId);
      if (attnum != InvalidAttrNumber)
      {
        datum = heap_getattr(tuple, attnum, RelationGetDescr(catalog), &isnull);
        if (!isnull)
        {
          Oid namespaceId;

          namespaceId = DatumGetObjectId(datum);
                                                                 
          if (isTempNamespace(namespaceId))
          {
            obj->schemaname = "pg_temp";
            obj->istemp = true;
          }
          else if (isAnyTempNamespace(namespaceId))
          {
            pfree(obj);
            table_close(catalog, AccessShareLock);
            MemoryContextSwitchTo(oldcxt);
            return;
          }
          else
          {
            obj->schemaname = get_namespace_name(namespaceId);
            obj->istemp = false;
          }
        }
      }

      if (get_object_namensp_unique(obj->address.classId) && obj->address.objectSubId == 0)
      {
        attnum = get_object_attnum_name(obj->address.classId);
        if (attnum != InvalidAttrNumber)
        {
          datum = heap_getattr(tuple, attnum, RelationGetDescr(catalog), &isnull);
          if (!isnull)
          {
            obj->objname = pstrdup(NameStr(*DatumGetName(datum)));
          }
        }
      }
    }

    table_close(catalog, AccessShareLock);
  }
  else
  {
    if (object->classId == NamespaceRelationId && isTempNamespace(object->objectId))
    {
      obj->istemp = true;
    }
  }

                                            
  obj->objidentity = getObjectIdentityParts(&obj->address, &obj->addrnames, &obj->addrargs);

                   
  obj->objecttype = getObjectTypeDescription(&obj->address);

  slist_push_head(&(currentEventTriggerState->SQLDropList), &obj->next);

  MemoryContextSwitchTo(oldcxt);
}

   
                                    
   
                                                                              
                  
   
Datum
pg_event_trigger_dropped_objects(PG_FUNCTION_ARGS)
{
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;
  slist_iter iter;

     
                                                            
     
  if (!currentEventTriggerState || !currentEventTriggerState->in_sql_drop)
  {
    ereport(ERROR, (errcode(ERRCODE_E_R_I_E_EVENT_TRIGGER_PROTOCOL_VIOLATED), errmsg("%s can only be called in a sql_drop event trigger function", "pg_event_trigger_dropped_objects()")));
  }

                                                                 
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required, but it is not allowed in this context")));
  }

                                                    
  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    elog(ERROR, "return type must be a row type");
  }

                                                
  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  oldcontext = MemoryContextSwitchTo(per_query_ctx);

  tupstore = tuplestore_begin_heap(true, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  MemoryContextSwitchTo(oldcontext);

  slist_foreach(iter, &(currentEventTriggerState->SQLDropList))
  {
    SQLDropObject *obj;
    int i = 0;
    Datum values[12];
    bool nulls[12];

    obj = slist_container(SQLDropObject, next, iter.cur);

    MemSet(values, 0, sizeof(values));
    MemSet(nulls, 0, sizeof(nulls));

                 
    values[i++] = ObjectIdGetDatum(obj->address.classId);

               
    values[i++] = ObjectIdGetDatum(obj->address.objectId);

                  
    values[i++] = Int32GetDatum(obj->address.objectSubId);

                  
    values[i++] = BoolGetDatum(obj->original);

                
    values[i++] = BoolGetDatum(obj->normal);

                      
    values[i++] = BoolGetDatum(obj->istemp);

                     
    values[i++] = CStringGetTextDatum(obj->objecttype);

                     
    if (obj->schemaname)
    {
      values[i++] = CStringGetTextDatum(obj->schemaname);
    }
    else
    {
      nulls[i++] = true;
    }

                     
    if (obj->objname)
    {
      values[i++] = CStringGetTextDatum(obj->objname);
    }
    else
    {
      nulls[i++] = true;
    }

                         
    if (obj->objidentity)
    {
      values[i++] = CStringGetTextDatum(obj->objidentity);
    }
    else
    {
      nulls[i++] = true;
    }

                                        
    if (obj->addrnames)
    {
      values[i++] = PointerGetDatum(strlist_to_textarray(obj->addrnames));

      if (obj->addrargs)
      {
        values[i++] = PointerGetDatum(strlist_to_textarray(obj->addrargs));
      }
      else
      {
        values[i++] = PointerGetDatum(construct_empty_array(TEXTOID));
      }
    }
    else
    {
      nulls[i++] = true;
      nulls[i++] = true;
    }

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

                                          
  tuplestore_donestoring(tupstore);

  return (Datum)0;
}

   
                                      
   
                                                                         
                                      
   
Datum
pg_event_trigger_table_rewrite_oid(PG_FUNCTION_ARGS)
{
     
                                                            
     
  if (!currentEventTriggerState || currentEventTriggerState->table_rewrite_oid == InvalidOid)
  {
    ereport(ERROR, (errcode(ERRCODE_E_R_I_E_EVENT_TRIGGER_PROTOCOL_VIOLATED), errmsg("%s can only be called in a table_rewrite event trigger function", "pg_event_trigger_table_rewrite_oid()")));
  }

  PG_RETURN_OID(currentEventTriggerState->table_rewrite_oid);
}

   
                                         
   
                                                  
   
Datum
pg_event_trigger_table_rewrite_reason(PG_FUNCTION_ARGS)
{
     
                                                            
     
  if (!currentEventTriggerState || currentEventTriggerState->table_rewrite_reason == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_E_R_I_E_EVENT_TRIGGER_PROTOCOL_VIOLATED), errmsg("%s can only be called in a table_rewrite event trigger function", "pg_event_trigger_table_rewrite_reason()")));
  }

  PG_RETURN_INT32(currentEventTriggerState->table_rewrite_reason);
}

                                                                            
                                     
   
                                                                           
                                                                           
            
   
                                                                               
                                                                                
                             
   
                                                                               
                                                                                
                                                                                
   
                                                                            
                                                                           
                               
                                                                            
   

   
                                   
   
void
EventTriggerInhibitCommandCollection(void)
{
  if (!currentEventTriggerState)
  {
    return;
  }

  currentEventTriggerState->commandCollectionInhibited = true;
}

   
                                        
   
void
EventTriggerUndoInhibitCommandCollection(void)
{
  if (!currentEventTriggerState)
  {
    return;
  }

  currentEventTriggerState->commandCollectionInhibited = false;
}

   
                                    
                                                                
   
                                                                           
                                                                            
                                
   
                                                                             
                                                                               
                                                                              
                   
   
void
EventTriggerCollectSimpleCommand(ObjectAddress address, ObjectAddress secondaryObject, Node *parsetree)
{
  MemoryContext oldcxt;
  CollectedCommand *command;

                                                                       
  if (!currentEventTriggerState || currentEventTriggerState->commandCollectionInhibited)
  {
    return;
  }

  oldcxt = MemoryContextSwitchTo(currentEventTriggerState->cxt);

  command = palloc(sizeof(CollectedCommand));

  command->type = SCT_Simple;
  command->in_extension = creating_extension;

  command->d.simple.address = address;
  command->d.simple.secondaryObject = secondaryObject;
  command->parsetree = copyObject(parsetree);

  currentEventTriggerState->commandList = lappend(currentEventTriggerState->commandList, command);

  MemoryContextSwitchTo(oldcxt);
}

   
                               
                                                                           
   
                                                                        
                                                                               
                               
   
void
EventTriggerAlterTableStart(Node *parsetree)
{
  MemoryContext oldcxt;
  CollectedCommand *command;

                                                                       
  if (!currentEventTriggerState || currentEventTriggerState->commandCollectionInhibited)
  {
    return;
  }

  oldcxt = MemoryContextSwitchTo(currentEventTriggerState->cxt);

  command = palloc(sizeof(CollectedCommand));

  command->type = SCT_AlterTable;
  command->in_extension = creating_extension;

  command->d.alterTable.classId = RelationRelationId;
  command->d.alterTable.objectId = InvalidOid;
  command->d.alterTable.subcmds = NIL;
  command->parsetree = copyObject(parsetree);

  command->parent = currentEventTriggerState->currentCommand;
  currentEventTriggerState->currentCommand = command;

  MemoryContextSwitchTo(oldcxt);
}

   
                                                                    
   
                                                                           
   
void
EventTriggerAlterTableRelid(Oid objectId)
{
  if (!currentEventTriggerState || currentEventTriggerState->commandCollectionInhibited)
  {
    return;
  }

  currentEventTriggerState->currentCommand->d.alterTable.objectId = objectId;
}

   
                                       
                                                     
   
                                                                               
                                                                              
                                                                           
   
void
EventTriggerCollectAlterTableSubcmd(Node *subcmd, ObjectAddress address)
{
  MemoryContext oldcxt;
  CollectedATSubcmd *newsub;

                                                                       
  if (!currentEventTriggerState || currentEventTriggerState->commandCollectionInhibited)
  {
    return;
  }

  Assert(IsA(subcmd, AlterTableCmd));
  Assert(currentEventTriggerState->currentCommand != NULL);
  Assert(OidIsValid(currentEventTriggerState->currentCommand->d.alterTable.objectId));

  oldcxt = MemoryContextSwitchTo(currentEventTriggerState->cxt);

  newsub = palloc(sizeof(CollectedATSubcmd));
  newsub->address = address;
  newsub->parsetree = copyObject(subcmd);

  currentEventTriggerState->currentCommand->d.alterTable.subcmds = lappend(currentEventTriggerState->currentCommand->d.alterTable.subcmds, newsub);

  MemoryContextSwitchTo(oldcxt);
}

   
                             
                                                                         
   
                                                                            
                                                          
                                            
   
void
EventTriggerAlterTableEnd(void)
{
  CollectedCommand *parent;

                                                                       
  if (!currentEventTriggerState || currentEventTriggerState->commandCollectionInhibited)
  {
    return;
  }

  parent = currentEventTriggerState->currentCommand->parent;

                                        
  if (list_length(currentEventTriggerState->currentCommand->d.alterTable.subcmds) != 0)
  {
    MemoryContext oldcxt;

    oldcxt = MemoryContextSwitchTo(currentEventTriggerState->cxt);

    currentEventTriggerState->commandList = lappend(currentEventTriggerState->commandList, currentEventTriggerState->currentCommand);

    MemoryContextSwitchTo(oldcxt);
  }
  else
  {
    pfree(currentEventTriggerState->currentCommand);
  }

  currentEventTriggerState->currentCommand = parent;
}

   
                            
                                                          
   
                                                                            
                                
   
void
EventTriggerCollectGrant(InternalGrant *istmt)
{
  MemoryContext oldcxt;
  CollectedCommand *command;
  InternalGrant *icopy;
  ListCell *cell;

                                                                       
  if (!currentEventTriggerState || currentEventTriggerState->commandCollectionInhibited)
  {
    return;
  }

  oldcxt = MemoryContextSwitchTo(currentEventTriggerState->cxt);

     
                                     
     
  icopy = palloc(sizeof(InternalGrant));
  memcpy(icopy, istmt, sizeof(InternalGrant));
  icopy->objects = list_copy(istmt->objects);
  icopy->grantees = list_copy(istmt->grantees);
  icopy->col_privs = NIL;
  foreach (cell, istmt->col_privs)
  {
    icopy->col_privs = lappend(icopy->col_privs, copyObject(lfirst(cell)));
  }

                                                      
  command = palloc(sizeof(CollectedCommand));
  command->type = SCT_Grant;
  command->in_extension = creating_extension;
  command->d.grant.istmt = icopy;
  command->parsetree = NULL;

  currentEventTriggerState->commandList = lappend(currentEventTriggerState->commandList, command);

  MemoryContextSwitchTo(oldcxt);
}

   
                                 
                                                                    
             
   
void
EventTriggerCollectAlterOpFam(AlterOpFamilyStmt *stmt, Oid opfamoid, List *operators, List *procedures)
{
  MemoryContext oldcxt;
  CollectedCommand *command;

                                                                       
  if (!currentEventTriggerState || currentEventTriggerState->commandCollectionInhibited)
  {
    return;
  }

  oldcxt = MemoryContextSwitchTo(currentEventTriggerState->cxt);

  command = palloc(sizeof(CollectedCommand));
  command->type = SCT_AlterOpFamily;
  command->in_extension = creating_extension;
  ObjectAddressSet(command->d.opfam.address, OperatorFamilyRelationId, opfamoid);
  command->d.opfam.operators = operators;
  command->d.opfam.procedures = procedures;
  command->parsetree = (Node *)copyObject(stmt);

  currentEventTriggerState->commandList = lappend(currentEventTriggerState->commandList, command);

  MemoryContextSwitchTo(oldcxt);
}

   
                                    
                                                                   
   
void
EventTriggerCollectCreateOpClass(CreateOpClassStmt *stmt, Oid opcoid, List *operators, List *procedures)
{
  MemoryContext oldcxt;
  CollectedCommand *command;

                                                                       
  if (!currentEventTriggerState || currentEventTriggerState->commandCollectionInhibited)
  {
    return;
  }

  oldcxt = MemoryContextSwitchTo(currentEventTriggerState->cxt);

  command = palloc0(sizeof(CollectedCommand));
  command->type = SCT_CreateOpClass;
  command->in_extension = creating_extension;
  ObjectAddressSet(command->d.createopc.address, OperatorClassRelationId, opcoid);
  command->d.createopc.operators = operators;
  command->d.createopc.procedures = procedures;
  command->parsetree = (Node *)copyObject(stmt);

  currentEventTriggerState->commandList = lappend(currentEventTriggerState->commandList, command);

  MemoryContextSwitchTo(oldcxt);
}

   
                                    
                                                                     
             
   
void
EventTriggerCollectAlterTSConfig(AlterTSConfigurationStmt *stmt, Oid cfgId, Oid *dictIds, int ndicts)
{
  MemoryContext oldcxt;
  CollectedCommand *command;

                                                                       
  if (!currentEventTriggerState || currentEventTriggerState->commandCollectionInhibited)
  {
    return;
  }

  oldcxt = MemoryContextSwitchTo(currentEventTriggerState->cxt);

  command = palloc0(sizeof(CollectedCommand));
  command->type = SCT_AlterTSConfig;
  command->in_extension = creating_extension;
  ObjectAddressSet(command->d.atscfg.address, TSConfigRelationId, cfgId);
  command->d.atscfg.dictIds = palloc(sizeof(Oid) * ndicts);
  memcpy(command->d.atscfg.dictIds, dictIds, sizeof(Oid) * ndicts);
  command->d.atscfg.ndicts = ndicts;
  command->parsetree = (Node *)copyObject(stmt);

  currentEventTriggerState->commandList = lappend(currentEventTriggerState->commandList, command);

  MemoryContextSwitchTo(oldcxt);
}

   
                                    
                                                              
             
   
void
EventTriggerCollectAlterDefPrivs(AlterDefaultPrivilegesStmt *stmt)
{
  MemoryContext oldcxt;
  CollectedCommand *command;

                                                                       
  if (!currentEventTriggerState || currentEventTriggerState->commandCollectionInhibited)
  {
    return;
  }

  oldcxt = MemoryContextSwitchTo(currentEventTriggerState->cxt);

  command = palloc0(sizeof(CollectedCommand));
  command->type = SCT_AlterDefaultPrivileges;
  command->d.defprivs.objtype = stmt->action->objtype;
  command->in_extension = creating_extension;
  command->parsetree = (Node *)copyObject(stmt);

  currentEventTriggerState->commandList = lappend(currentEventTriggerState->commandList, command);
  MemoryContextSwitchTo(oldcxt);
}

   
                                                                              
              
   
Datum
pg_event_trigger_ddl_commands(PG_FUNCTION_ARGS)
{
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  TupleDesc tupdesc;
  Tuplestorestate *tupstore;
  MemoryContext per_query_ctx;
  MemoryContext oldcontext;
  ListCell *lc;

     
                                                            
     
  if (!currentEventTriggerState)
  {
    ereport(ERROR, (errcode(ERRCODE_E_R_I_E_EVENT_TRIGGER_PROTOCOL_VIOLATED), errmsg("%s can only be called in an event trigger function", "pg_event_trigger_ddl_commands()")));
  }

                                                                 
  if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("set-valued function called in context that cannot accept a set")));
  }
  if (!(rsinfo->allowedModes & SFRM_Materialize))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("materialize mode required, but it is not allowed in this context")));
  }

                                                    
  if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
  {
    elog(ERROR, "return type must be a row type");
  }

                                                
  per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
  oldcontext = MemoryContextSwitchTo(per_query_ctx);

  tupstore = tuplestore_begin_heap(true, false, work_mem);
  rsinfo->returnMode = SFRM_Materialize;
  rsinfo->setResult = tupstore;
  rsinfo->setDesc = tupdesc;

  MemoryContextSwitchTo(oldcontext);

  foreach (lc, currentEventTriggerState->commandList)
  {
    CollectedCommand *cmd = lfirst(lc);
    Datum values[9];
    bool nulls[9];
    ObjectAddress addr;
    int i = 0;

       
                                                                     
                                                                    
       
                                                                         
                                                                      
                                                                         
                                                                          
                     
       
    if (cmd->type == SCT_Simple && !OidIsValid(cmd->d.simple.address.objectId))
    {
      continue;
    }

    MemSet(nulls, 0, sizeof(nulls));

    switch (cmd->type)
    {
    case SCT_Simple:
    case SCT_AlterTable:
    case SCT_AlterOpFamily:
    case SCT_CreateOpClass:
    case SCT_AlterTSConfig:
    {
      char *identity;
      char *type;
      char *schema = NULL;

      if (cmd->type == SCT_Simple)
      {
        addr = cmd->d.simple.address;
      }
      else if (cmd->type == SCT_AlterTable)
      {
        ObjectAddressSet(addr, cmd->d.alterTable.classId, cmd->d.alterTable.objectId);
      }
      else if (cmd->type == SCT_AlterOpFamily)
      {
        addr = cmd->d.opfam.address;
      }
      else if (cmd->type == SCT_CreateOpClass)
      {
        addr = cmd->d.createopc.address;
      }
      else if (cmd->type == SCT_AlterTSConfig)
      {
        addr = cmd->d.atscfg.address;
      }

      type = getObjectTypeDescription(&addr);
      identity = getObjectIdentity(&addr);

         
                                                         
                                                              
                                                              
                                                
         
      if (is_objectclass_supported(addr.classId))
      {
        AttrNumber nspAttnum;

        nspAttnum = get_object_attnum_namespace(addr.classId);
        if (nspAttnum != InvalidAttrNumber)
        {
          Relation catalog;
          HeapTuple objtup;
          Oid schema_oid;
          bool isnull;

          catalog = table_open(addr.classId, AccessShareLock);
          objtup = get_catalog_object_by_oid(catalog, get_object_attnum_oid(addr.classId), addr.objectId);
          if (!HeapTupleIsValid(objtup))
          {
            elog(ERROR, "cache lookup failed for object %u/%u", addr.classId, addr.objectId);
          }
          schema_oid = heap_getattr(objtup, nspAttnum, RelationGetDescr(catalog), &isnull);
          if (isnull)
          {
            elog(ERROR, "invalid null namespace in object %u/%u/%d", addr.classId, addr.objectId, addr.objectSubId);
          }
                                                        
          if (isAnyTempNamespace(schema_oid))
          {
            schema = pstrdup("pg_temp");
          }
          else
          {
            schema = get_namespace_name(schema_oid);
          }

          table_close(catalog, AccessShareLock);
        }
      }

                   
      values[i++] = ObjectIdGetDatum(addr.classId);
                 
      values[i++] = ObjectIdGetDatum(addr.objectId);
                    
      values[i++] = Int32GetDatum(addr.objectSubId);
                       
      values[i++] = CStringGetTextDatum(CreateCommandTag(cmd->parsetree));
                       
      values[i++] = CStringGetTextDatum(type);
                  
      if (schema == NULL)
      {
        nulls[i++] = true;
      }
      else
      {
        values[i++] = CStringGetTextDatum(schema);
      }
                    
      values[i++] = CStringGetTextDatum(identity);
                        
      values[i++] = BoolGetDatum(cmd->in_extension);
                   
      values[i++] = PointerGetDatum(cmd);
    }
    break;

    case SCT_AlterDefaultPrivileges:
                   
      nulls[i++] = true;
                 
      nulls[i++] = true;
                    
      nulls[i++] = true;
                       
      values[i++] = CStringGetTextDatum(CreateCommandTag(cmd->parsetree));
                       
      values[i++] = CStringGetTextDatum(stringify_adefprivs_objtype(cmd->d.defprivs.objtype));
                  
      nulls[i++] = true;
                    
      nulls[i++] = true;
                        
      values[i++] = BoolGetDatum(cmd->in_extension);
                   
      values[i++] = PointerGetDatum(cmd);
      break;

    case SCT_Grant:
                   
      nulls[i++] = true;
                 
      nulls[i++] = true;
                    
      nulls[i++] = true;
                       
      values[i++] = CStringGetTextDatum(cmd->d.grant.istmt->is_grant ? "GRANT" : "REVOKE");
                       
      values[i++] = CStringGetTextDatum(stringify_grant_objtype(cmd->d.grant.istmt->objtype));
                  
      nulls[i++] = true;
                    
      nulls[i++] = true;
                        
      values[i++] = BoolGetDatum(cmd->in_extension);
                   
      values[i++] = PointerGetDatum(cmd);
      break;
    }

    tuplestore_putvalues(tupstore, tupdesc, values, nulls);
  }

                                          
  tuplestore_donestoring(tupstore);

  PG_RETURN_VOID();
}

   
                                                                      
                    
   
static const char *
stringify_grant_objtype(ObjectType objtype)
{
  switch (objtype)
  {
  case OBJECT_COLUMN:
    return "COLUMN";
  case OBJECT_TABLE:
    return "TABLE";
  case OBJECT_SEQUENCE:
    return "SEQUENCE";
  case OBJECT_DATABASE:
    return "DATABASE";
  case OBJECT_DOMAIN:
    return "DOMAIN";
  case OBJECT_FDW:
    return "FOREIGN DATA WRAPPER";
  case OBJECT_FOREIGN_SERVER:
    return "FOREIGN SERVER";
  case OBJECT_FUNCTION:
    return "FUNCTION";
  case OBJECT_LANGUAGE:
    return "LANGUAGE";
  case OBJECT_LARGEOBJECT:
    return "LARGE OBJECT";
  case OBJECT_SCHEMA:
    return "SCHEMA";
  case OBJECT_PROCEDURE:
    return "PROCEDURE";
  case OBJECT_ROUTINE:
    return "ROUTINE";
  case OBJECT_TABLESPACE:
    return "TABLESPACE";
  case OBJECT_TYPE:
    return "TYPE";
                                     
  case OBJECT_ACCESS_METHOD:
  case OBJECT_AGGREGATE:
  case OBJECT_AMOP:
  case OBJECT_AMPROC:
  case OBJECT_ATTRIBUTE:
  case OBJECT_CAST:
  case OBJECT_COLLATION:
  case OBJECT_CONVERSION:
  case OBJECT_DEFAULT:
  case OBJECT_DEFACL:
  case OBJECT_DOMCONSTRAINT:
  case OBJECT_EVENT_TRIGGER:
  case OBJECT_EXTENSION:
  case OBJECT_FOREIGN_TABLE:
  case OBJECT_INDEX:
  case OBJECT_MATVIEW:
  case OBJECT_OPCLASS:
  case OBJECT_OPERATOR:
  case OBJECT_OPFAMILY:
  case OBJECT_POLICY:
  case OBJECT_PUBLICATION:
  case OBJECT_PUBLICATION_REL:
  case OBJECT_ROLE:
  case OBJECT_RULE:
  case OBJECT_STATISTIC_EXT:
  case OBJECT_SUBSCRIPTION:
  case OBJECT_TABCONSTRAINT:
  case OBJECT_TRANSFORM:
  case OBJECT_TRIGGER:
  case OBJECT_TSCONFIGURATION:
  case OBJECT_TSDICTIONARY:
  case OBJECT_TSPARSER:
  case OBJECT_TSTEMPLATE:
  case OBJECT_USER_MAPPING:
  case OBJECT_VIEW:
    elog(ERROR, "unsupported object type: %d", (int)objtype);
  }

  return "???";                          
}

   
                                                                     
                                                                         
               
   
static const char *
stringify_adefprivs_objtype(ObjectType objtype)
{
  switch (objtype)
  {
  case OBJECT_COLUMN:
    return "COLUMNS";
  case OBJECT_TABLE:
    return "TABLES";
  case OBJECT_SEQUENCE:
    return "SEQUENCES";
  case OBJECT_DATABASE:
    return "DATABASES";
  case OBJECT_DOMAIN:
    return "DOMAINS";
  case OBJECT_FDW:
    return "FOREIGN DATA WRAPPERS";
  case OBJECT_FOREIGN_SERVER:
    return "FOREIGN SERVERS";
  case OBJECT_FUNCTION:
    return "FUNCTIONS";
  case OBJECT_LANGUAGE:
    return "LANGUAGES";
  case OBJECT_LARGEOBJECT:
    return "LARGE OBJECTS";
  case OBJECT_SCHEMA:
    return "SCHEMAS";
  case OBJECT_PROCEDURE:
    return "PROCEDURES";
  case OBJECT_ROUTINE:
    return "ROUTINES";
  case OBJECT_TABLESPACE:
    return "TABLESPACES";
  case OBJECT_TYPE:
    return "TYPES";
                                     
  case OBJECT_ACCESS_METHOD:
  case OBJECT_AGGREGATE:
  case OBJECT_AMOP:
  case OBJECT_AMPROC:
  case OBJECT_ATTRIBUTE:
  case OBJECT_CAST:
  case OBJECT_COLLATION:
  case OBJECT_CONVERSION:
  case OBJECT_DEFAULT:
  case OBJECT_DEFACL:
  case OBJECT_DOMCONSTRAINT:
  case OBJECT_EVENT_TRIGGER:
  case OBJECT_EXTENSION:
  case OBJECT_FOREIGN_TABLE:
  case OBJECT_INDEX:
  case OBJECT_MATVIEW:
  case OBJECT_OPCLASS:
  case OBJECT_OPERATOR:
  case OBJECT_OPFAMILY:
  case OBJECT_POLICY:
  case OBJECT_PUBLICATION:
  case OBJECT_PUBLICATION_REL:
  case OBJECT_ROLE:
  case OBJECT_RULE:
  case OBJECT_STATISTIC_EXT:
  case OBJECT_SUBSCRIPTION:
  case OBJECT_TABCONSTRAINT:
  case OBJECT_TRANSFORM:
  case OBJECT_TRIGGER:
  case OBJECT_TSCONFIGURATION:
  case OBJECT_TSDICTIONARY:
  case OBJECT_TSPARSER:
  case OBJECT_TSTEMPLATE:
  case OBJECT_USER_MAPPING:
  case OBJECT_VIEW:
    elog(ERROR, "unsupported object type: %d", (int)objtype);
  }

  return "???";                          
}
