                                                                            
   
             
                                                                 
                                 
   
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   

#include "postgres.h"

#include <fcntl.h>
#include <unistd.h>

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/transam.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_auth_members.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_database.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_pltemplate.h"
#include "catalog/pg_db_role_setting.h"
#include "catalog/pg_replication_origin.h"
#include "catalog/pg_shdepend.h"
#include "catalog/pg_shdescription.h"
#include "catalog/pg_shseclabel.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "catalog/toasting.h"
#include "miscadmin.h"
#include "storage/fd.h"
#include "utils/fmgroids.h"
#include "utils/fmgrprotos.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

   
                    
                                                                       
                                                                        
   
                                                                      
                                                                     
                                                                    
                                                                 
                                                                
                               
   
                                                         
                               
   
bool
IsSystemRelation(Relation relation)
{
  return IsSystemClass(RelationGetRelid(relation), relation->rd_rel);
}

   
                 
                                                           
                                                              
                              
   
bool
IsSystemClass(Oid relid, Form_pg_class reltuple)
{
                                                                
  return (IsCatalogRelationOid(relid) || IsToastClass(reltuple));
}

   
                     
                                               
   
                                                                          
                                                                      
                                                             
   
                                                         
                               
   
bool
IsCatalogRelation(Relation relation)
{
  return IsCatalogRelationOid(RelationGetRelid(relation));
}

   
                        
                                                                      
   
                                                                          
                                                                      
                                                             
   
                                                         
                               
   
bool
IsCatalogRelationOid(Oid relid)
{
     
                                                                             
                                                                        
                                                                          
     
                                                                           
                                                                           
                                                                         
                                                
     
                                                                           
                                 
     
  return (relid < (Oid)FirstBootstrapObjectId);
}

   
                   
                                                              
   
                                           
   
bool
IsToastRelation(Relation relation)
{
     
                                                                          
                                                                            
                                                                      
                                                                         
                                                                            
                                                                           
     
  return IsToastNamespace(RelationGetNamespace(relation));
}

   
                
                                                           
                                                              
                              
   
bool
IsToastClass(Form_pg_class reltuple)
{
  Oid relnamespace = reltuple->relnamespace;

  return IsToastNamespace(relnamespace);
}

   
                      
                                      
   
                                           
   
                                                                     
                                              
   
bool
IsCatalogNamespace(Oid namespaceId)
{
  return namespaceId == PG_CATALOG_NAMESPACE;
}

   
                    
                                                                          
   
                                           
   
                                                                               
                                                                             
                                                                     
                                                                              
                                                       
   
bool
IsToastNamespace(Oid namespaceId)
{
  return (namespaceId == PG_TOAST_NAMESPACE) || isTempToastNamespace(namespaceId);
}

   
                  
                                              
   
                                                                
                                                            
                                                              
               
   
bool
IsReservedName(const char *name)
{
                             
  return (name[0] == 'p' && name[1] == 'g' && name[2] == '_');
}

   
                    
                                                                       
                                              
   
                                                                             
                                                                          
                                                                             
                                                                               
                                                                           
                                                                         
                                                                            
                                                                              
                                                                            
                                      
   
bool
IsSharedRelation(Oid relationId)
{
                                                                    
  if (relationId == AuthIdRelationId || relationId == AuthMemRelationId || relationId == DatabaseRelationId || relationId == PLTemplateRelationId || relationId == SharedDescriptionRelationId || relationId == SharedDependRelationId || relationId == SharedSecLabelRelationId || relationId == TableSpaceRelationId || relationId == DbRoleSettingRelationId || relationId == ReplicationOriginRelationId || relationId == SubscriptionRelationId)
  {
    return true;
  }
                                                
  if (relationId == AuthIdRolnameIndexId || relationId == AuthIdOidIndexId || relationId == AuthMemRoleMemIndexId || relationId == AuthMemMemRoleIndexId || relationId == DatabaseNameIndexId || relationId == DatabaseOidIndexId || relationId == PLTemplateNameIndexId || relationId == SharedDescriptionObjIndexId || relationId == SharedDependDependerIndexId || relationId == SharedDependReferenceIndexId || relationId == SharedSecLabelObjectIndexId || relationId == TablespaceOidIndexId || relationId == TablespaceNameIndexId || relationId == DbRoleSettingDatidRolidIndexId || relationId == ReplicationOriginIdentIndex || relationId == ReplicationOriginNameIndex || relationId == SubscriptionObjectIndexId || relationId == SubscriptionNameIndexId)
  {
    return true;
  }
                                                                       
  if (relationId == PgAuthidToastTable || relationId == PgAuthidToastIndex || relationId == PgDatabaseToastTable || relationId == PgDatabaseToastIndex || relationId == PgDbRoleSettingToastTable || relationId == PgDbRoleSettingToastIndex || relationId == PgPlTemplateToastTable || relationId == PgPlTemplateToastIndex || relationId == PgReplicationOriginToastTable || relationId == PgReplicationOriginToastIndex || relationId == PgShdescriptionToastTable || relationId == PgShdescriptionToastIndex || relationId == PgShseclabelToastTable || relationId == PgShseclabelToastIndex || relationId == PgSubscriptionToastTable || relationId == PgSubscriptionToastIndex || relationId == PgTablespaceToastTable || relationId == PgTablespaceToastIndex)
  {
    return true;
  }
  return false;
}

   
                      
                                                                  
   
                                                                        
                                                                       
                                                                          
                                                                         
                                                                          
                                                                             
                                                                              
                                                                           
                                                                        
                                                                              
                                                                            
   
                                                                               
                                                                              
                                                                          
                    
   
                                                                            
                                                                             
                                                                      
                                                                        
                                                                            
                                                              
   
                                                     
   
Oid
GetNewOidWithIndex(Relation relation, Oid indexId, AttrNumber oidcolumn)
{
  Oid newOid;
  SysScanDesc scan;
  ScanKeyData key;
  bool collides;

                                           
  Assert(IsSystemRelation(relation));

                                                           
  if (IsBootstrapProcessingMode())
  {
    return GetNewObjectId();
  }

     
                                                                   
                                                                          
                                                                             
                                                                          
     
  Assert(!IsBinaryUpgrade || RelationGetRelid(relation) != TypeRelationId);

                                                            
  do
  {
    CHECK_FOR_INTERRUPTS();

    newOid = GetNewObjectId();

    ScanKeyInit(&key, oidcolumn, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(newOid));

                                                 
    scan = systable_beginscan(relation, indexId, true, SnapshotAny, 1, &key);

    collides = HeapTupleIsValid(systable_getnext(scan));

    systable_endscan(scan);
  } while (collides);

  return newOid;
}

   
                     
                                                                
                                      
   
                                                                        
                                                                            
                                                                            
                                                                      
   
                                                                          
                                                        
   
                                                                       
                                                                   
   
Oid
GetNewRelFileNode(Oid reltablespace, Relation pg_class, char relpersistence)
{
  RelFileNodeBackend rnode;
  char *rpath;
  bool collides;
  BackendId backend;

     
                                                                         
                                                                   
                                                
     
  Assert(!IsBinaryUpgrade);

  switch (relpersistence)
  {
  case RELPERSISTENCE_TEMP:
    backend = BackendIdForTempRelations();
    break;
  case RELPERSISTENCE_UNLOGGED:
  case RELPERSISTENCE_PERMANENT:
    backend = InvalidBackendId;
    break;
  default:
    elog(ERROR, "invalid relpersistence: %c", relpersistence);
    return InvalidOid;                       
  }

                                                        
  rnode.node.spcNode = reltablespace ? reltablespace : MyDatabaseTableSpace;
  rnode.node.dbNode = (rnode.node.spcNode == GLOBALTABLESPACE_OID) ? InvalidOid : MyDatabaseId;

     
                                                                          
                                                                           
                            
     
  rnode.backend = backend;

  do
  {
    CHECK_FOR_INTERRUPTS();

                          
    if (pg_class)
    {
      rnode.node.relNode = GetNewOidWithIndex(pg_class, ClassOidIndexId, Anum_pg_class_oid);
    }
    else
    {
      rnode.node.relNode = GetNewObjectId();
    }

                                              
    rpath = relpath(rnode, MAIN_FORKNUM);

    if (access(rpath, F_OK) == 0)
    {
                              
      collides = true;
    }
    else
    {
         
                                                                       
                                                                       
                                                                         
                                                                       
                                                  
         
      collides = false;
    }

    pfree(rpath);
  } while (collides);

  return rnode.node.relNode;
}

   
                                                                         
                                                                               
                            
   
                                                                     
   
Datum
pg_nextoid(PG_FUNCTION_ARGS)
{
  Oid reloid = PG_GETARG_OID(0);
  Name attname = PG_GETARG_NAME(1);
  Oid idxoid = PG_GETARG_OID(2);
  Relation rel;
  Relation idx;
  HeapTuple atttuple;
  Form_pg_attribute attform;
  AttrNumber attno;
  Oid newoid;

     
                                                                            
                                                                           
                                                                      
               
     
  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to call pg_nextoid()")));
  }

  rel = table_open(reloid, RowExclusiveLock);
  idx = index_open(idxoid, RowExclusiveLock);

  if (!IsSystemRelation(rel))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("pg_nextoid() can only be used on system catalogs")));
  }

  if (idx->rd_index->indrelid != RelationGetRelid(rel))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("index \"%s\" does not belong to table \"%s\"", RelationGetRelationName(idx), RelationGetRelationName(rel))));
  }

  atttuple = SearchSysCacheAttName(reloid, NameStr(*attname));
  if (!HeapTupleIsValid(atttuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", NameStr(*attname), RelationGetRelationName(rel))));
  }

  attform = ((Form_pg_attribute)GETSTRUCT(atttuple));
  attno = attform->attnum;

  if (attform->atttypid != OIDOID)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("column \"%s\" is not of type oid", NameStr(*attname))));
  }

  if (IndexRelationGetNumberOfKeyAttributes(idx) != 1 || idx->rd_index->indkey.values[0] != attno)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("index \"%s\" is not the index for column \"%s\"", RelationGetRelationName(idx), NameStr(*attname))));
  }

  newoid = GetNewOidWithIndex(rel, idxoid, attno);

  ReleaseSysCache(atttuple);
  table_close(rel, RowExclusiveLock);
  index_close(idx, RowExclusiveLock);

  return newoid;
}
