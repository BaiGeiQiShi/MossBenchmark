                                                                            
   
                   
                                                                    
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/tupconvert.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "commands/tablecmds.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

   
                         
                                    
   
                                                                    
                                                                        
                                                    
   
                                         
   
Oid
CreateConstraintEntry(const char *constraintName, Oid constraintNamespace, char constraintType, bool isDeferrable, bool isDeferred, bool isValidated, Oid parentConstrId, Oid relId, const int16 *constraintKey, int constraintNKeys, int constraintNTotalKeys, Oid domainId, Oid indexRelId, Oid foreignRelId, const int16 *foreignKey, const Oid *pfEqOp, const Oid *ppEqOp, const Oid *ffEqOp, int foreignNKeys, char foreignUpdateType, char foreignDeleteType, char foreignMatchType, const Oid *exclOp, Node *conExpr, const char *conBin, bool conIsLocal, int conInhCount, bool conNoInherit, bool is_internal)
{
  Relation conDesc;
  Oid conOid;
  HeapTuple tup;
  bool nulls[Natts_pg_constraint];
  Datum values[Natts_pg_constraint];
  ArrayType *conkeyArray;
  ArrayType *confkeyArray;
  ArrayType *conpfeqopArray;
  ArrayType *conppeqopArray;
  ArrayType *conffeqopArray;
  ArrayType *conexclopArray;
  NameData cname;
  int i;
  ObjectAddress conobject;

  conDesc = table_open(ConstraintRelationId, RowExclusiveLock);

  Assert(constraintName);
  namestrcpy(&cname, constraintName);

     
                                            
     
  if (constraintNKeys > 0)
  {
    Datum *conkey;

    conkey = (Datum *)palloc(constraintNKeys * sizeof(Datum));
    for (i = 0; i < constraintNKeys; i++)
    {
      conkey[i] = Int16GetDatum(constraintKey[i]);
    }
    conkeyArray = construct_array(conkey, constraintNKeys, INT2OID, 2, true, 's');
  }
  else
  {
    conkeyArray = NULL;
  }

  if (foreignNKeys > 0)
  {
    Datum *fkdatums;

    fkdatums = (Datum *)palloc(foreignNKeys * sizeof(Datum));
    for (i = 0; i < foreignNKeys; i++)
    {
      fkdatums[i] = Int16GetDatum(foreignKey[i]);
    }
    confkeyArray = construct_array(fkdatums, foreignNKeys, INT2OID, 2, true, 's');
    for (i = 0; i < foreignNKeys; i++)
    {
      fkdatums[i] = ObjectIdGetDatum(pfEqOp[i]);
    }
    conpfeqopArray = construct_array(fkdatums, foreignNKeys, OIDOID, sizeof(Oid), true, 'i');
    for (i = 0; i < foreignNKeys; i++)
    {
      fkdatums[i] = ObjectIdGetDatum(ppEqOp[i]);
    }
    conppeqopArray = construct_array(fkdatums, foreignNKeys, OIDOID, sizeof(Oid), true, 'i');
    for (i = 0; i < foreignNKeys; i++)
    {
      fkdatums[i] = ObjectIdGetDatum(ffEqOp[i]);
    }
    conffeqopArray = construct_array(fkdatums, foreignNKeys, OIDOID, sizeof(Oid), true, 'i');
  }
  else
  {
    confkeyArray = NULL;
    conpfeqopArray = NULL;
    conppeqopArray = NULL;
    conffeqopArray = NULL;
  }

  if (exclOp != NULL)
  {
    Datum *opdatums;

    opdatums = (Datum *)palloc(constraintNKeys * sizeof(Datum));
    for (i = 0; i < constraintNKeys; i++)
    {
      opdatums[i] = ObjectIdGetDatum(exclOp[i]);
    }
    conexclopArray = construct_array(opdatums, constraintNKeys, OIDOID, sizeof(Oid), true, 'i');
  }
  else
  {
    conexclopArray = NULL;
  }

                                   
  for (i = 0; i < Natts_pg_constraint; i++)
  {
    nulls[i] = false;
    values[i] = (Datum)NULL;
  }

  conOid = GetNewOidWithIndex(conDesc, ConstraintOidIndexId, Anum_pg_constraint_oid);
  values[Anum_pg_constraint_oid - 1] = ObjectIdGetDatum(conOid);
  values[Anum_pg_constraint_conname - 1] = NameGetDatum(&cname);
  values[Anum_pg_constraint_connamespace - 1] = ObjectIdGetDatum(constraintNamespace);
  values[Anum_pg_constraint_contype - 1] = CharGetDatum(constraintType);
  values[Anum_pg_constraint_condeferrable - 1] = BoolGetDatum(isDeferrable);
  values[Anum_pg_constraint_condeferred - 1] = BoolGetDatum(isDeferred);
  values[Anum_pg_constraint_convalidated - 1] = BoolGetDatum(isValidated);
  values[Anum_pg_constraint_conrelid - 1] = ObjectIdGetDatum(relId);
  values[Anum_pg_constraint_contypid - 1] = ObjectIdGetDatum(domainId);
  values[Anum_pg_constraint_conindid - 1] = ObjectIdGetDatum(indexRelId);
  values[Anum_pg_constraint_conparentid - 1] = ObjectIdGetDatum(parentConstrId);
  values[Anum_pg_constraint_confrelid - 1] = ObjectIdGetDatum(foreignRelId);
  values[Anum_pg_constraint_confupdtype - 1] = CharGetDatum(foreignUpdateType);
  values[Anum_pg_constraint_confdeltype - 1] = CharGetDatum(foreignDeleteType);
  values[Anum_pg_constraint_confmatchtype - 1] = CharGetDatum(foreignMatchType);
  values[Anum_pg_constraint_conislocal - 1] = BoolGetDatum(conIsLocal);
  values[Anum_pg_constraint_coninhcount - 1] = Int32GetDatum(conInhCount);
  values[Anum_pg_constraint_connoinherit - 1] = BoolGetDatum(conNoInherit);

  if (conkeyArray)
  {
    values[Anum_pg_constraint_conkey - 1] = PointerGetDatum(conkeyArray);
  }
  else
  {
    nulls[Anum_pg_constraint_conkey - 1] = true;
  }

  if (confkeyArray)
  {
    values[Anum_pg_constraint_confkey - 1] = PointerGetDatum(confkeyArray);
  }
  else
  {
    nulls[Anum_pg_constraint_confkey - 1] = true;
  }

  if (conpfeqopArray)
  {
    values[Anum_pg_constraint_conpfeqop - 1] = PointerGetDatum(conpfeqopArray);
  }
  else
  {
    nulls[Anum_pg_constraint_conpfeqop - 1] = true;
  }

  if (conppeqopArray)
  {
    values[Anum_pg_constraint_conppeqop - 1] = PointerGetDatum(conppeqopArray);
  }
  else
  {
    nulls[Anum_pg_constraint_conppeqop - 1] = true;
  }

  if (conffeqopArray)
  {
    values[Anum_pg_constraint_conffeqop - 1] = PointerGetDatum(conffeqopArray);
  }
  else
  {
    nulls[Anum_pg_constraint_conffeqop - 1] = true;
  }

  if (conexclopArray)
  {
    values[Anum_pg_constraint_conexclop - 1] = PointerGetDatum(conexclopArray);
  }
  else
  {
    nulls[Anum_pg_constraint_conexclop - 1] = true;
  }

  if (conBin)
  {
    values[Anum_pg_constraint_conbin - 1] = CStringGetTextDatum(conBin);
  }
  else
  {
    nulls[Anum_pg_constraint_conbin - 1] = true;
  }

  tup = heap_form_tuple(RelationGetDescr(conDesc), values, nulls);

  CatalogTupleInsert(conDesc, tup);

  conobject.classId = ConstraintRelationId;
  conobject.objectId = conOid;
  conobject.objectSubId = 0;

  table_close(conDesc, RowExclusiveLock);

  if (OidIsValid(relId))
  {
       
                                                                          
                                                
       
    ObjectAddress relobject;

    relobject.classId = RelationRelationId;
    relobject.objectId = relId;
    if (constraintNTotalKeys > 0)
    {
      for (i = 0; i < constraintNTotalKeys; i++)
      {
        relobject.objectSubId = constraintKey[i];

        recordDependencyOn(&conobject, &relobject, DEPENDENCY_AUTO);
      }
    }
    else
    {
      relobject.objectSubId = 0;

      recordDependencyOn(&conobject, &relobject, DEPENDENCY_AUTO);
    }
  }

  if (OidIsValid(domainId))
  {
       
                                                                 
       
    ObjectAddress domobject;

    domobject.classId = TypeRelationId;
    domobject.objectId = domainId;
    domobject.objectSubId = 0;

    recordDependencyOn(&conobject, &domobject, DEPENDENCY_AUTO);
  }

  if (OidIsValid(foreignRelId))
  {
       
                                                                          
                                                   
       
    ObjectAddress relobject;

    relobject.classId = RelationRelationId;
    relobject.objectId = foreignRelId;
    if (foreignNKeys > 0)
    {
      for (i = 0; i < foreignNKeys; i++)
      {
        relobject.objectSubId = foreignKey[i];

        recordDependencyOn(&conobject, &relobject, DEPENDENCY_NORMAL);
      }
    }
    else
    {
      relobject.objectSubId = 0;

      recordDependencyOn(&conobject, &relobject, DEPENDENCY_NORMAL);
    }
  }

  if (OidIsValid(indexRelId) && constraintType == CONSTRAINT_FOREIGN)
  {
       
                                                                      
                                                                          
                                                                          
                          
       
    ObjectAddress relobject;

    relobject.classId = RelationRelationId;
    relobject.objectId = indexRelId;
    relobject.objectSubId = 0;

    recordDependencyOn(&conobject, &relobject, DEPENDENCY_NORMAL);
  }

  if (foreignNKeys > 0)
  {
       
                                                                           
                                                                           
                                                                         
                  
       
    ObjectAddress oprobject;

    oprobject.classId = OperatorRelationId;
    oprobject.objectSubId = 0;

    for (i = 0; i < foreignNKeys; i++)
    {
      oprobject.objectId = pfEqOp[i];
      recordDependencyOn(&conobject, &oprobject, DEPENDENCY_NORMAL);
      if (ppEqOp[i] != pfEqOp[i])
      {
        oprobject.objectId = ppEqOp[i];
        recordDependencyOn(&conobject, &oprobject, DEPENDENCY_NORMAL);
      }
      if (ffEqOp[i] != pfEqOp[i])
      {
        oprobject.objectId = ffEqOp[i];
        recordDependencyOn(&conobject, &oprobject, DEPENDENCY_NORMAL);
      }
    }
  }

     
                                                                            
                                                                         
                                                                             
                                                                             
                               
     

  if (conExpr != NULL)
  {
       
                                                                           
                   
       
    recordDependencyOnSingleRelExpr(&conobject, conExpr, relId, DEPENDENCY_NORMAL, DEPENDENCY_NORMAL, false);
  }

                                             
  InvokeObjectPostCreateHookArg(ConstraintRelationId, conOid, 0, is_internal);

  return conOid;
}

   
                                                                  
                                              
   
                                                                              
                                                                               
                                                                             
                                                                
   
                                                                   
                                                    
   
bool
ConstraintNameIsUsed(ConstraintCategory conCat, Oid objId, const char *conname)
{
  bool found;
  Relation conDesc;
  SysScanDesc conscan;
  ScanKeyData skey[3];

  conDesc = table_open(ConstraintRelationId, AccessShareLock);

  ScanKeyInit(&skey[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum((conCat == CONSTRAINT_RELATION) ? objId : InvalidOid));
  ScanKeyInit(&skey[1], Anum_pg_constraint_contypid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum((conCat == CONSTRAINT_DOMAIN) ? objId : InvalidOid));
  ScanKeyInit(&skey[2], Anum_pg_constraint_conname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(conname));

  conscan = systable_beginscan(conDesc, ConstraintRelidTypidNameIndexId, true, NULL, 3, skey);

                                             
  found = (HeapTupleIsValid(systable_getnext(conscan)));

  systable_endscan(conscan);
  table_close(conDesc, AccessShareLock);

  return found;
}

   
                                                                       
   
                                                                         
                                                                           
              
   
bool
ConstraintNameExists(const char *conname, Oid namespaceid)
{
  bool found;
  Relation conDesc;
  SysScanDesc conscan;
  ScanKeyData skey[2];

  conDesc = table_open(ConstraintRelationId, AccessShareLock);

  ScanKeyInit(&skey[0], Anum_pg_constraint_conname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(conname));

  ScanKeyInit(&skey[1], Anum_pg_constraint_connamespace, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(namespaceid));

  conscan = systable_beginscan(conDesc, ConstraintNameNspIndexId, true, NULL, 2, skey);

  found = (HeapTupleIsValid(systable_getnext(conscan)));

  systable_endscan(conscan);
  table_close(conDesc, AccessShareLock);

  return found;
}

   
                                                      
   
                                                                    
                                                                     
                                                                       
                                   
   
                                                                          
                                                                             
                                                                             
   
                                                                            
                                                                         
                                       
   
                                                                            
                                                                           
                                                                            
                                     
   
                              
   
char *
ChooseConstraintName(const char *name1, const char *name2, const char *label, Oid namespaceid, List *others)
{
  int pass = 0;
  char *conname = NULL;
  char modlabel[NAMEDATALEN];
  Relation conDesc;
  SysScanDesc conscan;
  ScanKeyData skey[2];
  bool found;
  ListCell *l;

  conDesc = table_open(ConstraintRelationId, AccessShareLock);

                                      
  StrNCpy(modlabel, label, sizeof(modlabel));

  for (;;)
  {
    conname = makeObjectName(name1, name2, modlabel);

    found = false;

    foreach (l, others)
    {
      if (strcmp((char *)lfirst(l), conname) == 0)
      {
        found = true;
        break;
      }
    }

    if (!found)
    {
      ScanKeyInit(&skey[0], Anum_pg_constraint_conname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(conname));

      ScanKeyInit(&skey[1], Anum_pg_constraint_connamespace, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(namespaceid));

      conscan = systable_beginscan(conDesc, ConstraintNameNspIndexId, true, NULL, 2, skey);

      found = (HeapTupleIsValid(systable_getnext(conscan)));

      systable_endscan(conscan);
    }

    if (!found)
    {
      break;
    }

                                                       
    pfree(conname);
    snprintf(modlabel, sizeof(modlabel), "%s%d", label, ++pass);
  }

  table_close(conDesc, AccessShareLock);

  return conname;
}

   
                                      
   
void
RemoveConstraintById(Oid conId)
{
  Relation conDesc;
  HeapTuple tup;
  Form_pg_constraint con;

  conDesc = table_open(ConstraintRelationId, RowExclusiveLock);

  tup = SearchSysCache1(CONSTROID, ObjectIdGetDatum(conId));
  if (!HeapTupleIsValid(tup))                        
  {
    elog(ERROR, "cache lookup failed for constraint %u", conId);
  }
  con = (Form_pg_constraint)GETSTRUCT(tup);

     
                                                                 
     
  if (OidIsValid(con->conrelid))
  {
    Relation rel;

       
                                                                        
                          
       
    rel = table_open(con->conrelid, AccessExclusiveLock);

       
                                                                        
                                                                           
                               
       
    if (con->contype == CONSTRAINT_CHECK)
    {
      Relation pgrel;
      HeapTuple relTup;
      Form_pg_class classForm;

      pgrel = table_open(RelationRelationId, RowExclusiveLock);
      relTup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(con->conrelid));
      if (!HeapTupleIsValid(relTup))
      {
        elog(ERROR, "cache lookup failed for relation %u", con->conrelid);
      }
      classForm = (Form_pg_class)GETSTRUCT(relTup);

      if (classForm->relchecks == 0)                        
      {
        elog(ERROR, "relation \"%s\" has relchecks = 0", RelationGetRelationName(rel));
      }
      classForm->relchecks--;

      CatalogTupleUpdate(pgrel, &relTup->t_self, relTup);

      heap_freetuple(relTup);

      table_close(pgrel, RowExclusiveLock);
    }

                                                         
    table_close(rel, NoLock);
  }
  else if (OidIsValid(con->contypid))
  {
       
                                                                         
       
                                                                         
                                                  
       
  }
  else
  {
    elog(ERROR, "constraint %u is not of a known type", conId);
  }

                                 
  CatalogTupleDelete(conDesc, &tup->t_self);

                
  ReleaseSysCache(tup);
  table_close(conDesc, RowExclusiveLock);
}

   
                        
                         
   
                                                                             
                                                                           
                                                                             
                                                                       
               
   
void
RenameConstraintById(Oid conId, const char *newname)
{
  Relation conDesc;
  HeapTuple tuple;
  Form_pg_constraint con;

  conDesc = table_open(ConstraintRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopy1(CONSTROID, ObjectIdGetDatum(conId));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for constraint %u", conId);
  }
  con = (Form_pg_constraint)GETSTRUCT(tuple);

     
                                                                      
     
  if (OidIsValid(con->conrelid) && ConstraintNameIsUsed(CONSTRAINT_RELATION, con->conrelid, newname))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("constraint \"%s\" for relation \"%s\" already exists", newname, get_rel_name(con->conrelid))));
  }
  if (OidIsValid(con->contypid) && ConstraintNameIsUsed(CONSTRAINT_DOMAIN, con->contypid, newname))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("constraint \"%s\" for domain %s already exists", newname, format_type_be(con->contypid))));
  }

                                                                      
  namestrcpy(&(con->conname), newname);

  CatalogTupleUpdate(conDesc, &tuple->t_self, tuple);

  InvokeObjectPostAlterHook(ConstraintRelationId, conId, 0);

  heap_freetuple(tuple);
  table_close(conDesc, RowExclusiveLock);
}

   
                             
                                                            
                                                  
   
                                                                       
   
void
AlterConstraintNamespaces(Oid ownerId, Oid oldNspId, Oid newNspId, bool isType, ObjectAddresses *objsMoved)
{
  Relation conRel;
  ScanKeyData key[2];
  SysScanDesc scan;
  HeapTuple tup;

  conRel = table_open(ConstraintRelationId, RowExclusiveLock);

  ScanKeyInit(&key[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(isType ? InvalidOid : ownerId));
  ScanKeyInit(&key[1], Anum_pg_constraint_contypid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(isType ? ownerId : InvalidOid));

  scan = systable_beginscan(conRel, ConstraintRelidTypidNameIndexId, true, NULL, 2, key);

  while (HeapTupleIsValid((tup = systable_getnext(scan))))
  {
    Form_pg_constraint conform = (Form_pg_constraint)GETSTRUCT(tup);
    ObjectAddress thisobj;

    thisobj.classId = ConstraintRelationId;
    thisobj.objectId = conform->oid;
    thisobj.objectSubId = 0;

    if (object_address_present(&thisobj, objsMoved))
    {
      continue;
    }

                                                                     
    if (conform->connamespace == oldNspId && oldNspId != newNspId)
    {
      tup = heap_copytuple(tup);
      conform = (Form_pg_constraint)GETSTRUCT(tup);

      conform->connamespace = newNspId;

      CatalogTupleUpdate(conRel, &tup->t_self, tup);

         
                                                               
                                                             
                                
         
    }

    InvokeObjectPostAlterHook(ConstraintRelationId, thisobj.objectId, 0);

    add_exact_object_address(&thisobj, objsMoved);
  }

  systable_endscan(scan);

  table_close(conRel, RowExclusiveLock);
}

   
                                 
                                                                    
                                                           
   
                                                                                
                                                                            
                                             
   
void
ConstraintSetParentConstraint(Oid childConstrId, Oid parentConstrId, Oid childTableId)
{
  Relation constrRel;
  Form_pg_constraint constrForm;
  HeapTuple tuple, newtup;
  ObjectAddress depender;
  ObjectAddress referenced;

  constrRel = table_open(ConstraintRelationId, RowExclusiveLock);
  tuple = SearchSysCache1(CONSTROID, ObjectIdGetDatum(childConstrId));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for constraint %u", childConstrId);
  }
  newtup = heap_copytuple(tuple);
  constrForm = (Form_pg_constraint)GETSTRUCT(newtup);
  if (OidIsValid(parentConstrId))
  {
                                                                          
    Assert(constrForm->coninhcount == 0);
    if (constrForm->conparentid != InvalidOid)
    {
      elog(ERROR, "constraint %u already has a parent constraint", childConstrId);
    }

    constrForm->conislocal = false;
    constrForm->coninhcount++;
    constrForm->conparentid = parentConstrId;

    CatalogTupleUpdate(constrRel, &tuple->t_self, newtup);

    ObjectAddressSet(depender, ConstraintRelationId, childConstrId);

    ObjectAddressSet(referenced, ConstraintRelationId, parentConstrId);
    recordDependencyOn(&depender, &referenced, DEPENDENCY_PARTITION_PRI);

    ObjectAddressSet(referenced, RelationRelationId, childTableId);
    recordDependencyOn(&depender, &referenced, DEPENDENCY_PARTITION_SEC);
  }
  else
  {
    constrForm->coninhcount--;
    constrForm->conislocal = true;
    constrForm->conparentid = InvalidOid;

                                                   
    Assert(constrForm->coninhcount == 0);

    CatalogTupleUpdate(constrRel, &tuple->t_self, newtup);

    deleteDependencyRecordsForClass(ConstraintRelationId, childConstrId, ConstraintRelationId, DEPENDENCY_PARTITION_PRI);
    deleteDependencyRecordsForClass(ConstraintRelationId, childConstrId, RelationRelationId, DEPENDENCY_PARTITION_SEC);
  }

  ReleaseSysCache(tuple);
  table_close(constrRel, RowExclusiveLock);
}

   
                               
                                                                         
                              
   
Oid
get_relation_constraint_oid(Oid relid, const char *conname, bool missing_ok)
{
  Relation pg_constraint;
  HeapTuple tuple;
  SysScanDesc scan;
  ScanKeyData skey[3];
  Oid conOid = InvalidOid;

  pg_constraint = table_open(ConstraintRelationId, AccessShareLock);

  ScanKeyInit(&skey[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));
  ScanKeyInit(&skey[1], Anum_pg_constraint_contypid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(InvalidOid));
  ScanKeyInit(&skey[2], Anum_pg_constraint_conname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(conname));

  scan = systable_beginscan(pg_constraint, ConstraintRelidTypidNameIndexId, true, NULL, 3, skey);

                                             
  if (HeapTupleIsValid(tuple = systable_getnext(scan)))
  {
    conOid = ((Form_pg_constraint)GETSTRUCT(tuple))->oid;
  }

  systable_endscan(scan);

                                              
  if (!OidIsValid(conOid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("constraint \"%s\" for table \"%s\" does not exist", conname, get_rel_name(relid))));
  }

  table_close(pg_constraint, AccessShareLock);

  return conOid;
}

   
                                  
                                                                        
                                        
   
                                                                             
                                                                            
                               
   
                                                                        
            
   
Bitmapset *
get_relation_constraint_attnos(Oid relid, const char *conname, bool missing_ok, Oid *constraintOid)
{
  Bitmapset *conattnos = NULL;
  Relation pg_constraint;
  HeapTuple tuple;
  SysScanDesc scan;
  ScanKeyData skey[3];

                                                                        
  *constraintOid = InvalidOid;

  pg_constraint = table_open(ConstraintRelationId, AccessShareLock);

  ScanKeyInit(&skey[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));
  ScanKeyInit(&skey[1], Anum_pg_constraint_contypid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(InvalidOid));
  ScanKeyInit(&skey[2], Anum_pg_constraint_conname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(conname));

  scan = systable_beginscan(pg_constraint, ConstraintRelidTypidNameIndexId, true, NULL, 3, skey);

                                             
  if (HeapTupleIsValid(tuple = systable_getnext(scan)))
  {
    Datum adatum;
    bool isNull;

    *constraintOid = ((Form_pg_constraint)GETSTRUCT(tuple))->oid;

                                                                      
    adatum = heap_getattr(tuple, Anum_pg_constraint_conkey, RelationGetDescr(pg_constraint), &isNull);
    if (!isNull)
    {
      ArrayType *arr;
      int numcols;
      int16 *attnums;
      int i;

      arr = DatumGetArrayTypeP(adatum);                         
      numcols = ARR_DIMS(arr)[0];
      if (ARR_NDIM(arr) != 1 || numcols < 0 || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != INT2OID)
      {
        elog(ERROR, "conkey is not a 1-D smallint array");
      }
      attnums = (int16 *)ARR_DATA_PTR(arr);

                                      
      for (i = 0; i < numcols; i++)
      {
        conattnos = bms_add_member(conattnos, attnums[i] - FirstLowInvalidHeapAttributeNumber);
      }
    }
  }

  systable_endscan(scan);

                                              
  if (!OidIsValid(*constraintOid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("constraint \"%s\" for table \"%s\" does not exist", conname, get_rel_name(relid))));
  }

  table_close(pg_constraint, AccessShareLock);

  return conattnos;
}

   
                                                                       
                                                                 
   
                                                                            
                                                                          
                                                                    
   
Oid
get_relation_idx_constraint_oid(Oid relationId, Oid indexId)
{
  Relation pg_constraint;
  SysScanDesc scan;
  ScanKeyData key;
  HeapTuple tuple;
  Oid constraintId = InvalidOid;

  pg_constraint = table_open(ConstraintRelationId, AccessShareLock);

  ScanKeyInit(&key, Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relationId));
  scan = systable_beginscan(pg_constraint, ConstraintRelidTypidNameIndexId, true, NULL, 1, &key);
  while ((tuple = systable_getnext(scan)) != NULL)
  {
    Form_pg_constraint constrForm;

    constrForm = (Form_pg_constraint)GETSTRUCT(tuple);

                   
    if (constrForm->contype != CONSTRAINT_PRIMARY && constrForm->contype != CONSTRAINT_UNIQUE && constrForm->contype != CONSTRAINT_EXCLUSION)
    {
      continue;
    }

    if (constrForm->conindid == indexId)
    {
      constraintId = constrForm->oid;
      break;
    }
  }
  systable_endscan(scan);

  table_close(pg_constraint, AccessShareLock);
  return constraintId;
}

   
                             
                                                                       
                              
   
Oid
get_domain_constraint_oid(Oid typid, const char *conname, bool missing_ok)
{
  Relation pg_constraint;
  HeapTuple tuple;
  SysScanDesc scan;
  ScanKeyData skey[3];
  Oid conOid = InvalidOid;

  pg_constraint = table_open(ConstraintRelationId, AccessShareLock);

  ScanKeyInit(&skey[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(InvalidOid));
  ScanKeyInit(&skey[1], Anum_pg_constraint_contypid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(typid));
  ScanKeyInit(&skey[2], Anum_pg_constraint_conname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(conname));

  scan = systable_beginscan(pg_constraint, ConstraintRelidTypidNameIndexId, true, NULL, 3, skey);

                                             
  if (HeapTupleIsValid(tuple = systable_getnext(scan)))
  {
    conOid = ((Form_pg_constraint)GETSTRUCT(tuple))->oid;
  }

  systable_endscan(scan);

                                              
  if (!OidIsValid(conOid) && !missing_ok)
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("constraint \"%s\" for domain %s does not exist", conname, format_type_be(typid))));
  }

  table_close(pg_constraint, AccessShareLock);

  return conOid;
}

   
                          
                                                              
   
                                                                          
                                                                          
                                      
   
                                                                             
                                                       
   
                                                                          
               
   
Bitmapset *
get_primary_key_attnos(Oid relid, bool deferrableOk, Oid *constraintOid)
{
  Bitmapset *pkattnos = NULL;
  Relation pg_constraint;
  HeapTuple tuple;
  SysScanDesc scan;
  ScanKeyData skey[1];

                                                                        
  *constraintOid = InvalidOid;

                                                            
  pg_constraint = table_open(ConstraintRelationId, AccessShareLock);

  ScanKeyInit(&skey[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));

  scan = systable_beginscan(pg_constraint, ConstraintRelidTypidNameIndexId, true, NULL, 1, skey);

  while (HeapTupleIsValid(tuple = systable_getnext(scan)))
  {
    Form_pg_constraint con = (Form_pg_constraint)GETSTRUCT(tuple);
    Datum adatum;
    bool isNull;
    ArrayType *arr;
    int16 *attnums;
    int numkeys;
    int i;

                                                    
    if (con->contype != CONSTRAINT_PRIMARY)
    {
      continue;
    }

       
                                                                      
                                                                    
                                                                           
       
    if (con->condeferrable && !deferrableOk)
    {
      break;
    }

                                                               
    adatum = heap_getattr(tuple, Anum_pg_constraint_conkey, RelationGetDescr(pg_constraint), &isNull);
    if (isNull)
    {
      elog(ERROR, "null conkey for constraint %u", ((Form_pg_constraint)GETSTRUCT(tuple))->oid);
    }
    arr = DatumGetArrayTypeP(adatum);                         
    numkeys = ARR_DIMS(arr)[0];
    if (ARR_NDIM(arr) != 1 || numkeys < 0 || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != INT2OID)
    {
      elog(ERROR, "conkey is not a 1-D smallint array");
    }
    attnums = (int16 *)ARR_DATA_PTR(arr);

                                    
    for (i = 0; i < numkeys; i++)
    {
      pkattnos = bms_add_member(pkattnos, attnums[i] - FirstLowInvalidHeapAttributeNumber);
    }
    *constraintOid = ((Form_pg_constraint)GETSTRUCT(tuple))->oid;

                                   
    break;
  }

  systable_endscan(scan);

  table_close(pg_constraint, AccessShareLock);

  return pkattnos;
}

   
                                                                          
   
                                                                             
                                                      
   
void
DeconstructFkConstraintRow(HeapTuple tuple, int *numfks, AttrNumber *conkey, AttrNumber *confkey, Oid *pf_eq_oprs, Oid *pp_eq_oprs, Oid *ff_eq_oprs)
{
  Oid constrId;
  Datum adatum;
  bool isNull;
  ArrayType *arr;
  int numkeys;

  constrId = ((Form_pg_constraint)GETSTRUCT(tuple))->oid;

     
                                                                            
                                                                           
                                             
     
  adatum = SysCacheGetAttr(CONSTROID, tuple, Anum_pg_constraint_conkey, &isNull);
  if (isNull)
  {
    elog(ERROR, "null conkey for constraint %u", constrId);
  }
  arr = DatumGetArrayTypeP(adatum);                         
  if (ARR_NDIM(arr) != 1 || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != INT2OID)
  {
    elog(ERROR, "conkey is not a 1-D smallint array");
  }
  numkeys = ARR_DIMS(arr)[0];
  if (numkeys <= 0 || numkeys > INDEX_MAX_KEYS)
  {
    elog(ERROR, "foreign key constraint cannot have %d columns", numkeys);
  }
  memcpy(conkey, ARR_DATA_PTR(arr), numkeys * sizeof(int16));
  if ((Pointer)arr != DatumGetPointer(adatum))
  {
    pfree(arr);                                   
  }

  adatum = SysCacheGetAttr(CONSTROID, tuple, Anum_pg_constraint_confkey, &isNull);
  if (isNull)
  {
    elog(ERROR, "null confkey for constraint %u", constrId);
  }
  arr = DatumGetArrayTypeP(adatum);                         
  if (ARR_NDIM(arr) != 1 || ARR_DIMS(arr)[0] != numkeys || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != INT2OID)
  {
    elog(ERROR, "confkey is not a 1-D smallint array");
  }
  memcpy(confkey, ARR_DATA_PTR(arr), numkeys * sizeof(int16));
  if ((Pointer)arr != DatumGetPointer(adatum))
  {
    pfree(arr);                                   
  }

  if (pf_eq_oprs)
  {
    adatum = SysCacheGetAttr(CONSTROID, tuple, Anum_pg_constraint_conpfeqop, &isNull);
    if (isNull)
    {
      elog(ERROR, "null conpfeqop for constraint %u", constrId);
    }
    arr = DatumGetArrayTypeP(adatum);                         
                                                             
    if (ARR_NDIM(arr) != 1 || ARR_DIMS(arr)[0] != numkeys || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != OIDOID)
    {
      elog(ERROR, "conpfeqop is not a 1-D Oid array");
    }
    memcpy(pf_eq_oprs, ARR_DATA_PTR(arr), numkeys * sizeof(Oid));
    if ((Pointer)arr != DatumGetPointer(adatum))
    {
      pfree(arr);                                   
    }
  }

  if (pp_eq_oprs)
  {
    adatum = SysCacheGetAttr(CONSTROID, tuple, Anum_pg_constraint_conppeqop, &isNull);
    if (isNull)
    {
      elog(ERROR, "null conppeqop for constraint %u", constrId);
    }
    arr = DatumGetArrayTypeP(adatum);                         
    if (ARR_NDIM(arr) != 1 || ARR_DIMS(arr)[0] != numkeys || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != OIDOID)
    {
      elog(ERROR, "conppeqop is not a 1-D Oid array");
    }
    memcpy(pp_eq_oprs, ARR_DATA_PTR(arr), numkeys * sizeof(Oid));
    if ((Pointer)arr != DatumGetPointer(adatum))
    {
      pfree(arr);                                   
    }
  }

  if (ff_eq_oprs)
  {
    adatum = SysCacheGetAttr(CONSTROID, tuple, Anum_pg_constraint_conffeqop, &isNull);
    if (isNull)
    {
      elog(ERROR, "null conffeqop for constraint %u", constrId);
    }
    arr = DatumGetArrayTypeP(adatum);                         
    if (ARR_NDIM(arr) != 1 || ARR_DIMS(arr)[0] != numkeys || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != OIDOID)
    {
      elog(ERROR, "conffeqop is not a 1-D Oid array");
    }
    memcpy(ff_eq_oprs, ARR_DATA_PTR(arr), numkeys * sizeof(Oid));
    if ((Pointer)arr != DatumGetPointer(adatum))
    {
      pfree(arr);                                   
    }
  }

  *numfks = numkeys;
}

   
                                                                        
                                                                            
                                                                             
   
                                                                           
                                                                      
   
                                                                         
                                                                               
                                                                           
                                                                              
                                                                            
                     
   
bool
check_functional_grouping(Oid relid, Index varno, Index varlevelsup, List *grouping_columns, List **constraintDeps)
{
  Bitmapset *pkattnos;
  Bitmapset *groupbyattnos;
  Oid constraintOid;
  ListCell *gl;

                                                                       
  pkattnos = get_primary_key_attnos(relid, false, &constraintOid);
  if (pkattnos == NULL)
  {
    return false;
  }

                                                                      
  groupbyattnos = NULL;
  foreach (gl, grouping_columns)
  {
    Var *gvar = (Var *)lfirst(gl);

    if (IsA(gvar, Var) && gvar->varno == varno && gvar->varlevelsup == varlevelsup)
    {
      groupbyattnos = bms_add_member(groupbyattnos, gvar->varattno - FirstLowInvalidHeapAttributeNumber);
    }
  }

  if (bms_is_subset(pkattnos, groupbyattnos))
  {
                                                           
    *constraintDeps = lappend_oid(*constraintDeps, constraintOid);
    return true;
  }

  return false;
}
