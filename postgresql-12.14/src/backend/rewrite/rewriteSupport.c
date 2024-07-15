                                                                            
   
                    
   
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/pg_rewrite.h"
#include "rewrite/rewriteSupport.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

   
                                      
   
bool
IsDefinedRewriteRule(Oid owningRel, const char *ruleName)
{
  return SearchSysCacheExists2(RULERELNAME, ObjectIdGetDatum(owningRel), PointerGetDatum(ruleName));
}

   
                         
                                                                   
   
                                                                     
   
                                                                               
                                                                             
                                                                             
                                                                             
        
   
void
SetRelationRuleStatus(Oid relationId, bool relHasRules)
{
  Relation relationRelation;
  HeapTuple tuple;
  Form_pg_class classForm;

     
                                                                          
     
  relationRelation = table_open(RelationRelationId, RowExclusiveLock);
  tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relationId));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", relationId);
  }
  classForm = (Form_pg_class)GETSTRUCT(tuple);

  if (classForm->relhasrules != relHasRules)
  {
                       
    classForm->relhasrules = relHasRules;

    CatalogTupleUpdate(relationRelation, &tuple->t_self, tuple);
  }
  else
  {
                                                                    
    CacheInvalidateRelcacheByTuple(tuple);
  }

  heap_freetuple(tuple);
  table_close(relationRelation, RowExclusiveLock);
}

   
                  
   
                                                                      
                                 
   
Oid
get_rewrite_oid(Oid relid, const char *rulename, bool missing_ok)
{
  HeapTuple tuple;
  Form_pg_rewrite ruleform;
  Oid ruleoid;

                                                     
  tuple = SearchSysCache2(RULERELNAME, ObjectIdGetDatum(relid), PointerGetDatum(rulename));
  if (!HeapTupleIsValid(tuple))
  {
    if (missing_ok)
    {
      return InvalidOid;
    }
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("rule \"%s\" for relation \"%s\" does not exist", rulename, get_rel_name(relid))));
  }
  ruleform = (Form_pg_rewrite)GETSTRUCT(tuple);
  Assert(relid == ruleform->ev_class);
  ruleoid = ruleform->oid;
  ReleaseSysCache(tuple);
  return ruleoid;
}
