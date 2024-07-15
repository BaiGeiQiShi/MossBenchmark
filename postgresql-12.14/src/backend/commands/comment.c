                                                                            
   
             
   
                                            
   
                                                                
   
                  
                                    
   
                                                                            
   

#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/relation.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/objectaddress.h"
#include "catalog/pg_description.h"
#include "catalog/pg_shdescription.h"
#include "commands/comment.h"
#include "commands/dbcommands.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"

   
                    
   
                                                           
                                                                     
   
ObjectAddress
CommentObject(CommentStmt *stmt)
{
  Relation relation;
  ObjectAddress address = InvalidObjectAddress;

     
                                                                            
                                                                             
                                                                          
                                                                          
                                                                           
           
     
  if (stmt->objtype == OBJECT_DATABASE)
  {
    char *database = strVal((Value *)stmt->object);

    if (!OidIsValid(get_database_oid(database, true)))
    {
      ereport(WARNING, (errcode(ERRCODE_UNDEFINED_DATABASE), errmsg("database \"%s\" does not exist", database)));
      return address;
    }
  }

     
                                                                             
                                                                            
                                                                         
                                         
     
  address = get_object_address(stmt->objtype, stmt->object, &relation, ShareUpdateExclusiveLock, false);

                                               
  check_object_ownership(GetUserId(), stmt->objtype, address, stmt->object, relation);

                                                 
  switch (stmt->objtype)
  {
  case OBJECT_COLUMN:

       
                                                                     
                                                                      
                                                                      
                                                                 
                                                                     
                                                                    
                 
       
    if (relation->rd_rel->relkind != RELKIND_RELATION && relation->rd_rel->relkind != RELKIND_VIEW && relation->rd_rel->relkind != RELKIND_MATVIEW && relation->rd_rel->relkind != RELKIND_COMPOSITE_TYPE && relation->rd_rel->relkind != RELKIND_FOREIGN_TABLE && relation->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table, view, materialized view, composite type, or foreign table", RelationGetRelationName(relation))));
    }
    break;
  default:
    break;
  }

     
                                                                        
                                                                           
                                                                             
     
  if (stmt->objtype == OBJECT_DATABASE || stmt->objtype == OBJECT_TABLESPACE || stmt->objtype == OBJECT_ROLE)
  {
    CreateSharedComments(address.objectId, address.classId, stmt->comment);
  }
  else
  {
    CreateComments(address.objectId, address.classId, address.objectSubId, stmt->comment);
  }

     
                                                                             
                                                                       
                                                                         
               
     
  if (relation != NULL)
  {
    relation_close(relation, NoLock);
  }

  return address;
}

   
                     
   
                                                                        
                                                                        
   
                                                                       
                                           
   
void
CreateComments(Oid oid, Oid classoid, int32 subid, const char *comment)
{
  Relation description;
  ScanKeyData skey[3];
  SysScanDesc sd;
  HeapTuple oldtuple;
  HeapTuple newtuple = NULL;
  Datum values[Natts_pg_description];
  bool nulls[Natts_pg_description];
  bool replaces[Natts_pg_description];
  int i;

                                        
  if (comment != NULL && strlen(comment) == 0)
  {
    comment = NULL;
  }

                                                       
  if (comment != NULL)
  {
    for (i = 0; i < Natts_pg_description; i++)
    {
      nulls[i] = false;
      replaces[i] = true;
    }
    values[Anum_pg_description_objoid - 1] = ObjectIdGetDatum(oid);
    values[Anum_pg_description_classoid - 1] = ObjectIdGetDatum(classoid);
    values[Anum_pg_description_objsubid - 1] = Int32GetDatum(subid);
    values[Anum_pg_description_description - 1] = CStringGetTextDatum(comment);
  }

                                                        

  ScanKeyInit(&skey[0], Anum_pg_description_objoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(oid));
  ScanKeyInit(&skey[1], Anum_pg_description_classoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(classoid));
  ScanKeyInit(&skey[2], Anum_pg_description_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(subid));

  description = table_open(DescriptionRelationId, RowExclusiveLock);

  sd = systable_beginscan(description, DescriptionObjIndexId, true, NULL, 3, skey);

  while ((oldtuple = systable_getnext(sd)) != NULL)
  {
                                                     

    if (comment == NULL)
    {
      CatalogTupleDelete(description, &oldtuple->t_self);
    }
    else
    {
      newtuple = heap_modify_tuple(oldtuple, RelationGetDescr(description), values, nulls, replaces);
      CatalogTupleUpdate(description, &oldtuple->t_self, newtuple);
    }

    break;                                         
  }

  systable_endscan(sd);

                                                        

  if (newtuple == NULL && comment != NULL)
  {
    newtuple = heap_form_tuple(RelationGetDescr(description), values, nulls);
    CatalogTupleInsert(description, newtuple);
  }

  if (newtuple != NULL)
  {
    heap_freetuple(newtuple);
  }

            

  table_close(description, NoLock);
}

   
                           
   
                                                                           
                                                                              
   
                                                                       
                                           
   
void
CreateSharedComments(Oid oid, Oid classoid, const char *comment)
{
  Relation shdescription;
  ScanKeyData skey[2];
  SysScanDesc sd;
  HeapTuple oldtuple;
  HeapTuple newtuple = NULL;
  Datum values[Natts_pg_shdescription];
  bool nulls[Natts_pg_shdescription];
  bool replaces[Natts_pg_shdescription];
  int i;

                                        
  if (comment != NULL && strlen(comment) == 0)
  {
    comment = NULL;
  }

                                                       
  if (comment != NULL)
  {
    for (i = 0; i < Natts_pg_shdescription; i++)
    {
      nulls[i] = false;
      replaces[i] = true;
    }
    values[Anum_pg_shdescription_objoid - 1] = ObjectIdGetDatum(oid);
    values[Anum_pg_shdescription_classoid - 1] = ObjectIdGetDatum(classoid);
    values[Anum_pg_shdescription_description - 1] = CStringGetTextDatum(comment);
  }

                                                        

  ScanKeyInit(&skey[0], Anum_pg_shdescription_objoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(oid));
  ScanKeyInit(&skey[1], Anum_pg_shdescription_classoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(classoid));

  shdescription = table_open(SharedDescriptionRelationId, RowExclusiveLock);

  sd = systable_beginscan(shdescription, SharedDescriptionObjIndexId, true, NULL, 2, skey);

  while ((oldtuple = systable_getnext(sd)) != NULL)
  {
                                                     

    if (comment == NULL)
    {
      CatalogTupleDelete(shdescription, &oldtuple->t_self);
    }
    else
    {
      newtuple = heap_modify_tuple(oldtuple, RelationGetDescr(shdescription), values, nulls, replaces);
      CatalogTupleUpdate(shdescription, &oldtuple->t_self, newtuple);
    }

    break;                                         
  }

  systable_endscan(sd);

                                                        

  if (newtuple == NULL && comment != NULL)
  {
    newtuple = heap_form_tuple(RelationGetDescr(shdescription), values, nulls);
    CatalogTupleInsert(shdescription, newtuple);
  }

  if (newtuple != NULL)
  {
    heap_freetuple(newtuple);
  }

            

  table_close(shdescription, NoLock);
}

   
                                                   
   
                                                                       
                                                                            
                                                  
   
void
DeleteComments(Oid oid, Oid classoid, int32 subid)
{
  Relation description;
  ScanKeyData skey[3];
  int nkeys;
  SysScanDesc sd;
  HeapTuple oldtuple;

                                                           

  ScanKeyInit(&skey[0], Anum_pg_description_objoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(oid));
  ScanKeyInit(&skey[1], Anum_pg_description_classoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(classoid));

  if (subid != 0)
  {
    ScanKeyInit(&skey[2], Anum_pg_description_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(subid));
    nkeys = 3;
  }
  else
  {
    nkeys = 2;
  }

  description = table_open(DescriptionRelationId, RowExclusiveLock);

  sd = systable_beginscan(description, DescriptionObjIndexId, true, NULL, nkeys, skey);

  while ((oldtuple = systable_getnext(sd)) != NULL)
  {
    CatalogTupleDelete(description, &oldtuple->t_self);
  }

            

  systable_endscan(sd);
  table_close(description, RowExclusiveLock);
}

   
                                                               
   
void
DeleteSharedComments(Oid oid, Oid classoid)
{
  Relation shdescription;
  ScanKeyData skey[2];
  SysScanDesc sd;
  HeapTuple oldtuple;

                                                           

  ScanKeyInit(&skey[0], Anum_pg_shdescription_objoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(oid));
  ScanKeyInit(&skey[1], Anum_pg_shdescription_classoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(classoid));

  shdescription = table_open(SharedDescriptionRelationId, RowExclusiveLock);

  sd = systable_beginscan(shdescription, SharedDescriptionObjIndexId, true, NULL, 2, skey);

  while ((oldtuple = systable_getnext(sd)) != NULL)
  {
    CatalogTupleDelete(shdescription, &oldtuple->t_self);
  }

            

  systable_endscan(sd);
  table_close(shdescription, RowExclusiveLock);
}

   
                                                                      
   
char *
GetComment(Oid oid, Oid classoid, int32 subid)
{
  Relation description;
  ScanKeyData skey[3];
  SysScanDesc sd;
  TupleDesc tupdesc;
  HeapTuple tuple;
  char *comment;

                                                        

  ScanKeyInit(&skey[0], Anum_pg_description_objoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(oid));
  ScanKeyInit(&skey[1], Anum_pg_description_classoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(classoid));
  ScanKeyInit(&skey[2], Anum_pg_description_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(subid));

  description = table_open(DescriptionRelationId, AccessShareLock);
  tupdesc = RelationGetDescr(description);

  sd = systable_beginscan(description, DescriptionObjIndexId, true, NULL, 3, skey);

  comment = NULL;
  while ((tuple = systable_getnext(sd)) != NULL)
  {
    Datum value;
    bool isnull;

                                                
    value = heap_getattr(tuple, Anum_pg_description_description, tupdesc, &isnull);
    if (!isnull)
    {
      comment = TextDatumGetCString(value);
    }
    break;                                         
  }

  systable_endscan(sd);

            
  table_close(description, AccessShareLock);

  return comment;
}
