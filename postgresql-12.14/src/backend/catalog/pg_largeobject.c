                                                                            
   
                    
                                                                     
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_largeobject.h"
#include "catalog/pg_largeobject_metadata.h"
#include "miscadmin.h"
#include "utils/acl.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"

   
                                                         
   
                                                           
                                                                      
                                     
   
Oid
LargeObjectCreate(Oid loid)
{
  Relation pg_lo_meta;
  HeapTuple ntup;
  Oid loid_new;
  Datum values[Natts_pg_largeobject_metadata];
  bool nulls[Natts_pg_largeobject_metadata];

  pg_lo_meta = table_open(LargeObjectMetadataRelationId, RowExclusiveLock);

     
                                        
     
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));

  if (OidIsValid(loid))
  {
    loid_new = loid;
  }
  else
  {
    loid_new = GetNewOidWithIndex(pg_lo_meta, LargeObjectMetadataOidIndexId, Anum_pg_largeobject_metadata_oid);
  }

  values[Anum_pg_largeobject_metadata_oid - 1] = ObjectIdGetDatum(loid_new);
  values[Anum_pg_largeobject_metadata_lomowner - 1] = ObjectIdGetDatum(GetUserId());
  nulls[Anum_pg_largeobject_metadata_lomacl - 1] = true;

  ntup = heap_form_tuple(RelationGetDescr(pg_lo_meta), values, nulls);

  CatalogTupleInsert(pg_lo_meta, ntup);

  heap_freetuple(ntup);

  table_close(pg_lo_meta, RowExclusiveLock);

  return loid_new;
}

   
                                                                            
                                 
   
void
LargeObjectDrop(Oid loid)
{
  Relation pg_lo_meta;
  Relation pg_largeobject;
  ScanKeyData skey[1];
  SysScanDesc scan;
  HeapTuple tuple;

  pg_lo_meta = table_open(LargeObjectMetadataRelationId, RowExclusiveLock);

  pg_largeobject = table_open(LargeObjectRelationId, RowExclusiveLock);

     
                                                  
     
  ScanKeyInit(&skey[0], Anum_pg_largeobject_metadata_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(loid));

  scan = systable_beginscan(pg_lo_meta, LargeObjectMetadataOidIndexId, true, NULL, 1, skey);

  tuple = systable_getnext(scan);
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("large object %u does not exist", loid)));
  }

  CatalogTupleDelete(pg_lo_meta, &tuple->t_self);

  systable_endscan(scan);

     
                                                           
     
  ScanKeyInit(&skey[0], Anum_pg_largeobject_loid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(loid));

  scan = systable_beginscan(pg_largeobject, LargeObjectLOidPNIndexId, true, NULL, 1, skey);
  while (HeapTupleIsValid(tuple = systable_getnext(scan)))
  {
    CatalogTupleDelete(pg_largeobject, &tuple->t_self);
  }

  systable_endscan(scan);

  table_close(pg_largeobject, RowExclusiveLock);

  table_close(pg_lo_meta, RowExclusiveLock);
}

   
                     
   
                                                                        
                                
   
                                                                               
                                                                            
                                                                             
                                                                          
                                    
   
bool
LargeObjectExists(Oid loid)
{
  Relation pg_lo_meta;
  ScanKeyData skey[1];
  SysScanDesc sd;
  HeapTuple tuple;
  bool retval = false;

  ScanKeyInit(&skey[0], Anum_pg_largeobject_metadata_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(loid));

  pg_lo_meta = table_open(LargeObjectMetadataRelationId, AccessShareLock);

  sd = systable_beginscan(pg_lo_meta, LargeObjectMetadataOidIndexId, true, NULL, 1, skey);

  tuple = systable_getnext(sd);
  if (HeapTupleIsValid(tuple))
  {
    retval = true;
  }

  systable_endscan(sd);

  table_close(pg_lo_meta, AccessShareLock);

  return retval;
}
