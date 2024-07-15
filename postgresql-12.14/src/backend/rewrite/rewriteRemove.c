                                                                            
   
                   
                                         
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_rewrite.h"
#include "miscadmin.h"
#include "rewrite/rewriteRemove.h"
#include "utils/acl.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

   
                          
   
void
RemoveRewriteRuleById(Oid ruleOid)
{
  Relation RewriteRelation;
  ScanKeyData skey[1];
  SysScanDesc rcscan;
  Relation event_relation;
  HeapTuple tuple;
  Oid eventRelationOid;

     
                                   
     
  RewriteRelation = table_open(RewriteRelationId, RowExclusiveLock);

     
                                         
     
  ScanKeyInit(&skey[0], Anum_pg_rewrite_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(ruleOid));

  rcscan = systable_beginscan(RewriteRelation, RewriteOidIndexId, true, NULL, 1, skey);

  tuple = systable_getnext(rcscan);

  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "could not find tuple for rule %u", ruleOid);
  }

     
                                                                          
                                                                          
                                             
     
  eventRelationOid = ((Form_pg_rewrite)GETSTRUCT(tuple))->ev_class;
  event_relation = table_open(eventRelationOid, AccessExclusiveLock);

     
                                                  
     
  CatalogTupleDelete(RewriteRelation, &tuple->t_self);

  systable_endscan(rcscan);

  table_close(RewriteRelation, RowExclusiveLock);

     
                                                                        
                                                    
     
  CacheInvalidateRelcache(event_relation);

                                               
  table_close(event_relation, NoLock);
}
