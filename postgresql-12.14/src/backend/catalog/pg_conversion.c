                                                                            
   
                   
                                                                    
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_conversion.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_proc.h"
#include "mb/pg_wchar.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"
#include "utils/syscache.h"

   
                    
   
                                     
   
ObjectAddress
ConversionCreate(const char *conname, Oid connamespace, Oid conowner, int32 conforencoding, int32 contoencoding, Oid conproc, bool def)
{
  int i;
  Relation rel;
  TupleDesc tupDesc;
  HeapTuple tup;
  Oid oid;
  bool nulls[Natts_pg_conversion];
  Datum values[Natts_pg_conversion];
  NameData cname;
  ObjectAddress myself, referenced;

                     
  if (!conname)
  {
    elog(ERROR, "no conversion name supplied");
  }

                                                              
  if (SearchSysCacheExists2(CONNAMENSP, PointerGetDatum(conname), ObjectIdGetDatum(connamespace)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("conversion \"%s\" already exists", conname)));
  }

  if (def)
  {
       
                                                                          
                               
       
    if (FindDefaultConversion(connamespace, conforencoding, contoencoding))
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("default conversion for %s to %s already exists", pg_encoding_to_char(conforencoding), pg_encoding_to_char(contoencoding))));
    }
  }

                          
  rel = table_open(ConversionRelationId, RowExclusiveLock);
  tupDesc = rel->rd_att;

                                   
  for (i = 0; i < Natts_pg_conversion; i++)
  {
    nulls[i] = false;
    values[i] = (Datum)NULL;
  }

                    
  namestrcpy(&cname, conname);
  oid = GetNewOidWithIndex(rel, ConversionOidIndexId, Anum_pg_conversion_oid);
  values[Anum_pg_conversion_oid - 1] = ObjectIdGetDatum(oid);
  values[Anum_pg_conversion_conname - 1] = NameGetDatum(&cname);
  values[Anum_pg_conversion_connamespace - 1] = ObjectIdGetDatum(connamespace);
  values[Anum_pg_conversion_conowner - 1] = ObjectIdGetDatum(conowner);
  values[Anum_pg_conversion_conforencoding - 1] = Int32GetDatum(conforencoding);
  values[Anum_pg_conversion_contoencoding - 1] = Int32GetDatum(contoencoding);
  values[Anum_pg_conversion_conproc - 1] = ObjectIdGetDatum(conproc);
  values[Anum_pg_conversion_condefault - 1] = BoolGetDatum(def);

  tup = heap_form_tuple(tupDesc, values, nulls);

                          
  CatalogTupleInsert(rel, tup);

  myself.classId = ConversionRelationId;
  myself.objectId = oid;
  myself.objectSubId = 0;

                                                 
  referenced.classId = ProcedureRelationId;
  referenced.objectId = conproc;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                      
  referenced.classId = NamespaceRelationId;
  referenced.objectId = connamespace;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                  
  recordDependencyOnOwner(ConversionRelationId, oid, conowner);

                               
  recordDependencyOnCurrentExtension(&myself, false);

                                             
  InvokeObjectPostCreateHook(ConversionRelationId, oid, 0);

  heap_freetuple(tup);
  table_close(rel, RowExclusiveLock);

  return myself;
}

   
                        
   
                                                                     
                                      
   
void
RemoveConversionById(Oid conversionOid)
{
  Relation rel;
  HeapTuple tuple;
  TableScanDesc scan;
  ScanKeyData scanKeyData;

  ScanKeyInit(&scanKeyData, Anum_pg_conversion_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(conversionOid));

                          
  rel = table_open(ConversionRelationId, RowExclusiveLock);

  scan = table_beginscan_catalog(rel, 1, &scanKeyData);

                                   
  if (HeapTupleIsValid(tuple = heap_getnext(scan, ForwardScanDirection)))
  {
    CatalogTupleDelete(rel, &tuple->t_self);
  }
  else
  {
    elog(ERROR, "could not find tuple for conversion %u", conversionOid);
  }
  table_endscan(scan);
  table_close(rel, RowExclusiveLock);
}

   
                         
   
                                                                         
                    
   
                                                                           
                                                         
   
Oid
FindDefaultConversion(Oid name_space, int32 for_encoding, int32 to_encoding)
{
  CatCList *catlist;
  HeapTuple tuple;
  Form_pg_conversion body;
  Oid proc = InvalidOid;
  int i;

  catlist = SearchSysCacheList3(CONDEFAULT, ObjectIdGetDatum(name_space), Int32GetDatum(for_encoding), Int32GetDatum(to_encoding));

  for (i = 0; i < catlist->n_members; i++)
  {
    tuple = &catlist->members[i]->tuple;
    body = (Form_pg_conversion)GETSTRUCT(tuple);
    if (body->condefault)
    {
      proc = body->conproc;
      break;
    }
  }
  ReleaseSysCacheList(catlist);
  return proc;
}
