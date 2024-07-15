                                                                            
   
                      
                                                                              
                                                                            
                              
   
                                                                               
                                                                       
                                                                               
                                                                         
                              
   
                                                                         
                                                                        
   
   
                  
                                               
   
                                                                            
   
#include "postgres.h"

#include "access/table.h"
#include "utils/queryenvironment.h"
#include "utils/rel.h"

   
                                         
   
struct QueryEnvironment
{
  List *namedRelList;
};

QueryEnvironment *
create_queryEnv()
{
  return (QueryEnvironment *)palloc0(sizeof(QueryEnvironment));
}

EphemeralNamedRelationMetadata
get_visible_ENR_metadata(QueryEnvironment *queryEnv, const char *refname)
{
  EphemeralNamedRelation enr;

  Assert(refname != NULL);

  if (queryEnv == NULL)
  {
    return NULL;
  }

  enr = get_ENR(queryEnv, refname);

  if (enr)
  {
    return &(enr->md);
  }

  return NULL;
}

   
                                                               
   
                                                                               
                 
   
void
register_ENR(QueryEnvironment *queryEnv, EphemeralNamedRelation enr)
{
  Assert(enr != NULL);
  Assert(get_ENR(queryEnv, enr->md.name) == NULL);

  queryEnv->namedRelList = lappend(queryEnv->namedRelList, enr);
}

   
                                                                             
                                                                       
   
void
unregister_ENR(QueryEnvironment *queryEnv, const char *name)
{
  EphemeralNamedRelation match;

  match = get_ENR(queryEnv, name);
  if (match)
  {
    queryEnv->namedRelList = list_delete(queryEnv->namedRelList, match);
  }
}

   
                                                                             
                                                  
   
EphemeralNamedRelation
get_ENR(QueryEnvironment *queryEnv, const char *name)
{
  ListCell *lc;

  Assert(name != NULL);

  if (queryEnv == NULL)
  {
    return NULL;
  }

  foreach (lc, queryEnv->namedRelList)
  {
    EphemeralNamedRelation enr = (EphemeralNamedRelation)lfirst(lc);

    if (strcmp(enr->md.name, name) == 0)
    {
      return enr;
    }
  }

  return NULL;
}

   
                                                                               
           
   
                                                                            
                                                                             
                                                            
   
TupleDesc
ENRMetadataGetTupDesc(EphemeralNamedRelationMetadata enrmd)
{
  TupleDesc tupdesc;

                                                          
  Assert((enrmd->reliddesc == InvalidOid) != (enrmd->tupdesc == NULL));

  if (enrmd->tupdesc != NULL)
  {
    tupdesc = enrmd->tupdesc;
  }
  else
  {
    Relation relation;

    relation = table_open(enrmd->reliddesc, NoLock);
    tupdesc = relation->rd_att;
    table_close(relation, NoLock);
  }

  return tupdesc;
}
