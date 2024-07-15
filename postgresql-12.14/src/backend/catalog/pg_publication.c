                                                                            
   
                    
                                   
   
                                                                         
                                                                        
   
                  
                     
   
                                                                            
   

#include "postgres.h"

#include "funcapi.h"
#include "miscadmin.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/tableam.h"
#include "access/xact.h"

#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/objectaddress.h"
#include "catalog/pg_type.h"
#include "catalog/pg_publication.h"
#include "catalog/pg_publication_rel.h"

#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

   
                                                                        
                 
   
static void
check_publication_add_relation(Relation targetrel)
{
                                                       
  if (RelationGetForm(targetrel)->relkind == RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("\"%s\" is a partitioned table", RelationGetRelationName(targetrel)), errdetail("Adding partitioned tables to publications is not supported."), errhint("You can add the table partitions individually.")));
  }

                     
  if (RelationGetForm(targetrel)->relkind != RELKIND_RELATION)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("\"%s\" is not a table", RelationGetRelationName(targetrel)), errdetail("Only tables can be added to publications.")));
  }

                             
  if (IsCatalogRelation(targetrel))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("\"%s\" is a system table", RelationGetRelationName(targetrel)), errdetail("System tables cannot be added to publications.")));
  }

                                                                  
  if (!RelationNeedsWAL(targetrel))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("table \"%s\" cannot be replicated", RelationGetRelationName(targetrel)), errdetail("Temporary and unlogged relations cannot be replicated.")));
  }
}

   
                                                                  
                   
   
                                                                          
                                   
   
                                                                        
                                                                              
                                                                         
                                                                      
                                                                    
                                                                     
                                                                         
                                                                        
                                                                            
                                                                           
                                             
   
static bool
is_publishable_class(Oid relid, Form_pg_class reltuple)
{
  return reltuple->relkind == RELKIND_RELATION && !IsCatalogRelationOid(relid) && reltuple->relpersistence == RELPERSISTENCE_PERMANENT && relid >= FirstNormalObjectId;
}

   
                                               
   
bool
is_publishable_relation(Relation rel)
{
  return is_publishable_class(RelationGetRelid(rel), rel->rd_rel);
}

   
                                     
   
                                                                               
                                                                      
                               
   
Datum
pg_relation_is_publishable(PG_FUNCTION_ARGS)
{
  Oid relid = PG_GETARG_OID(0);
  HeapTuple tuple;
  bool result;

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tuple))
  {
    PG_RETURN_NULL();
  }
  result = is_publishable_class(relid, (Form_pg_class)GETSTRUCT(tuple));
  ReleaseSysCache(tuple);
  PG_RETURN_BOOL(result);
}

   
                                              
   
ObjectAddress
publication_add_relation(Oid pubid, Relation targetrel, bool if_not_exists)
{
  Relation rel;
  HeapTuple tup;
  Datum values[Natts_pg_publication_rel];
  bool nulls[Natts_pg_publication_rel];
  Oid relid = RelationGetRelid(targetrel);
  Oid prrelid;
  Publication *pub = GetPublication(pubid);
  ObjectAddress myself, referenced;

  rel = table_open(PublicationRelRelationId, RowExclusiveLock);

     
                                                                  
                                                                         
                                                                 
     
  if (SearchSysCacheExists2(PUBLICATIONRELMAP, ObjectIdGetDatum(relid), ObjectIdGetDatum(pubid)))
  {
    table_close(rel, RowExclusiveLock);

    if (if_not_exists)
    {
      return InvalidObjectAddress;
    }

    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("relation \"%s\" is already member of publication \"%s\"", RelationGetRelationName(targetrel), pub->name)));
  }

  check_publication_add_relation(targetrel);

                     
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));

  prrelid = GetNewOidWithIndex(rel, PublicationRelObjectIndexId, Anum_pg_publication_rel_oid);
  values[Anum_pg_publication_rel_oid - 1] = ObjectIdGetDatum(prrelid);
  values[Anum_pg_publication_rel_prpubid - 1] = ObjectIdGetDatum(pubid);
  values[Anum_pg_publication_rel_prrelid - 1] = ObjectIdGetDatum(relid);

  tup = heap_form_tuple(RelationGetDescr(rel), values, nulls);

                                  
  CatalogTupleInsert(rel, tup);
  heap_freetuple(tup);

  ObjectAddressSet(myself, PublicationRelRelationId, prrelid);

                                         
  ObjectAddressSet(referenced, PublicationRelationId, pubid);
  recordDependencyOn(&myself, &referenced, DEPENDENCY_AUTO);

                                      
  ObjectAddressSet(referenced, RelationRelationId, relid);
  recordDependencyOn(&myself, &referenced, DEPENDENCY_AUTO);

                        
  table_close(rel, RowExclusiveLock);

                                                                
  CacheInvalidateRelcache(targetrel);

  return myself;
}

   
                                                     
   
List *
GetRelationPublications(Oid relid)
{
  List *result = NIL;
  CatCList *pubrellist;
  int i;

                                                           
  pubrellist = SearchSysCacheList1(PUBLICATIONRELMAP, ObjectIdGetDatum(relid));
  for (i = 0; i < pubrellist->n_members; i++)
  {
    HeapTuple tup = &pubrellist->members[i]->tuple;
    Oid pubid = ((Form_pg_publication_rel)GETSTRUCT(tup))->prpubid;

    result = lappend_oid(result, pubid);
  }

  ReleaseSysCacheList(pubrellist);

  return result;
}

   
                                                 
   
                                                                        
                                                  
   
List *
GetPublicationRelations(Oid pubid)
{
  List *result;
  Relation pubrelsrel;
  ScanKeyData scankey;
  SysScanDesc scan;
  HeapTuple tup;

                                                           
  pubrelsrel = table_open(PublicationRelRelationId, AccessShareLock);

  ScanKeyInit(&scankey, Anum_pg_publication_rel_prpubid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(pubid));

  scan = systable_beginscan(pubrelsrel, PublicationRelPrrelidPrpubidIndexId, true, NULL, 1, &scankey);

  result = NIL;
  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    Form_pg_publication_rel pubrel;

    pubrel = (Form_pg_publication_rel)GETSTRUCT(tup);

    result = lappend_oid(result, pubrel->prrelid);
  }

  systable_endscan(scan);
  table_close(pubrelsrel, AccessShareLock);

  return result;
}

   
                                                                            
   
List *
GetAllTablesPublications(void)
{
  List *result;
  Relation rel;
  ScanKeyData scankey;
  SysScanDesc scan;
  HeapTuple tup;

                                                                
  rel = table_open(PublicationRelationId, AccessShareLock);

  ScanKeyInit(&scankey, Anum_pg_publication_puballtables, BTEqualStrategyNumber, F_BOOLEQ, BoolGetDatum(true));

  scan = systable_beginscan(rel, InvalidOid, false, NULL, 1, &scankey);

  result = NIL;
  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    Oid oid = ((Form_pg_publication)GETSTRUCT(tup))->oid;

    result = lappend_oid(result, oid);
  }

  systable_endscan(scan);
  table_close(rel, AccessShareLock);

  return result;
}

   
                                                                         
   
List *
GetAllTablesPublicationRelations(void)
{
  Relation classRel;
  ScanKeyData key[1];
  TableScanDesc scan;
  HeapTuple tuple;
  List *result = NIL;

  classRel = table_open(RelationRelationId, AccessShareLock);

  ScanKeyInit(&key[0], Anum_pg_class_relkind, BTEqualStrategyNumber, F_CHAREQ, CharGetDatum(RELKIND_RELATION));

  scan = table_beginscan_catalog(classRel, 1, key);

  while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    Form_pg_class relForm = (Form_pg_class)GETSTRUCT(tuple);
    Oid relid = relForm->oid;

    if (is_publishable_class(relid, relForm))
    {
      result = lappend_oid(result, relid);
    }
  }

  table_endscan(scan);
  table_close(classRel, AccessShareLock);

  return result;
}

   
                             
   
                                                           
   
Publication *
GetPublication(Oid pubid)
{
  HeapTuple tup;
  Publication *pub;
  Form_pg_publication pubform;

  tup = SearchSysCache1(PUBLICATIONOID, ObjectIdGetDatum(pubid));

  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for publication %u", pubid);
  }

  pubform = (Form_pg_publication)GETSTRUCT(tup);

  pub = (Publication *)palloc(sizeof(Publication));
  pub->oid = pubid;
  pub->name = pstrdup(NameStr(pubform->pubname));
  pub->alltables = pubform->puballtables;
  pub->pubactions.pubinsert = pubform->pubinsert;
  pub->pubactions.pubupdate = pubform->pubupdate;
  pub->pubactions.pubdelete = pubform->pubdelete;
  pub->pubactions.pubtruncate = pubform->pubtruncate;

  ReleaseSysCache(tup);

  return pub;
}

   
                               
   
Publication *
GetPublicationByName(const char *pubname, bool missing_ok)
{
  Oid oid;

  oid = GetSysCacheOid1(PUBLICATIONNAME, Anum_pg_publication_oid, CStringGetDatum(pubname));
  if (!OidIsValid(oid))
  {
    if (missing_ok)
    {
      return NULL;
    }

    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("publication \"%s\" does not exist", pubname)));
  }

  return GetPublication(oid);
}

   
                                                                   
   
                                                                            
                      
   
Oid
get_publication_oid(const char *pubname, bool missing_ok)
{
  Oid oid;

  oid = GetSysCacheOid1(PUBLICATIONNAME, Anum_pg_publication_oid, CStringGetDatum(pubname));
  if (!OidIsValid(oid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("publication \"%s\" does not exist", pubname)));
  }
  return oid;
}

   
                                                                    
   
                                                                            
                
   
char *
get_publication_name(Oid pubid, bool missing_ok)
{
  HeapTuple tup;
  char *pubname;
  Form_pg_publication pubform;

  tup = SearchSysCache1(PUBLICATIONOID, ObjectIdGetDatum(pubid));

  if (!HeapTupleIsValid(tup))
  {
    if (!missing_ok)
    {
      elog(ERROR, "cache lookup failed for publication %u", pubid);
    }
    return NULL;
  }

  pubform = (Form_pg_publication)GETSTRUCT(tup);
  pubname = pstrdup(NameStr(pubform->pubname));

  ReleaseSysCache(tup);

  return pubname;
}

   
                                            
   
Datum
pg_get_publication_tables(PG_FUNCTION_ARGS)
{
  FuncCallContext *funcctx;
  char *pubname = text_to_cstring(PG_GETARG_TEXT_PP(0));
  Publication *publication;
  List *tables;
  ListCell **lcp;

                                                         
  if (SRF_IS_FIRSTCALL())
  {
    MemoryContext oldcontext;

                                                              
    funcctx = SRF_FIRSTCALL_INIT();

                                                                          
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

    publication = GetPublicationByName(pubname, false);
    if (publication->alltables)
    {
      tables = GetAllTablesPublicationRelations();
    }
    else
    {
      tables = GetPublicationRelations(publication->oid);
    }
    lcp = (ListCell **)palloc(sizeof(ListCell *));
    *lcp = list_head(tables);
    funcctx->user_fctx = (void *)lcp;

    MemoryContextSwitchTo(oldcontext);
  }

                                                
  funcctx = SRF_PERCALL_SETUP();
  lcp = (ListCell **)funcctx->user_fctx;

  while (*lcp != NULL)
  {
    Oid relid = lfirst_oid(*lcp);

    *lcp = lnext(*lcp);
    SRF_RETURN_NEXT(funcctx, ObjectIdGetDatum(relid));
  }

  SRF_RETURN_DONE(funcctx);
}
