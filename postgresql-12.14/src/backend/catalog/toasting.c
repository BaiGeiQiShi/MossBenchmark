                                                                            
   
              
                                                                     
   
   
                                                                         
                                                                        
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "access/heapam.h"
#include "access/xact.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/namespace.h"
#include "catalog/pg_am.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_type.h"
#include "catalog/toasting.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "storage/lock.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/syscache.h"

                                                     
Oid binary_upgrade_next_toast_pg_type_oid = InvalidOid;

static void
CheckAndCreateToastTable(Oid relOid, Datum reloptions, LOCKMODE lockmode, bool check, Oid OIDOldToast);
static bool
create_toast_table(Relation rel, Oid toastOid, Oid toastIndexOid, Datum reloptions, LOCKMODE lockmode, bool check, Oid OIDOldToast);
static bool
needs_toast_table(Relation rel);

   
                             
                                                                    
                                      
   
                                                                      
                           
   
                                                                               
                                                                               
                                                                
   
void
AlterTableCreateToastTable(Oid relOid, Datum reloptions, LOCKMODE lockmode)
{
  CheckAndCreateToastTable(relOid, reloptions, lockmode, true, InvalidOid);
}

void
NewHeapCreateToastTable(Oid relOid, Datum reloptions, LOCKMODE lockmode, Oid OIDOldToast)
{
  CheckAndCreateToastTable(relOid, reloptions, lockmode, false, OIDOldToast);
}

void
NewRelationCreateToastTable(Oid relOid, Datum reloptions)
{
  CheckAndCreateToastTable(relOid, reloptions, AccessExclusiveLock, false, InvalidOid);
}

static void
CheckAndCreateToastTable(Oid relOid, Datum reloptions, LOCKMODE lockmode, bool check, Oid OIDOldToast)
{
  Relation rel;

  rel = table_open(relOid, lockmode);

                                            
  (void)create_toast_table(rel, InvalidOid, InvalidOid, reloptions, lockmode, check, OIDOldToast);

  table_close(rel, NoLock);
}

   
                                         
   
                                                                        
   
void
BootstrapToastTable(char *relName, Oid toastOid, Oid toastIndexOid)
{
  Relation rel;

  rel = table_openrv(makeRangeVar(NULL, relName, -1), AccessExclusiveLock);

  if (rel->rd_rel->relkind != RELKIND_RELATION && rel->rd_rel->relkind != RELKIND_MATVIEW)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table or materialized view", relName)));
  }

                                            
  if (!create_toast_table(rel, toastOid, toastIndexOid, (Datum)0, AccessExclusiveLock, false, InvalidOid))
  {
    elog(ERROR, "\"%s\" does not require a toast table", relName);
  }

  table_close(rel, NoLock);
}

   
                                             
   
                                    
                                                                  
                                                               
   
static bool
create_toast_table(Relation rel, Oid toastOid, Oid toastIndexOid, Datum reloptions, LOCKMODE lockmode, bool check, Oid OIDOldToast)
{
  Oid relOid = RelationGetRelid(rel);
  HeapTuple reltup;
  TupleDesc tupdesc;
  bool shared_relation;
  bool mapped_relation;
  Relation toast_rel;
  Relation class_rel;
  Oid toast_relid;
  Oid toast_typid = InvalidOid;
  Oid namespaceid;
  char toast_relname[NAMEDATALEN];
  char toast_idxname[NAMEDATALEN];
  IndexInfo *indexInfo;
  Oid collationObjectId[2];
  Oid classObjectId[2];
  int16 coloptions[2];
  ObjectAddress baseobject, toastobject;

     
                            
     
  if (rel->rd_rel->reltoastrelid != InvalidOid)
  {
    return false;
  }

     
                                                                  
     
  if (!IsBinaryUpgrade)
  {
                                   
    if (!needs_toast_table(rel))
    {
      return false;
    }
  }
  else
  {
       
                                                                   
                                                                        
       
                                                                     
                                                                       
                                                            
       
                                                                       
                                                                         
                                                                           
                                                                        
                                                                         
                                                                           
                                                                     
                                                                         
                                              
       
    if (!OidIsValid(binary_upgrade_next_toast_pg_class_oid) || !OidIsValid(binary_upgrade_next_toast_pg_type_oid))
    {
      return false;
    }
  }

     
                                                                         
                                                              
     
  if (check && lockmode != AccessExclusiveLock)
  {
    elog(ERROR, "AccessExclusiveLock required to add toast table.");
  }

     
                                          
     
  snprintf(toast_relname, sizeof(toast_relname), "pg_toast_%u", relOid);
  snprintf(toast_idxname, sizeof(toast_idxname), "pg_toast_%u_index", relOid);

                                                          
  tupdesc = CreateTemplateTupleDesc(3);
  TupleDescInitEntry(tupdesc, (AttrNumber)1, "chunk_id", OIDOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)2, "chunk_seq", INT4OID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)3, "chunk_data", BYTEAOID, -1, 0);

     
                                                                         
                                                                        
                                                   
     
  TupleDescAttr(tupdesc, 0)->attstorage = 'p';
  TupleDescAttr(tupdesc, 1)->attstorage = 'p';
  TupleDescAttr(tupdesc, 2)->attstorage = 'p';

     
                                                                       
                                                                   
     
  if (isTempOrTempToastNamespace(rel->rd_rel->relnamespace))
  {
    namespaceid = GetTempToastNamespace();
  }
  else
  {
    namespaceid = PG_TOAST_NAMESPACE;
  }

     
                                                                            
                                                                        
                                                               
     
  if (IsBinaryUpgrade && OidIsValid(binary_upgrade_next_toast_pg_type_oid))
  {
    toast_typid = binary_upgrade_next_toast_pg_type_oid;
    binary_upgrade_next_toast_pg_type_oid = InvalidOid;
  }

                                                           
  shared_relation = rel->rd_rel->relisshared;

                                                     
  mapped_relation = RelationIsMapped(rel);

  toast_relid = heap_create_with_catalog(toast_relname, namespaceid, rel->rd_rel->reltablespace, toastOid, toast_typid, InvalidOid, rel->rd_rel->relowner, rel->rd_rel->relam, tupdesc, NIL, RELKIND_TOASTVALUE, rel->rd_rel->relpersistence, shared_relation, mapped_relation, ONCOMMIT_NOOP, reloptions, false, true, true, OIDOldToast, NULL);
  Assert(toast_relid != InvalidOid);

                                                                  
  CommandCounterIncrement();

                                                               
  toast_rel = table_open(toast_relid, ShareLock);

     
                                                 
     
                                                                           
                                                                     
                                                                            
                                                                             
                                                                       
                                                                             
                    
     

  indexInfo = makeNode(IndexInfo);
  indexInfo->ii_NumIndexAttrs = 2;
  indexInfo->ii_NumIndexKeyAttrs = 2;
  indexInfo->ii_IndexAttrNumbers[0] = 1;
  indexInfo->ii_IndexAttrNumbers[1] = 2;
  indexInfo->ii_Expressions = NIL;
  indexInfo->ii_ExpressionsState = NIL;
  indexInfo->ii_Predicate = NIL;
  indexInfo->ii_PredicateState = NULL;
  indexInfo->ii_ExclusionOps = NULL;
  indexInfo->ii_ExclusionProcs = NULL;
  indexInfo->ii_ExclusionStrats = NULL;
  indexInfo->ii_Unique = true;
  indexInfo->ii_ReadyForInserts = true;
  indexInfo->ii_Concurrent = false;
  indexInfo->ii_BrokenHotChain = false;
  indexInfo->ii_ParallelWorkers = 0;
  indexInfo->ii_Am = BTREE_AM_OID;
  indexInfo->ii_AmCache = NULL;
  indexInfo->ii_Context = CurrentMemoryContext;

  collationObjectId[0] = InvalidOid;
  collationObjectId[1] = InvalidOid;

  classObjectId[0] = OID_BTREE_OPS_OID;
  classObjectId[1] = INT4_BTREE_OPS_OID;

  coloptions[0] = 0;
  coloptions[1] = 0;

  index_create(toast_rel, toast_idxname, toastIndexOid, InvalidOid, InvalidOid, InvalidOid, indexInfo, list_make2("chunk_id", "chunk_seq"), BTREE_AM_OID, rel->rd_rel->reltablespace, collationObjectId, classObjectId, coloptions, (Datum)0, INDEX_CREATE_IS_PRIMARY, 0, true, true, NULL);

  table_close(toast_rel, NoLock);

     
                                                                       
     
  class_rel = table_open(RelationRelationId, RowExclusiveLock);

  reltup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relOid));
  if (!HeapTupleIsValid(reltup))
  {
    elog(ERROR, "cache lookup failed for relation %u", relOid);
  }

  ((Form_pg_class)GETSTRUCT(reltup))->reltoastrelid = toast_relid;

  if (!IsBootstrapProcessingMode())
  {
                                                 
    CatalogTupleUpdate(class_rel, &reltup->t_self, reltup);
  }
  else
  {
                                                                      
    heap_inplace_update(class_rel, reltup);
  }

  heap_freetuple(reltup);

  table_close(class_rel, RowExclusiveLock);

     
                                                                         
                                                                           
           
     
  if (!IsBootstrapProcessingMode())
  {
    baseobject.classId = RelationRelationId;
    baseobject.objectId = relOid;
    baseobject.objectSubId = 0;
    toastobject.classId = RelationRelationId;
    toastobject.objectId = toast_relid;
    toastobject.objectSubId = 0;

    recordDependencyOn(&toastobject, &baseobject, DEPENDENCY_INTERNAL);
  }

     
                          
     
  CommandCounterIncrement();

  return true;
}

   
                                                       
   
static bool
needs_toast_table(Relation rel)
{
     
                                                             
     
  if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    return false;
  }

     
                                                                      
                                                                      
     
  if (rel->rd_rel->relisshared && !IsBootstrapProcessingMode())
  {
    return false;
  }

     
                                                                            
                                                             
                                                                             
                                              
     
  if (IsCatalogRelation(rel) && !IsBootstrapProcessingMode())
  {
    return false;
  }

                                     
  return table_relation_needs_toast_table(rel);
}
