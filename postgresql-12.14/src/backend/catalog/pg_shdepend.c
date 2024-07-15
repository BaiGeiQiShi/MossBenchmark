                                                                            
   
                 
                                                                  
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_database.h"
#include "catalog/pg_default_acl.h"
#include "catalog/pg_event_trigger.h"
#include "catalog/pg_extension.h"
#include "catalog/pg_foreign_data_wrapper.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_language.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_largeobject_metadata.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_shdepend.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_type.h"
#include "catalog/pg_user_mapping.h"
#include "commands/alter.h"
#include "commands/dbcommands.h"
#include "commands/collationcmds.h"
#include "commands/conversioncmds.h"
#include "commands/defrem.h"
#include "commands/event_trigger.h"
#include "commands/extension.h"
#include "commands/policy.h"
#include "commands/proclang.h"
#include "commands/publicationcmds.h"
#include "commands/schemacmds.h"
#include "commands/subscriptioncmds.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "commands/typecmds.h"
#include "storage/lmgr.h"
#include "miscadmin.h"
#include "utils/acl.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/syscache.h"

typedef enum
{
  LOCAL_OBJECT,
  SHARED_OBJECT,
  REMOTE_OBJECT
} SharedDependencyObjectType;

typedef struct
{
  ObjectAddress object;
  char deptype;
  SharedDependencyObjectType objtype;
} ShDependObjectInfo;

static void
getOidListDiff(Oid *list1, int *nlist1, Oid *list2, int *nlist2);
static Oid
classIdGetDbId(Oid classId);
static void
shdepChangeDep(Relation sdepRel, Oid classid, Oid objid, int32 objsubid, Oid refclassid, Oid refobjid, SharedDependencyType deptype);
static void
shdepAddDependency(Relation sdepRel, Oid classId, Oid objectId, int32 objsubId, Oid refclassId, Oid refobjId, SharedDependencyType deptype);
static void
shdepDropDependency(Relation sdepRel, Oid classId, Oid objectId, int32 objsubId, bool drop_subobjects, Oid refclassId, Oid refobjId, SharedDependencyType deptype);
static void
storeObjectDescription(StringInfo descs, SharedDependencyObjectType type, ObjectAddress *object, SharedDependencyType deptype, int count);
static bool
isSharedObjectPinned(Oid classId, Oid objectId, Relation sdepRel);

   
                            
   
                                                                               
                                                                     
                                               
   
                                                                    
                                                                    
                               
   
                                                    
   
void
recordSharedDependencyOn(ObjectAddress *depender, ObjectAddress *referenced, SharedDependencyType deptype)
{
  Relation sdepRel;

     
                                               
     
  Assert(depender->objectSubId == 0);
  Assert(referenced->objectSubId == 0);

     
                                                                       
                                                                          
     
  if (IsBootstrapProcessingMode())
  {
    return;
  }

  sdepRel = table_open(SharedDependRelationId, RowExclusiveLock);

                                                       
  if (!isSharedObjectPinned(referenced->classId, referenced->objectId, sdepRel))
  {
    shdepAddDependency(sdepRel, depender->classId, depender->objectId, depender->objectSubId, referenced->classId, referenced->objectId, deptype);
  }

  table_close(sdepRel, RowExclusiveLock);
}

   
                           
   
                                                                              
                                      
   
                                                                              
                                 
   
void
recordDependencyOnOwner(Oid classId, Oid objectId, Oid owner)
{
  ObjectAddress myself, referenced;

  myself.classId = classId;
  myself.objectId = objectId;
  myself.objectSubId = 0;

  referenced.classId = AuthIdRelationId;
  referenced.objectId = owner;
  referenced.objectSubId = 0;

  recordSharedDependencyOn(&myself, &referenced, SHARED_DEPENDENCY_OWNER);
}

   
                  
   
                                                                         
                                                                          
                      
   
                                                                         
                                                                        
                                                                     
                                             
   
                                                                        
                                                                      
                                                                    
                                                                       
                                                                
   
                                                                         
           
   
static void
shdepChangeDep(Relation sdepRel, Oid classid, Oid objid, int32 objsubid, Oid refclassid, Oid refobjid, SharedDependencyType deptype)
{
  Oid dbid = classIdGetDbId(classid);
  HeapTuple oldtup = NULL;
  HeapTuple scantup;
  ScanKeyData key[4];
  SysScanDesc scan;

     
                                                                             
                 
     
  shdepLockAndCheckObject(refclassid, refobjid);

     
                               
     
  ScanKeyInit(&key[0], Anum_pg_shdepend_dbid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(dbid));
  ScanKeyInit(&key[1], Anum_pg_shdepend_classid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(classid));
  ScanKeyInit(&key[2], Anum_pg_shdepend_objid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(objid));
  ScanKeyInit(&key[3], Anum_pg_shdepend_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(objsubid));

  scan = systable_beginscan(sdepRel, SharedDependDependerIndexId, true, NULL, 4, key);

  while ((scantup = systable_getnext(scan)) != NULL)
  {
                                                     
    if (((Form_pg_shdepend)GETSTRUCT(scantup))->deptype != deptype)
    {
      continue;
    }
                                               
    if (oldtup)
    {
      elog(ERROR, "multiple pg_shdepend entries for object %u/%u/%d deptype %c", classid, objid, objsubid, deptype);
    }
    oldtup = heap_copytuple(scantup);
  }

  systable_endscan(scan);

  if (isSharedObjectPinned(refclassid, refobjid, sdepRel))
  {
                                                                   
    if (oldtup)
    {
      CatalogTupleDelete(sdepRel, &oldtup->t_self);
    }
  }
  else if (oldtup)
  {
                                       
    Form_pg_shdepend shForm = (Form_pg_shdepend)GETSTRUCT(oldtup);

                                                                 
    shForm->refclassid = refclassid;
    shForm->refobjid = refobjid;

    CatalogTupleUpdate(sdepRel, &oldtup->t_self, oldtup);
  }
  else
  {
                                  
    Datum values[Natts_pg_shdepend];
    bool nulls[Natts_pg_shdepend];

    memset(nulls, false, sizeof(nulls));

    values[Anum_pg_shdepend_dbid - 1] = ObjectIdGetDatum(dbid);
    values[Anum_pg_shdepend_classid - 1] = ObjectIdGetDatum(classid);
    values[Anum_pg_shdepend_objid - 1] = ObjectIdGetDatum(objid);
    values[Anum_pg_shdepend_objsubid - 1] = Int32GetDatum(objsubid);

    values[Anum_pg_shdepend_refclassid - 1] = ObjectIdGetDatum(refclassid);
    values[Anum_pg_shdepend_refobjid - 1] = ObjectIdGetDatum(refobjid);
    values[Anum_pg_shdepend_deptype - 1] = CharGetDatum(deptype);

       
                                                                         
                                  
       
    oldtup = heap_form_tuple(RelationGetDescr(sdepRel), values, nulls);
    CatalogTupleInsert(sdepRel, oldtup);
  }

  if (oldtup)
  {
    heap_freetuple(oldtup);
  }
}

   
                           
   
                                                                
   
                                                                       
                
   
void
changeDependencyOnOwner(Oid classId, Oid objectId, Oid newOwnerId)
{
  Relation sdepRel;

  sdepRel = table_open(SharedDependRelationId, RowExclusiveLock);

                                                
  shdepChangeDep(sdepRel, classId, objectId, 0, AuthIdRelationId, newOwnerId, SHARED_DEPENDENCY_OWNER);

               
                                                                        
                                                                         
                                                       
     
                                                                          
                                                            
                                             
                                                  
                                                  
                                                                         
                                                                        
                      
     
                                                                         
                                                                       
                                                                  
               
     
  shdepDropDependency(sdepRel, classId, objectId, 0, true, AuthIdRelationId, newOwnerId, SHARED_DEPENDENCY_ACL);

  table_close(sdepRel, RowExclusiveLock);
}

   
                                
   
                                                                              
                                               
   
                                                                       
                                            
   
void
recordDependencyOnTablespace(Oid classId, Oid objectId, Oid tablespace)
{
  ObjectAddress myself, referenced;

  ObjectAddressSet(myself, classId, objectId);
  ObjectAddressSet(referenced, TableSpaceRelationId, tablespace);

  recordSharedDependencyOn(&myself, &referenced, SHARED_DEPENDENCY_TABLESPACE);
}

   
                                
   
                                                                     
   
                                                                       
                     
   
void
changeDependencyOnTablespace(Oid classId, Oid objectId, Oid newTablespaceId)
{
  Relation sdepRel;

  sdepRel = table_open(SharedDependRelationId, RowExclusiveLock);

  if (newTablespaceId != DEFAULTTABLESPACE_OID && newTablespaceId != InvalidOid)
  {
    shdepChangeDep(sdepRel, classId, objectId, 0, TableSpaceRelationId, newTablespaceId, SHARED_DEPENDENCY_TABLESPACE);
  }
  else
  {
    shdepDropDependency(sdepRel, classId, objectId, 0, true, InvalidOid, InvalidOid, SHARED_DEPENDENCY_INVALID);
  }

  table_close(sdepRel, RowExclusiveLock);
}

   
                  
                                      
   
                                                                             
                                                               
                                                        
   
static void
getOidListDiff(Oid *list1, int *nlist1, Oid *list2, int *nlist2)
{
  int in1, in2, out1, out2;

  in1 = in2 = out1 = out2 = 0;
  while (in1 < *nlist1 && in2 < *nlist2)
  {
    if (list1[in1] == list2[in2])
    {
                                
      in1++;
      in2++;
    }
    else if (list1[in1] < list2[in2])
    {
                                      
      list1[out1++] = list1[in1++];
    }
    else
    {
                                      
      list2[out2++] = list2[in2++];
    }
  }

                                                    
  while (in1 < *nlist1)
  {
    list1[out1++] = list1[in1++];
  }

                                                    
  while (in2 < *nlist2)
  {
    list2[out2++] = list2[in2++];
  }

  *nlist1 = out1;
  *nlist2 = out2;
}

   
                         
                                                                         
   
                                                                      
                                   
                                                                  
                                                                  
   
                                                                        
                                                              
   
                                                                             
                                                                             
                                                                            
                                                                              
                                                                            
                                                                      
                                                                          
                                                                               
   
                                                                         
                                                                        
                                                            
   
void
updateAclDependencies(Oid classId, Oid objectId, int32 objsubId, Oid ownerId, int noldmembers, Oid *oldmembers, int nnewmembers, Oid *newmembers)
{
  Relation sdepRel;
  int i;

     
                                                                            
                                           
     
                                                               
     
  getOidListDiff(oldmembers, &noldmembers, newmembers, &nnewmembers);

  if (noldmembers > 0 || nnewmembers > 0)
  {
    sdepRel = table_open(SharedDependRelationId, RowExclusiveLock);

                                                           
    for (i = 0; i < nnewmembers; i++)
    {
      Oid roleid = newmembers[i];

         
                                                                       
                                                                         
                                            
         
      if (roleid == ownerId)
      {
        continue;
      }

                                                                 
      if (isSharedObjectPinned(AuthIdRelationId, roleid, sdepRel))
      {
        continue;
      }

      shdepAddDependency(sdepRel, classId, objectId, objsubId, AuthIdRelationId, roleid, SHARED_DEPENDENCY_ACL);
    }

                                              
    for (i = 0; i < noldmembers; i++)
    {
      Oid roleid = oldmembers[i];

                                         
      if (roleid == ownerId)
      {
        continue;
      }

                             
      if (isSharedObjectPinned(AuthIdRelationId, roleid, sdepRel))
      {
        continue;
      }

      shdepDropDependency(sdepRel, classId, objectId, objsubId, false,                              
          AuthIdRelationId, roleid, SHARED_DEPENDENCY_ACL);
    }

    table_close(sdepRel, RowExclusiveLock);
  }

  if (oldmembers)
  {
    pfree(oldmembers);
  }
  if (newmembers)
  {
    pfree(newmembers);
  }
}

   
                                                                    
   
typedef struct
{
  Oid dbOid;
  int count;
} remoteDep;

   
                                                 
   
static int
shared_dependency_comparator(const void *a, const void *b)
{
  const ShDependObjectInfo *obja = (const ShDependObjectInfo *)a;
  const ShDependObjectInfo *objb = (const ShDependObjectInfo *)b;

     
                                        
     
  if (obja->object.objectId < objb->object.objectId)
  {
    return -1;
  }
  if (obja->object.objectId > objb->object.objectId)
  {
    return 1;
  }

     
                                                                         
                                                         
     
  if (obja->object.classId < objb->object.classId)
  {
    return -1;
  }
  if (obja->object.classId > objb->object.classId)
  {
    return 1;
  }

     
                           
     
                                                                            
                 
     
  if ((unsigned int)obja->object.objectSubId < (unsigned int)objb->object.objectSubId)
  {
    return -1;
  }
  if ((unsigned int)obja->object.objectSubId > (unsigned int)objb->object.objectSubId)
  {
    return 1;
  }

     
                                                                            
                                                                       
                                     
     
  if (obja->deptype < objb->deptype)
  {
    return -1;
  }
  if (obja->deptype > objb->deptype)
  {
    return 1;
  }

  return 0;
}

   
                           
   
                                                                        
                              
   
                                                                              
                                                                            
                                                                            
                                                                             
                                                                             
                           
   
                                                                              
                                                                             
                                                                              
                                                                         
                                                
   
                                                                     
   
bool
checkSharedDependencies(Oid classId, Oid objectId, char **detail_msg, char **detail_log_msg)
{
  Relation sdepRel;
  ScanKeyData key[2];
  SysScanDesc scan;
  HeapTuple tup;
  int numReportedDeps = 0;
  int numNotReportedDeps = 0;
  int numNotReportedDbs = 0;
  List *remDeps = NIL;
  ListCell *cell;
  ObjectAddress object;
  ShDependObjectInfo *objects;
  int numobjects;
  int allocedobjects;
  StringInfoData descs;
  StringInfoData alldescs;

     
                                                                   
                                                                     
                                                                        
     
                                                                        
                                                                           
                                                    
     
#define MAX_REPORTED_DEPS 100

  allocedobjects = 128;                                   
  objects = (ShDependObjectInfo *)palloc(allocedobjects * sizeof(ShDependObjectInfo));
  numobjects = 0;
  initStringInfo(&descs);
  initStringInfo(&alldescs);

  sdepRel = table_open(SharedDependRelationId, AccessShareLock);

  ScanKeyInit(&key[0], Anum_pg_shdepend_refclassid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(classId));
  ScanKeyInit(&key[1], Anum_pg_shdepend_refobjid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(objectId));

  scan = systable_beginscan(sdepRel, SharedDependReferenceIndexId, true, NULL, 2, key);

  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    Form_pg_shdepend sdepForm = (Form_pg_shdepend)GETSTRUCT(tup);

                                             
    if (sdepForm->deptype == SHARED_DEPENDENCY_PIN)
    {
      object.classId = classId;
      object.objectId = objectId;
      object.objectSubId = 0;
      ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST), errmsg("cannot drop %s because it is required by the database system", getObjectDescription(&object))));
    }

    object.classId = sdepForm->classid;
    object.objectId = sdepForm->objid;
    object.objectSubId = sdepForm->objsubid;

       
                                                                    
                                            
       
                                                                          
                             
       
    if (sdepForm->dbid == MyDatabaseId || sdepForm->dbid == InvalidOid)
    {
      if (numobjects >= allocedobjects)
      {
        allocedobjects *= 2;
        objects = (ShDependObjectInfo *)repalloc(objects, allocedobjects * sizeof(ShDependObjectInfo));
      }
      objects[numobjects].object = object;
      objects[numobjects].deptype = sdepForm->deptype;
      objects[numobjects].objtype = (sdepForm->dbid == MyDatabaseId) ? LOCAL_OBJECT : SHARED_OBJECT;
      numobjects++;
    }
    else
    {
                                                            
      remoteDep *dep;
      bool stored = false;

         
                                                                      
                                                                  
                                                                        
                    
         
      foreach (cell, remDeps)
      {
        dep = lfirst(cell);
        if (dep->dbOid == sdepForm->dbid)
        {
          dep->count++;
          stored = true;
          break;
        }
      }
      if (!stored)
      {
        dep = (remoteDep *)palloc(sizeof(remoteDep));
        dep->dbOid = sdepForm->dbid;
        dep->count = 1;
        remDeps = lappend(remDeps, dep);
      }
    }
  }

  systable_endscan(scan);

  table_close(sdepRel, AccessShareLock);

     
                                               
     
  if (numobjects > 1)
  {
    qsort((void *)objects, numobjects, sizeof(ShDependObjectInfo), shared_dependency_comparator);
  }

  for (int i = 0; i < numobjects; i++)
  {
    if (numReportedDeps < MAX_REPORTED_DEPS)
    {
      numReportedDeps++;
      storeObjectDescription(&descs, objects[i].objtype, &objects[i].object, objects[i].deptype, 0);
    }
    else
    {
      numNotReportedDeps++;
    }
    storeObjectDescription(&alldescs, objects[i].objtype, &objects[i].object, objects[i].deptype, 0);
  }

     
                                                 
     
  foreach (cell, remDeps)
  {
    remoteDep *dep = lfirst(cell);

    object.classId = DatabaseRelationId;
    object.objectId = dep->dbOid;
    object.objectSubId = 0;

    if (numReportedDeps < MAX_REPORTED_DEPS)
    {
      numReportedDeps++;
      storeObjectDescription(&descs, REMOTE_OBJECT, &object, SHARED_DEPENDENCY_INVALID, dep->count);
    }
    else
    {
      numNotReportedDbs++;
    }
    storeObjectDescription(&alldescs, REMOTE_OBJECT, &object, SHARED_DEPENDENCY_INVALID, dep->count);
  }

  pfree(objects);
  list_free_deep(remDeps);

  if (descs.len == 0)
  {
    pfree(descs.data);
    pfree(alldescs.data);
    *detail_msg = *detail_log_msg = NULL;
    return false;
  }

  if (numNotReportedDeps > 0)
  {
    appendStringInfo(&descs,
        ngettext("\nand %d other object "
                 "(see server log for list)",
            "\nand %d other objects "
            "(see server log for list)",
            numNotReportedDeps),
        numNotReportedDeps);
  }
  if (numNotReportedDbs > 0)
  {
    appendStringInfo(&descs,
        ngettext("\nand objects in %d other database "
                 "(see server log for list)",
            "\nand objects in %d other databases "
            "(see server log for list)",
            numNotReportedDbs),
        numNotReportedDbs);
  }

  *detail_msg = descs.data;
  *detail_log_msg = alldescs.data;
  return true;
}

   
                            
   
                                                                        
                                                               
   
void
copyTemplateDependencies(Oid templateDbId, Oid newDbId)
{
  Relation sdepRel;
  TupleDesc sdepDesc;
  ScanKeyData key[1];
  SysScanDesc scan;
  HeapTuple tup;
  CatalogIndexState indstate;
  Datum values[Natts_pg_shdepend];
  bool nulls[Natts_pg_shdepend];
  bool replace[Natts_pg_shdepend];

  sdepRel = table_open(SharedDependRelationId, RowExclusiveLock);
  sdepDesc = RelationGetDescr(sdepRel);

  indstate = CatalogOpenIndexes(sdepRel);

                                                 
  ScanKeyInit(&key[0], Anum_pg_shdepend_dbid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(templateDbId));

  scan = systable_beginscan(sdepRel, SharedDependDependerIndexId, true, NULL, 1, key);

                                                              
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));
  memset(replace, false, sizeof(replace));

  replace[Anum_pg_shdepend_dbid - 1] = true;
  values[Anum_pg_shdepend_dbid - 1] = ObjectIdGetDatum(newDbId);

     
                                                                            
                                                                          
                                                                            
                                                                            
                   
     
  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    HeapTuple newtup;

    newtup = heap_modify_tuple(tup, sdepDesc, values, nulls, replace);
    CatalogTupleInsertWithInfo(sdepRel, newtup, indstate);

    heap_freetuple(newtup);
  }

  systable_endscan(scan);

  CatalogCloseIndexes(indstate);
  table_close(sdepRel, RowExclusiveLock);
}

   
                            
   
                                                                       
            
   
void
dropDatabaseDependencies(Oid databaseId)
{
  Relation sdepRel;
  ScanKeyData key[1];
  SysScanDesc scan;
  HeapTuple tup;

  sdepRel = table_open(SharedDependRelationId, RowExclusiveLock);

     
                                                                          
            
     
  ScanKeyInit(&key[0], Anum_pg_shdepend_dbid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(databaseId));
                                                   

  scan = systable_beginscan(sdepRel, SharedDependDependerIndexId, true, NULL, 1, key);

  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    CatalogTupleDelete(sdepRel, &tup->t_self);
  }

  systable_endscan(scan);

                                                                   
  shdepDropDependency(sdepRel, DatabaseRelationId, databaseId, 0, true, InvalidOid, InvalidOid, SHARED_DEPENDENCY_INVALID);

  table_close(sdepRel, RowExclusiveLock);
}

   
                                    
   
                                                                          
                                                                            
                                                                  
   
                                                                         
                                               
   
void
deleteSharedDependencyRecordsFor(Oid classId, Oid objectId, int32 objectSubId)
{
  Relation sdepRel;

  sdepRel = table_open(SharedDependRelationId, RowExclusiveLock);

  shdepDropDependency(sdepRel, classId, objectId, objectSubId, (objectSubId == 0), InvalidOid, InvalidOid, SHARED_DEPENDENCY_INVALID);

  table_close(sdepRel, RowExclusiveLock);
}

   
                      
                                                      
   
                                                                         
           
   
static void
shdepAddDependency(Relation sdepRel, Oid classId, Oid objectId, int32 objsubId, Oid refclassId, Oid refobjId, SharedDependencyType deptype)
{
  HeapTuple tup;
  Datum values[Natts_pg_shdepend];
  bool nulls[Natts_pg_shdepend];

     
                                                                            
                                                                             
                          
     
  shdepLockAndCheckObject(refclassId, refobjId);

  memset(nulls, false, sizeof(nulls));

     
                                                   
     
  values[Anum_pg_shdepend_dbid - 1] = ObjectIdGetDatum(classIdGetDbId(classId));
  values[Anum_pg_shdepend_classid - 1] = ObjectIdGetDatum(classId);
  values[Anum_pg_shdepend_objid - 1] = ObjectIdGetDatum(objectId);
  values[Anum_pg_shdepend_objsubid - 1] = Int32GetDatum(objsubId);

  values[Anum_pg_shdepend_refclassid - 1] = ObjectIdGetDatum(refclassId);
  values[Anum_pg_shdepend_refobjid - 1] = ObjectIdGetDatum(refobjId);
  values[Anum_pg_shdepend_deptype - 1] = CharGetDatum(deptype);

  tup = heap_form_tuple(sdepRel->rd_att, values, nulls);

  CatalogTupleInsert(sdepRel, tup);

                
  heap_freetuple(tup);
}

   
                       
                                                              
   
                                                    
                                                                       
                                                                        
                                                                    
                                                                             
   
                                                                           
                              
   
                                                                         
           
   
static void
shdepDropDependency(Relation sdepRel, Oid classId, Oid objectId, int32 objsubId, bool drop_subobjects, Oid refclassId, Oid refobjId, SharedDependencyType deptype)
{
  ScanKeyData key[4];
  int nkeys;
  SysScanDesc scan;
  HeapTuple tup;

                                                      
  ScanKeyInit(&key[0], Anum_pg_shdepend_dbid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(classIdGetDbId(classId)));
  ScanKeyInit(&key[1], Anum_pg_shdepend_classid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(classId));
  ScanKeyInit(&key[2], Anum_pg_shdepend_objid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(objectId));
  if (drop_subobjects)
  {
    nkeys = 3;
  }
  else
  {
    ScanKeyInit(&key[3], Anum_pg_shdepend_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(objsubId));
    nkeys = 4;
  }

  scan = systable_beginscan(sdepRel, SharedDependDependerIndexId, true, NULL, nkeys, key);

  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    Form_pg_shdepend shdepForm = (Form_pg_shdepend)GETSTRUCT(tup);

                                                           
    if (OidIsValid(refclassId) && shdepForm->refclassid != refclassId)
    {
      continue;
    }
    if (OidIsValid(refobjId) && shdepForm->refobjid != refobjId)
    {
      continue;
    }
    if (deptype != SHARED_DEPENDENCY_INVALID && shdepForm->deptype != deptype)
    {
      continue;
    }

                       
    CatalogTupleDelete(sdepRel, &tup->t_self);
  }

  systable_endscan(scan);
}

   
                  
   
                                                                         
                                                                     
                                                                      
   
static Oid
classIdGetDbId(Oid classId)
{
  Oid dbId;

  if (IsSharedRelation(classId))
  {
    dbId = InvalidOid;
  }
  else
  {
    dbId = MyDatabaseId;
  }

  return dbId;
}

   
                           
   
                                                                
                                                                  
                                                                   
                    
   
void
shdepLockAndCheckObject(Oid classId, Oid objectId)
{
                                                                           
  LockSharedObject(classId, objectId, 0, AccessShareLock);

  switch (classId)
  {
  case AuthIdRelationId:
    if (!SearchSysCacheExists1(AUTHOID, ObjectIdGetDatum(objectId)))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("role %u was concurrently dropped", objectId)));
    }
    break;

  case TableSpaceRelationId:
  {
                                                           
    char *tablespace = get_tablespace_name(objectId);

    if (tablespace == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("tablespace %u was concurrently dropped", objectId)));
    }
    pfree(tablespace);
    break;
  }

  case DatabaseRelationId:
  {
                                                         
    char *database = get_database_name(objectId);

    if (database == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("database %u was concurrently dropped", objectId)));
    }
    pfree(database);
    break;
  }

  default:
    elog(ERROR, "unrecognized shared classId: %u", classId);
  }
}

   
                          
                                                            
   
                                                                     
                                                                          
                                                                         
                                    
   
                                                                          
                                                                            
                                                                           
                                                              
   
static void
storeObjectDescription(StringInfo descs, SharedDependencyObjectType type, ObjectAddress *object, SharedDependencyType deptype, int count)
{
  char *objdesc = getObjectDescription(object);

     
                                                                       
     
  if (objdesc == NULL)
  {
    return;
  }

                                       
  if (descs->len != 0)
  {
    appendStringInfoChar(descs, '\n');
  }

  switch (type)
  {
  case LOCAL_OBJECT:
  case SHARED_OBJECT:
    if (deptype == SHARED_DEPENDENCY_OWNER)
    {
      appendStringInfo(descs, _("owner of %s"), objdesc);
    }
    else if (deptype == SHARED_DEPENDENCY_ACL)
    {
      appendStringInfo(descs, _("privileges for %s"), objdesc);
    }
    else if (deptype == SHARED_DEPENDENCY_POLICY)
    {
      appendStringInfo(descs, _("target of %s"), objdesc);
    }
    else if (deptype == SHARED_DEPENDENCY_TABLESPACE)
    {
      appendStringInfo(descs, _("tablespace for %s"), objdesc);
    }
    else
    {
      elog(ERROR, "unrecognized dependency type: %d", (int)deptype);
    }
    break;

  case REMOTE_OBJECT:
                                                     
    appendStringInfo(descs, ngettext("%d object in %s", "%d objects in %s", count), count, objdesc);
    break;

  default:
    elog(ERROR, "unrecognized object type: %d", type);
  }

  pfree(objdesc);
}

   
                        
                                                                            
   
                                                                         
           
   
static bool
isSharedObjectPinned(Oid classId, Oid objectId, Relation sdepRel)
{
  bool result = false;
  ScanKeyData key[2];
  SysScanDesc scan;
  HeapTuple tup;

  ScanKeyInit(&key[0], Anum_pg_shdepend_refclassid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(classId));
  ScanKeyInit(&key[1], Anum_pg_shdepend_refobjid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(objectId));

  scan = systable_beginscan(sdepRel, SharedDependReferenceIndexId, true, NULL, 2, key);

     
                                                                       
                                                                          
                                                                          
                   
     
  tup = systable_getnext(scan);
  if (HeapTupleIsValid(tup))
  {
    Form_pg_shdepend shdepForm = (Form_pg_shdepend)GETSTRUCT(tup);

    if (shdepForm->deptype == SHARED_DEPENDENCY_PIN)
    {
      result = true;
    }
  }

  systable_endscan(scan);

  return result;
}

   
                  
   
                                                                          
                                                                          
                         
   
                                                                        
                                                                      
                                                                    
                                              
   
void
shdepDropOwned(List *roleids, DropBehavior behavior)
{
  Relation sdepRel;
  ListCell *cell;
  ObjectAddresses *deleteobjs;

  deleteobjs = new_object_addresses();

     
                                                                         
                                                                             
                        
     
  sdepRel = table_open(SharedDependRelationId, RowExclusiveLock);

     
                                                                       
                                                 
     
  foreach (cell, roleids)
  {
    Oid roleid = lfirst_oid(cell);
    ScanKeyData key[2];
    SysScanDesc scan;
    HeapTuple tuple;

                                         
    if (isSharedObjectPinned(AuthIdRelationId, roleid, sdepRel))
    {
      ObjectAddress obj;

      obj.classId = AuthIdRelationId;
      obj.objectId = roleid;
      obj.objectSubId = 0;

      ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST), errmsg("cannot drop objects owned by %s because they are "
                                                                             "required by the database system",
                                                                          getObjectDescription(&obj))));
    }

    ScanKeyInit(&key[0], Anum_pg_shdepend_refclassid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(AuthIdRelationId));
    ScanKeyInit(&key[1], Anum_pg_shdepend_refobjid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(roleid));

    scan = systable_beginscan(sdepRel, SharedDependReferenceIndexId, true, NULL, 2, key);

    while ((tuple = systable_getnext(scan)) != NULL)
    {
      Form_pg_shdepend sdepForm = (Form_pg_shdepend)GETSTRUCT(tuple);
      ObjectAddress obj;

         
                                                                      
                  
         
      if (sdepForm->dbid != MyDatabaseId && sdepForm->dbid != InvalidOid)
      {
        continue;
      }

      switch (sdepForm->deptype)
      {
                              
      case SHARED_DEPENDENCY_PIN:
      case SHARED_DEPENDENCY_INVALID:
        elog(ERROR, "unexpected dependency type");
        break;
      case SHARED_DEPENDENCY_ACL:
        RemoveRoleFromObjectACL(roleid, sdepForm->classid, sdepForm->objid);
        break;
      case SHARED_DEPENDENCY_POLICY:
           
                                                                
                   
           
        if (!RemoveRoleFromObjectPolicy(roleid, sdepForm->classid, sdepForm->objid))
        {
          obj.classId = sdepForm->classid;
          obj.objectId = sdepForm->objid;
          obj.objectSubId = sdepForm->objsubid;
             
                                                                 
                                                               
                                                              
                                  
             
          AcquireDeletionLock(&obj, 0);
          if (!systable_recheck_tuple(scan, tuple))
          {
            ReleaseDeletionLock(&obj);
            break;
          }
          add_exact_object_address(&obj, deleteobjs);
        }
        break;
      case SHARED_DEPENDENCY_OWNER:
                                                           
        if (sdepForm->dbid == MyDatabaseId)
        {
          obj.classId = sdepForm->classid;
          obj.objectId = sdepForm->objid;
          obj.objectSubId = sdepForm->objsubid;
                        
          AcquireDeletionLock(&obj, 0);
          if (!systable_recheck_tuple(scan, tuple))
          {
            ReleaseDeletionLock(&obj);
            break;
          }
          add_exact_object_address(&obj, deleteobjs);
        }
        break;
      }
    }

    systable_endscan(scan);
  }

     
                                                                      
                                                                           
                                                                            
                                                    
     
  sort_object_addresses(deleteobjs);

                                                     
  performMultipleDeletions(deleteobjs, behavior, 0);

  table_close(sdepRel, RowExclusiveLock);

  free_object_addresses(deleteobjs);
}

   
                      
   
                                                                       
                                     
   
void
shdepReassignOwned(List *roleids, Oid newrole)
{
  Relation sdepRel;
  ListCell *cell;

     
                                                                         
                                                                             
                        
     
  sdepRel = table_open(SharedDependRelationId, RowExclusiveLock);

  foreach (cell, roleids)
  {
    SysScanDesc scan;
    ScanKeyData key[2];
    HeapTuple tuple;
    Oid roleid = lfirst_oid(cell);

                                        
    if (isSharedObjectPinned(AuthIdRelationId, roleid, sdepRel))
    {
      ObjectAddress obj;

      obj.classId = AuthIdRelationId;
      obj.objectId = roleid;
      obj.objectSubId = 0;

      ereport(ERROR, (errcode(ERRCODE_DEPENDENT_OBJECTS_STILL_EXIST), errmsg("cannot reassign ownership of objects owned by %s because they are required by the database system", getObjectDescription(&obj))));

         
                                                                   
                                                    
         
    }

    ScanKeyInit(&key[0], Anum_pg_shdepend_refclassid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(AuthIdRelationId));
    ScanKeyInit(&key[1], Anum_pg_shdepend_refobjid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(roleid));

    scan = systable_beginscan(sdepRel, SharedDependReferenceIndexId, true, NULL, 2, key);

    while ((tuple = systable_getnext(scan)) != NULL)
    {
      Form_pg_shdepend sdepForm = (Form_pg_shdepend)GETSTRUCT(tuple);
      MemoryContext cxt, oldcxt;

         
                                                                      
                  
         
      if (sdepForm->dbid != MyDatabaseId && sdepForm->dbid != InvalidOid)
      {
        continue;
      }

                                                        
      if (sdepForm->deptype == SHARED_DEPENDENCY_PIN)
      {
        elog(ERROR, "unexpected shared pin");
      }

                                                 
      if (sdepForm->deptype != SHARED_DEPENDENCY_OWNER)
      {
        continue;
      }

         
                                                                 
                                                                       
                                                                      
                                                                        
                                                                 
         
      cxt = AllocSetContextCreate(CurrentMemoryContext, "shdepReassignOwned", ALLOCSET_DEFAULT_SIZES);
      oldcxt = MemoryContextSwitchTo(cxt);

                                                  
      switch (sdepForm->classid)
      {
      case TypeRelationId:
        AlterTypeOwner_oid(sdepForm->objid, newrole, true);
        break;

      case NamespaceRelationId:
        AlterSchemaOwner_oid(sdepForm->objid, newrole);
        break;

      case RelationRelationId:

           
                                                                   
                                                             
                                      
           
        ATExecChangeOwner(sdepForm->objid, newrole, true, AccessExclusiveLock);
        break;

      case DefaultAclRelationId:

           
                                                               
                                      
           
        break;

      case UserMappingRelationId:
                   
        break;

      case ForeignServerRelationId:
        AlterForeignServerOwner_oid(sdepForm->objid, newrole);
        break;

      case ForeignDataWrapperRelationId:
        AlterForeignDataWrapperOwner_oid(sdepForm->objid, newrole);
        break;

      case EventTriggerRelationId:
        AlterEventTriggerOwner_oid(sdepForm->objid, newrole);
        break;

      case PublicationRelationId:
        AlterPublicationOwner_oid(sdepForm->objid, newrole);
        break;

      case SubscriptionRelationId:
        AlterSubscriptionOwner_oid(sdepForm->objid, newrole);
        break;

                                       
      case CollationRelationId:
      case ConversionRelationId:
      case OperatorRelationId:
      case ProcedureRelationId:
      case LanguageRelationId:
      case LargeObjectRelationId:
      case OperatorFamilyRelationId:
      case OperatorClassRelationId:
      case ExtensionRelationId:
      case StatisticExtRelationId:
      case TableSpaceRelationId:
      case DatabaseRelationId:
      case TSConfigRelationId:
      case TSDictionaryRelationId:
      {
        Oid classId = sdepForm->classid;
        Relation catalog;

        if (classId == LargeObjectRelationId)
        {
          classId = LargeObjectMetadataRelationId;
        }

        catalog = table_open(classId, RowExclusiveLock);

        AlterObjectOwner_internal(catalog, sdepForm->objid, newrole);

        table_close(catalog, NoLock);
      }
      break;

      default:
        elog(ERROR, "unexpected classid %u", sdepForm->classid);
        break;
      }

                    
      MemoryContextSwitchTo(oldcxt);
      MemoryContextDelete(cxt);

                                                            
      CommandCounterIncrement();
    }

    systable_endscan(scan);
  }

  table_close(sdepRel, RowExclusiveLock);
}
