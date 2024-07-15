                                                                            
   
              
                                        
   
                                                                         
                                                                        
   
   
                  
                                          
   
         
                                                                              
                                                                              
                                                      
   
                                                                            
   

#include "postgres.h"

#include "access/relation.h"
#include "access/xact.h"
#include "catalog/namespace.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "storage/lmgr.h"
#include "utils/inval.h"
#include "utils/syscache.h"

                    
                                                      
   
                                                               
                                                                 
                                                                 
                       
   
                                                       
   
                                                                       
                                                                      
                    
   
Relation
relation_open(Oid relationId, LOCKMODE lockmode)
{
  Relation r;

  Assert(lockmode >= NoLock && lockmode < MAX_LOCKMODES);

                                                             
  if (lockmode != NoLock)
  {
    LockRelationOid(relationId, lockmode);
  }

                                              
  r = RelationIdGetRelation(relationId);

  if (!RelationIsValid(r))
  {
    elog(ERROR, "could not open relation with OID %u", relationId);
  }

     
                                                                        
                                                       
     
  Assert(lockmode != NoLock || IsBootstrapProcessingMode() || CheckRelationLockedByMe(r, AccessShareLock, true));

                                                          
  if (RelationUsesLocalBuffers(r))
  {
    MyXactFlags |= XACT_FLAGS_ACCESSEDTEMPNAMESPACE;
  }

  pgstat_initstats(r);

  return r;
}

                    
                                                          
   
                                                                 
                                    
                    
   
Relation
try_relation_open(Oid relationId, LOCKMODE lockmode)
{
  Relation r;

  Assert(lockmode >= NoLock && lockmode < MAX_LOCKMODES);

                          
  if (lockmode != NoLock)
  {
    LockRelationOid(relationId, lockmode);
  }

     
                                                                           
             
     
  if (!SearchSysCacheExists1(RELOID, ObjectIdGetDatum(relationId)))
  {
                              
    if (lockmode != NoLock)
    {
      UnlockRelationOid(relationId, lockmode);
    }

    return NULL;
  }

                                            
  r = RelationIdGetRelation(relationId);

  if (!RelationIsValid(r))
  {
    elog(ERROR, "could not open relation with OID %u", relationId);
  }

                                                                         
  Assert(lockmode != NoLock || CheckRelationLockedByMe(r, AccessShareLock, true));

                                                          
  if (RelationUsesLocalBuffers(r))
  {
    MyXactFlags |= XACT_FLAGS_ACCESSEDTEMPNAMESPACE;
  }

  pgstat_initstats(r);

  return r;
}

                    
                                                                
   
                                                                        
                    
   
Relation
relation_openrv(const RangeVar *relation, LOCKMODE lockmode)
{
  Oid relOid;

     
                                                                     
                                                                     
                                                                            
                                                                    
                                                                           
                                                                             
                                                                            
                                                                            
                         
     
  if (lockmode != NoLock)
  {
    AcceptInvalidationMessages();
  }

                                                                        
  relOid = RangeVarGetRelid(relation, lockmode, false);

                                     
  return relation_open(relOid, NoLock);
}

                    
                                                                         
   
                                                                        
                                                                       
                                                                        
                                      
                    
   
Relation
relation_openrv_extended(const RangeVar *relation, LOCKMODE lockmode, bool missing_ok)
{
  Oid relOid;

     
                                                                     
                                                   
     
  if (lockmode != NoLock)
  {
    AcceptInvalidationMessages();
  }

                                                                        
  relOid = RangeVarGetRelid(relation, lockmode, missing_ok);

                                
  if (!OidIsValid(relOid))
  {
    return NULL;
  }

                                     
  return relation_open(relOid, NoLock);
}

                    
                                        
   
                                                                     
   
                                                                         
                                                                  
                    
   
void
relation_close(Relation relation, LOCKMODE lockmode)
{
  LockRelId relid = relation->rd_lockInfo.lockRelId;

  Assert(lockmode >= NoLock && lockmode < MAX_LOCKMODES);

                                          
  RelationClose(relation);

  if (lockmode != NoLock)
  {
    UnlockRelationId(&relid, lockmode);
  }
}
