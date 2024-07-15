                                                                            
   
           
                                              
   
                                                                         
                                                                        
   
   
                  
                                      
   
   
         
                                                                            
                                                                            
                                      
   
                                                                            
   

#include "postgres.h"

#include "access/relation.h"
#include "access/table.h"
#include "storage/lmgr.h"

                    
                                                       
   
                                                                   
                                                                   
                                                                       
              
                    
   
Relation
table_open(Oid relationId, LOCKMODE lockmode)
{
  Relation r;

  r = relation_open(relationId, lockmode);

  if (r->rd_rel->relkind == RELKIND_INDEX || r->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is an index", RelationGetRelationName(r))));
  }
  else if (r->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is a composite type", RelationGetRelationName(r))));
  }

  return r;
}

                    
                                                   
                       
   
                                                       
                    
   
Relation
table_openrv(const RangeVar *relation, LOCKMODE lockmode)
{
  Relation r;

  r = relation_openrv(relation, lockmode);

  if (r->rd_rel->relkind == RELKIND_INDEX || r->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is an index", RelationGetRelationName(r))));
  }
  else if (r->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is a composite type", RelationGetRelationName(r))));
  }

  return r;
}

                    
                                                            
                       
   
                                                                
                        
                    
   
Relation
table_openrv_extended(const RangeVar *relation, LOCKMODE lockmode, bool missing_ok)
{
  Relation r;

  r = relation_openrv_extended(relation, lockmode, missing_ok);

  if (r)
  {
    if (r->rd_rel->relkind == RELKIND_INDEX || r->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is an index", RelationGetRelationName(r))));
    }
    else if (r->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is a composite type", RelationGetRelationName(r))));
    }
  }

  return r;
}

                    
                                
   
                                                                     
   
                                                                         
                                                                  
                     
   
void
table_close(Relation relation, LOCKMODE lockmode)
{
  relation_close(relation, lockmode);
}
