                                                                            
   
                  
                                                                   
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_namespace.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/syscache.h"

                    
                   
   
                                                                  
   
                                                                      
                                                                     
                                                                        
                                                                       
                                                                        
                                                
                   
   
Oid
NamespaceCreate(const char *nspName, Oid ownerId, bool isTemp)
{
  Relation nspdesc;
  HeapTuple tup;
  Oid nspoid;
  bool nulls[Natts_pg_namespace];
  Datum values[Natts_pg_namespace];
  NameData nname;
  TupleDesc tupDesc;
  ObjectAddress myself;
  int i;
  Acl *nspacl;

                     
  if (!nspName)
  {
    elog(ERROR, "no namespace name supplied");
  }

                                                             
  if (SearchSysCacheExists1(NAMESPACENAME, PointerGetDatum(nspName)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_SCHEMA), errmsg("schema \"%s\" already exists", nspName)));
  }

  if (!isTemp)
  {
    nspacl = get_user_default_acl(OBJECT_SCHEMA, ownerId, InvalidOid);
  }
  else
  {
    nspacl = NULL;
  }

  nspdesc = table_open(NamespaceRelationId, RowExclusiveLock);
  tupDesc = nspdesc->rd_att;

                                   
  for (i = 0; i < Natts_pg_namespace; i++)
  {
    nulls[i] = false;
    values[i] = (Datum)NULL;
  }

  nspoid = GetNewOidWithIndex(nspdesc, NamespaceOidIndexId, Anum_pg_namespace_oid);
  values[Anum_pg_namespace_oid - 1] = ObjectIdGetDatum(nspoid);
  namestrcpy(&nname, nspName);
  values[Anum_pg_namespace_nspname - 1] = NameGetDatum(&nname);
  values[Anum_pg_namespace_nspowner - 1] = ObjectIdGetDatum(ownerId);
  if (nspacl != NULL)
  {
    values[Anum_pg_namespace_nspacl - 1] = PointerGetDatum(nspacl);
  }
  else
  {
    nulls[Anum_pg_namespace_nspacl - 1] = true;
  }

  tup = heap_form_tuple(tupDesc, values, nulls);

  CatalogTupleInsert(nspdesc, tup);
  Assert(OidIsValid(nspoid));

  table_close(nspdesc, RowExclusiveLock);

                           
  myself.classId = NamespaceRelationId;
  myself.objectId = nspoid;
  myself.objectSubId = 0;

                           
  recordDependencyOnOwner(NamespaceRelationId, nspoid, ownerId);

                                                     
  recordDependencyOnNewAcl(NamespaceRelationId, nspoid, 0, ownerId, nspacl);

                                                                  
  if (!isTemp)
  {
    recordDependencyOnCurrentExtension(&myself, false);
  }

                                         
  InvokeObjectPostCreateHook(NamespaceRelationId, nspoid, 0);

  return nspoid;
}
