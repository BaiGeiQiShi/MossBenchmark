                                                                             
   
              
                                                 
   
                                                                         
                                                                        
   
                                                                             
   
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

   
                       
   
                                                
   
                                                                            
   
ObjectAddress
ExecSecLabelStmt(SecLabelStmt *stmt)
{
  LabelProvider *provider = NULL;
  ObjectAddress address;
  Relation relation;
  ListCell *lc;

     
                                                                        
                                            
     
  if (stmt->provider == NULL)
  {
    if (label_provider_list == NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("no security label providers have been loaded")));
    }
    if (lnext(list_head(label_provider_list)) != NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("must specify provider when multiple security label providers have been loaded")));
    }
    provider = (LabelProvider *)linitial(label_provider_list);
  }
  else
  {
    foreach (lc, label_provider_list)
    {
      LabelProvider *lp = lfirst(lc);

      if (strcmp(stmt->provider, lp->provider_name) == 0)
      {
        provider = lp;
        break;
      }
    }
    if (provider == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("security label provider \"%s\" is not loaded", stmt->provider)));
    }
  }

     
                                                                           
                                                                       
                                                                          
                                             
     
  address = get_object_address(stmt->objtype, stmt->object, &relation, ShareUpdateExclusiveLock, false);

                                               
  check_object_ownership(GetUserId(), stmt->objtype, address, stmt->object, relation);

                                                 
  switch (stmt->objtype)
  {
  case OBJECT_COLUMN:

       
                                                               
                                                                      
                                                                  
       
    if (relation->rd_rel->relkind != RELKIND_RELATION && relation->rd_rel->relkind != RELKIND_VIEW && relation->rd_rel->relkind != RELKIND_MATVIEW && relation->rd_rel->relkind != RELKIND_COMPOSITE_TYPE && relation->rd_rel->relkind != RELKIND_FOREIGN_TABLE && relation->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table, view, materialized view, composite type, or foreign table", RelationGetRelationName(relation))));
    }
    break;
  default:
    break;
  }

                                                                      
  provider->hook(&address, stmt->label);

                        
  SetSecurityLabel(&address, provider->provider_name, stmt->label);

     
                                                                             
                                                                       
                                                                         
               
     
  if (relation != NULL)
  {
    relation_close(relation, NoLock);
  }

  return address;
}

   
                                                                             
                                                        
   
static char *
GetSharedSecurityLabel(const ObjectAddress *object, const char *provider)
{
  Relation pg_shseclabel;
  ScanKeyData keys[3];
  SysScanDesc scan;
  HeapTuple tuple;
  Datum datum;
  bool isnull;
  char *seclabel = NULL;

  ScanKeyInit(&keys[0], Anum_pg_shseclabel_objoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->objectId));
  ScanKeyInit(&keys[1], Anum_pg_shseclabel_classoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->classId));
  ScanKeyInit(&keys[2], Anum_pg_shseclabel_provider, BTEqualStrategyNumber, F_TEXTEQ, CStringGetTextDatum(provider));

  pg_shseclabel = table_open(SharedSecLabelRelationId, AccessShareLock);

  scan = systable_beginscan(pg_shseclabel, SharedSecLabelObjectIndexId, criticalSharedRelcachesBuilt, NULL, 3, keys);

  tuple = systable_getnext(scan);
  if (HeapTupleIsValid(tuple))
  {
    datum = heap_getattr(tuple, Anum_pg_shseclabel_label, RelationGetDescr(pg_shseclabel), &isnull);
    if (!isnull)
    {
      seclabel = TextDatumGetCString(datum);
    }
  }
  systable_endscan(scan);

  table_close(pg_shseclabel, AccessShareLock);

  return seclabel;
}

   
                                                                               
                                                            
   
char *
GetSecurityLabel(const ObjectAddress *object, const char *provider)
{
  Relation pg_seclabel;
  ScanKeyData keys[4];
  SysScanDesc scan;
  HeapTuple tuple;
  Datum datum;
  bool isnull;
  char *seclabel = NULL;

                                                             
  if (IsSharedRelation(object->classId))
  {
    return GetSharedSecurityLabel(object, provider);
  }

                                                           
  ScanKeyInit(&keys[0], Anum_pg_seclabel_objoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->objectId));
  ScanKeyInit(&keys[1], Anum_pg_seclabel_classoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->classId));
  ScanKeyInit(&keys[2], Anum_pg_seclabel_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(object->objectSubId));
  ScanKeyInit(&keys[3], Anum_pg_seclabel_provider, BTEqualStrategyNumber, F_TEXTEQ, CStringGetTextDatum(provider));

  pg_seclabel = table_open(SecLabelRelationId, AccessShareLock);

  scan = systable_beginscan(pg_seclabel, SecLabelObjectIndexId, true, NULL, 4, keys);

  tuple = systable_getnext(scan);
  if (HeapTupleIsValid(tuple))
  {
    datum = heap_getattr(tuple, Anum_pg_seclabel_label, RelationGetDescr(pg_seclabel), &isnull);
    if (!isnull)
    {
      seclabel = TextDatumGetCString(datum);
    }
  }
  systable_endscan(scan);

  table_close(pg_seclabel, AccessShareLock);

  return seclabel;
}

   
                                                                      
                                   
   
static void
SetSharedSecurityLabel(const ObjectAddress *object, const char *provider, const char *label)
{
  Relation pg_shseclabel;
  ScanKeyData keys[4];
  SysScanDesc scan;
  HeapTuple oldtup;
  HeapTuple newtup = NULL;
  Datum values[Natts_pg_shseclabel];
  bool nulls[Natts_pg_shseclabel];
  bool replaces[Natts_pg_shseclabel];

                                                        
  memset(nulls, false, sizeof(nulls));
  memset(replaces, false, sizeof(replaces));
  values[Anum_pg_shseclabel_objoid - 1] = ObjectIdGetDatum(object->objectId);
  values[Anum_pg_shseclabel_classoid - 1] = ObjectIdGetDatum(object->classId);
  values[Anum_pg_shseclabel_provider - 1] = CStringGetTextDatum(provider);
  if (label != NULL)
  {
    values[Anum_pg_shseclabel_label - 1] = CStringGetTextDatum(label);
  }

                                                        
  ScanKeyInit(&keys[0], Anum_pg_shseclabel_objoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->objectId));
  ScanKeyInit(&keys[1], Anum_pg_shseclabel_classoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->classId));
  ScanKeyInit(&keys[2], Anum_pg_shseclabel_provider, BTEqualStrategyNumber, F_TEXTEQ, CStringGetTextDatum(provider));

  pg_shseclabel = table_open(SharedSecLabelRelationId, RowExclusiveLock);

  scan = systable_beginscan(pg_shseclabel, SharedSecLabelObjectIndexId, true, NULL, 3, keys);

  oldtup = systable_getnext(scan);
  if (HeapTupleIsValid(oldtup))
  {
    if (label == NULL)
    {
      CatalogTupleDelete(pg_shseclabel, &oldtup->t_self);
    }
    else
    {
      replaces[Anum_pg_shseclabel_label - 1] = true;
      newtup = heap_modify_tuple(oldtup, RelationGetDescr(pg_shseclabel), values, nulls, replaces);
      CatalogTupleUpdate(pg_shseclabel, &oldtup->t_self, newtup);
    }
  }
  systable_endscan(scan);

                                                        
  if (newtup == NULL && label != NULL)
  {
    newtup = heap_form_tuple(RelationGetDescr(pg_shseclabel), values, nulls);
    CatalogTupleInsert(pg_shseclabel, newtup);
  }

  if (newtup != NULL)
  {
    heap_freetuple(newtup);
  }

  table_close(pg_shseclabel, RowExclusiveLock);
}

   
                                                                         
                                                                             
                                     
   
void
SetSecurityLabel(const ObjectAddress *object, const char *provider, const char *label)
{
  Relation pg_seclabel;
  ScanKeyData keys[4];
  SysScanDesc scan;
  HeapTuple oldtup;
  HeapTuple newtup = NULL;
  Datum values[Natts_pg_seclabel];
  bool nulls[Natts_pg_seclabel];
  bool replaces[Natts_pg_seclabel];

                                                             
  if (IsSharedRelation(object->classId))
  {
    SetSharedSecurityLabel(object, provider, label);
    return;
  }

                                                        
  memset(nulls, false, sizeof(nulls));
  memset(replaces, false, sizeof(replaces));
  values[Anum_pg_seclabel_objoid - 1] = ObjectIdGetDatum(object->objectId);
  values[Anum_pg_seclabel_classoid - 1] = ObjectIdGetDatum(object->classId);
  values[Anum_pg_seclabel_objsubid - 1] = Int32GetDatum(object->objectSubId);
  values[Anum_pg_seclabel_provider - 1] = CStringGetTextDatum(provider);
  if (label != NULL)
  {
    values[Anum_pg_seclabel_label - 1] = CStringGetTextDatum(label);
  }

                                                        
  ScanKeyInit(&keys[0], Anum_pg_seclabel_objoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->objectId));
  ScanKeyInit(&keys[1], Anum_pg_seclabel_classoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(object->classId));
  ScanKeyInit(&keys[2], Anum_pg_seclabel_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(object->objectSubId));
  ScanKeyInit(&keys[3], Anum_pg_seclabel_provider, BTEqualStrategyNumber, F_TEXTEQ, CStringGetTextDatum(provider));

  pg_seclabel = table_open(SecLabelRelationId, RowExclusiveLock);

  scan = systable_beginscan(pg_seclabel, SecLabelObjectIndexId, true, NULL, 4, keys);

  oldtup = systable_getnext(scan);
  if (HeapTupleIsValid(oldtup))
  {
    if (label == NULL)
    {
      CatalogTupleDelete(pg_seclabel, &oldtup->t_self);
    }
    else
    {
      replaces[Anum_pg_seclabel_label - 1] = true;
      newtup = heap_modify_tuple(oldtup, RelationGetDescr(pg_seclabel), values, nulls, replaces);
      CatalogTupleUpdate(pg_seclabel, &oldtup->t_self, newtup);
    }
  }
  systable_endscan(scan);

                                                        
  if (newtup == NULL && label != NULL)
  {
    newtup = heap_form_tuple(RelationGetDescr(pg_seclabel), values, nulls);
    CatalogTupleInsert(pg_seclabel, newtup);
  }

                                    
  if (newtup != NULL)
  {
    heap_freetuple(newtup);
  }

  table_close(pg_seclabel, RowExclusiveLock);
}

   
                                                                         
                                      
   
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
    CatalogTupleDelete(pg_shseclabel, &oldtup->t_self);
  }
  systable_endscan(scan);

  table_close(pg_shseclabel, RowExclusiveLock);
}

   
                                                                          
                                
   
void
DeleteSecurityLabel(const ObjectAddress *object)
{
  Relation pg_seclabel;
  ScanKeyData skey[3];
  SysScanDesc scan;
  HeapTuple oldtup;
  int nkeys;

                                                             
  if (IsSharedRelation(object->classId))
  {
    Assert(object->objectSubId == 0);
    DeleteSharedSecurityLabel(object->objectId, object->classId);
    return;
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
    CatalogTupleDelete(pg_seclabel, &oldtup->t_self);
  }
  systable_endscan(scan);

  table_close(pg_seclabel, RowExclusiveLock);
}

void
register_label_provider(const char *provider_name, check_object_relabel_type hook)
{
  LabelProvider *provider;
  MemoryContext oldcxt;

  oldcxt = MemoryContextSwitchTo(TopMemoryContext);
  provider = palloc(sizeof(LabelProvider));
  provider->provider_name = pstrdup(provider_name);
  provider->hook = hook;
  label_provider_list = lappend(label_provider_list, provider);
  MemoryContextSwitchTo(oldcxt);
}
