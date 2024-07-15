                                                                            
   
                 
   
                                                               
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include <limits.h>

#include "access/genam.h"
#include "access/hash.h"
#include "access/nbtree.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/opfam_internal.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/alter.h"
#include "commands/defrem.h"
#include "commands/event_trigger.h"
#include "miscadmin.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static void
AlterOpFamilyAdd(AlterOpFamilyStmt *stmt, Oid amoid, Oid opfamilyoid, int maxOpNumber, int maxProcNumber, List *items);
static void
AlterOpFamilyDrop(AlterOpFamilyStmt *stmt, Oid amoid, Oid opfamilyoid, int maxOpNumber, int maxProcNumber, List *items);
static void
processTypesSpec(List *args, Oid *lefttype, Oid *righttype);
static void
assignOperTypes(OpFamilyMember *member, Oid amoid, Oid typeoid);
static void
assignProcTypes(OpFamilyMember *member, Oid amoid, Oid typeoid);
static void
addFamilyMember(List **list, OpFamilyMember *member, bool isProc);
static void
storeOperators(List *opfamilyname, Oid amoid, Oid opfamilyoid, Oid opclassoid, List *operators, bool isAdd);
static void
storeProcedures(List *opfamilyname, Oid amoid, Oid opfamilyoid, Oid opclassoid, List *procedures, bool isAdd);
static void
dropOperators(List *opfamilyname, Oid amoid, Oid opfamilyoid, List *operators);
static void
dropProcedures(List *opfamilyname, Oid amoid, Oid opfamilyoid, List *procedures);

   
                       
                                          
   
                                                             
   
static HeapTuple
OpFamilyCacheLookup(Oid amID, List *opfamilyname, bool missing_ok)
{
  char *schemaname;
  char *opfname;
  HeapTuple htup;

                                 
  DeconstructQualifiedName(opfamilyname, &schemaname, &opfname);

  if (schemaname)
  {
                                      
    Oid namespaceId;

    namespaceId = LookupExplicitNamespace(schemaname, missing_ok);
    if (!OidIsValid(namespaceId))
    {
      htup = NULL;
    }
    else
    {
      htup = SearchSysCache3(OPFAMILYAMNAMENSP, ObjectIdGetDatum(amID), PointerGetDatum(opfname), ObjectIdGetDatum(namespaceId));
    }
  }
  else
  {
                                                              
    Oid opfID = OpfamilynameGetOpfid(amID, opfname);

    if (!OidIsValid(opfID))
    {
      htup = NULL;
    }
    else
    {
      htup = SearchSysCache1(OPFAMILYOID, ObjectIdGetDatum(opfID));
    }
  }

  if (!HeapTupleIsValid(htup) && !missing_ok)
  {
    HeapTuple amtup;

    amtup = SearchSysCache1(AMOID, ObjectIdGetDatum(amID));
    if (!HeapTupleIsValid(amtup))
    {
      elog(ERROR, "cache lookup failed for access method %u", amID);
    }
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("operator family \"%s\" does not exist for access method \"%s\"", NameListToString(opfamilyname), NameStr(((Form_pg_am)GETSTRUCT(amtup))->amname))));
  }

  return htup;
}

   
                    
                                                     
   
                                                                      
   
Oid
get_opfamily_oid(Oid amID, List *opfamilyname, bool missing_ok)
{
  HeapTuple htup;
  Form_pg_opfamily opfamform;
  Oid opfID;

  htup = OpFamilyCacheLookup(amID, opfamilyname, missing_ok);
  if (!HeapTupleIsValid(htup))
  {
    return InvalidOid;
  }
  opfamform = (Form_pg_opfamily)GETSTRUCT(htup);
  opfID = opfamform->oid;
  ReleaseSysCache(htup);

  return opfID;
}

   
                      
                                         
   
                                                             
   
static HeapTuple
OpClassCacheLookup(Oid amID, List *opclassname, bool missing_ok)
{
  char *schemaname;
  char *opcname;
  HeapTuple htup;

                                 
  DeconstructQualifiedName(opclassname, &schemaname, &opcname);

  if (schemaname)
  {
                                      
    Oid namespaceId;

    namespaceId = LookupExplicitNamespace(schemaname, missing_ok);
    if (!OidIsValid(namespaceId))
    {
      htup = NULL;
    }
    else
    {
      htup = SearchSysCache3(CLAAMNAMENSP, ObjectIdGetDatum(amID), PointerGetDatum(opcname), ObjectIdGetDatum(namespaceId));
    }
  }
  else
  {
                                                             
    Oid opcID = OpclassnameGetOpcid(amID, opcname);

    if (!OidIsValid(opcID))
    {
      htup = NULL;
    }
    else
    {
      htup = SearchSysCache1(CLAOID, ObjectIdGetDatum(opcID));
    }
  }

  if (!HeapTupleIsValid(htup) && !missing_ok)
  {
    HeapTuple amtup;

    amtup = SearchSysCache1(AMOID, ObjectIdGetDatum(amID));
    if (!HeapTupleIsValid(amtup))
    {
      elog(ERROR, "cache lookup failed for access method %u", amID);
    }
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("operator class \"%s\" does not exist for access method \"%s\"", NameListToString(opclassname), NameStr(((Form_pg_am)GETSTRUCT(amtup))->amname))));
  }

  return htup;
}

   
                   
                                                    
   
                                                                      
   
Oid
get_opclass_oid(Oid amID, List *opclassname, bool missing_ok)
{
  HeapTuple htup;
  Form_pg_opclass opcform;
  Oid opcID;

  htup = OpClassCacheLookup(amID, opclassname, missing_ok);
  if (!HeapTupleIsValid(htup))
  {
    return InvalidOid;
  }
  opcform = (Form_pg_opclass)GETSTRUCT(htup);
  opcID = opcform->oid;
  ReleaseSysCache(htup);

  return opcID;
}

   
                  
                                                                          
   
                                                          
   
static ObjectAddress
CreateOpFamily(CreateOpFamilyStmt *stmt, const char *opfname, Oid namespaceoid, Oid amoid)
{
  Oid opfamilyoid;
  Relation rel;
  HeapTuple tup;
  Datum values[Natts_pg_opfamily];
  bool nulls[Natts_pg_opfamily];
  NameData opfName;
  ObjectAddress myself, referenced;

  rel = table_open(OperatorFamilyRelationId, RowExclusiveLock);

     
                                                                           
                                                               
     
  if (SearchSysCacheExists3(OPFAMILYAMNAMENSP, ObjectIdGetDatum(amoid), CStringGetDatum(opfname), ObjectIdGetDatum(namespaceoid)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("operator family \"%s\" for access method \"%s\" already exists", opfname, stmt->amname)));
  }

     
                                               
     
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));

  opfamilyoid = GetNewOidWithIndex(rel, OpfamilyOidIndexId, Anum_pg_opfamily_oid);
  values[Anum_pg_opfamily_oid - 1] = ObjectIdGetDatum(opfamilyoid);
  values[Anum_pg_opfamily_opfmethod - 1] = ObjectIdGetDatum(amoid);
  namestrcpy(&opfName, opfname);
  values[Anum_pg_opfamily_opfname - 1] = NameGetDatum(&opfName);
  values[Anum_pg_opfamily_opfnamespace - 1] = ObjectIdGetDatum(namespaceoid);
  values[Anum_pg_opfamily_opfowner - 1] = ObjectIdGetDatum(GetUserId());

  tup = heap_form_tuple(rel->rd_att, values, nulls);

  CatalogTupleInsert(rel, tup);

  heap_freetuple(tup);

     
                                                  
     
  myself.classId = OperatorFamilyRelationId;
  myself.objectId = opfamilyoid;
  myself.objectSubId = 0;

                                   
  referenced.classId = AccessMethodRelationId;
  referenced.objectId = amoid;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_AUTO);

                               
  referenced.classId = NamespaceRelationId;
  referenced.objectId = namespaceoid;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                           
  recordDependencyOnOwner(OperatorFamilyRelationId, opfamilyoid, GetUserId());

                               
  recordDependencyOnCurrentExtension(&myself, false);

                                                                            
  EventTriggerCollectSimpleCommand(myself, InvalidObjectAddress, (Node *)stmt);

                                                  
  InvokeObjectPostCreateHook(OperatorFamilyRelationId, opfamilyoid, 0);

  table_close(rel, RowExclusiveLock);

  return myself;
}

   
                 
                                       
   
ObjectAddress
DefineOpClass(CreateOpClassStmt *stmt)
{
  char *opcname;                                         
  Oid amoid,                           
      typeoid,                                   
      storageoid,                                      
      namespaceoid,                                      
      opfamilyoid,                                   
      opclassoid;                                  
  int maxOpNumber,                           
      maxProcNumber;                      
  bool amstorage;                        
  List *operators;                                          
  List *procedures;                                             
  ListCell *l;
  Relation rel;
  HeapTuple tup;
  Form_pg_am amform;
  IndexAmRoutine *amroutine;
  Datum values[Natts_pg_opclass];
  bool nulls[Natts_pg_opclass];
  AclResult aclresult;
  NameData opcName;
  ObjectAddress myself, referenced;

                                                     
  namespaceoid = QualifiedNameGetCreationNamespace(stmt->opclassname, &opcname);

                                                         
  aclresult = pg_namespace_aclcheck(namespaceoid, GetUserId(), ACL_CREATE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_SCHEMA, get_namespace_name(namespaceoid));
  }

                                              
  tup = SearchSysCache1(AMNAME, CStringGetDatum(stmt->amname));
  if (!HeapTupleIsValid(tup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("access method \"%s\" does not exist", stmt->amname)));
  }

  amform = (Form_pg_am)GETSTRUCT(tup);
  amoid = amform->oid;
  amroutine = GetIndexAmRoutineByAmId(amoid, false);
  ReleaseSysCache(tup);

  maxOpNumber = amroutine->amstrategies;
                                                                          
  if (maxOpNumber <= 0)
  {
    maxOpNumber = SHRT_MAX;
  }
  maxProcNumber = amroutine->amsupport;
  amstorage = amroutine->amstorage;

                                                              

     
                                                                          
                                                                        
                                                                         
                                                                            
                                                                     
                                                                         
                                                                          
                                                                            
                                                                         
                         
     
                                                                           
                                                                             
                                                                           
                                                                         
                                             
     
                                                                         
     
  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to create an operator class")));
  }

                            
  typeoid = typenameTypeId(NULL, stmt->datatype);

#ifdef NOT_USED
                                                               
                                               
  if (!pg_type_ownercheck(typeoid, GetUserId()))
  {
    aclcheck_error_type(ACLCHECK_NOT_OWNER, typeoid);
  }
#endif

     
                                                                            
                                                  
     
  if (stmt->opfamilyname)
  {
    opfamilyoid = get_opfamily_oid(amoid, stmt->opfamilyname, false);
  }
  else
  {
                                                           
    tup = SearchSysCache3(OPFAMILYAMNAMENSP, ObjectIdGetDatum(amoid), PointerGetDatum(opcname), ObjectIdGetDatum(namespaceoid));
    if (HeapTupleIsValid(tup))
    {
      opfamilyoid = ((Form_pg_opfamily)GETSTRUCT(tup))->oid;

         
                                                                     
                              
         
      ReleaseSysCache(tup);
    }
    else
    {
      CreateOpFamilyStmt *opfstmt;
      ObjectAddress tmpAddr;

      opfstmt = makeNode(CreateOpFamilyStmt);
      opfstmt->opfamilyname = stmt->opclassname;
      opfstmt->amname = stmt->amname;

         
                                                              
         
      tmpAddr = CreateOpFamily(opfstmt, opcname, namespaceoid, amoid);
      opfamilyoid = tmpAddr.objectId;
    }
  }

  operators = NIL;
  procedures = NIL;

                                    
  storageoid = InvalidOid;

     
                                                      
     
  foreach (l, stmt->items)
  {
    CreateOpClassItem *item = lfirst_node(CreateOpClassItem, l);
    Oid operOid;
    Oid funcOid;
    Oid sortfamilyOid;
    OpFamilyMember *member;

    switch (item->itemtype)
    {
    case OPCLASS_ITEM_OPERATOR:
      if (item->number <= 0 || item->number > maxOpNumber)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("invalid operator number %d,"
                                                                           " must be between 1 and %d",
                                                                        item->number, maxOpNumber)));
      }
      if (item->name->objargs != NIL)
      {
        operOid = LookupOperWithArgs(item->name, false);
      }
      else
      {
                                                    
        operOid = LookupOperName(NULL, item->name->objname, typeoid, typeoid, false, -1);
      }

      if (item->order_family)
      {
        sortfamilyOid = get_opfamily_oid(BTREE_AM_OID, item->order_family, false);
      }
      else
      {
        sortfamilyOid = InvalidOid;
      }

#ifdef NOT_USED
                                                                   
                                                                
      if (!pg_oper_ownercheck(operOid, GetUserId()))
      {
        aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_OPERATOR, get_opname(operOid));
      }
      funcOid = get_opcode(operOid);
      if (!pg_proc_ownercheck(funcOid, GetUserId()))
      {
        aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_FUNCTION, get_func_name(funcOid));
      }
#endif

                         
      member = (OpFamilyMember *)palloc0(sizeof(OpFamilyMember));
      member->object = operOid;
      member->number = item->number;
      member->sortfamily = sortfamilyOid;
      assignOperTypes(member, amoid, typeoid);
      addFamilyMember(&operators, member, false);
      break;
    case OPCLASS_ITEM_FUNCTION:
      if (item->number <= 0 || item->number > maxProcNumber)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("invalid function number %d,"
                                                                           " must be between 1 and %d",
                                                                        item->number, maxProcNumber)));
      }
      funcOid = LookupFuncWithArgs(OBJECT_FUNCTION, item->name, false);
#ifdef NOT_USED
                                                                   
                                    
      if (!pg_proc_ownercheck(funcOid, GetUserId()))
      {
        aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_FUNCTION, get_func_name(funcOid));
      }
#endif

                         
      member = (OpFamilyMember *)palloc0(sizeof(OpFamilyMember));
      member->object = funcOid;
      member->number = item->number;

                                                               
      if (item->class_args)
      {
        processTypesSpec(item->class_args, &member->lefttype, &member->righttype);
      }

      assignProcTypes(member, amoid, typeoid);
      addFamilyMember(&procedures, member, true);
      break;
    case OPCLASS_ITEM_STORAGETYPE:
      if (OidIsValid(storageoid))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("storage type specified more than once")));
      }
      storageoid = typenameTypeId(NULL, item->storedtype);

#ifdef NOT_USED
                                                                   
                                                   
      if (!pg_type_ownercheck(storageoid, GetUserId()))
      {
        aclcheck_error_type(ACLCHECK_NOT_OWNER, storageoid);
      }
#endif
      break;
    default:
      elog(ERROR, "unrecognized item type: %d", item->itemtype);
      break;
    }
  }

     
                                                        
     
  if (OidIsValid(storageoid))
  {
                                                       
    if (storageoid == typeoid)
    {
      storageoid = InvalidOid;
    }
    else if (!amstorage)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("storage type cannot be different from data type for access method \"%s\"", stmt->amname)));
    }
  }

  rel = table_open(OperatorClassRelationId, RowExclusiveLock);

     
                                                                          
                                                               
     
  if (SearchSysCacheExists3(CLAAMNAMENSP, ObjectIdGetDatum(amoid), CStringGetDatum(opcname), ObjectIdGetDatum(namespaceoid)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("operator class \"%s\" for access method \"%s\" already exists", opcname, stmt->amname)));
  }

     
                                                                          
                                                                           
                                                                  
     
  if (stmt->isDefault)
  {
    ScanKeyData skey[1];
    SysScanDesc scan;

    ScanKeyInit(&skey[0], Anum_pg_opclass_opcmethod, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(amoid));

    scan = systable_beginscan(rel, OpclassAmNameNspIndexId, true, NULL, 1, skey);

    while (HeapTupleIsValid(tup = systable_getnext(scan)))
    {
      Form_pg_opclass opclass = (Form_pg_opclass)GETSTRUCT(tup);

      if (opclass->opcintype == typeoid && opclass->opcdefault)
      {
        ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("could not make operator class \"%s\" be default for type %s", opcname, TypeNameToString(stmt->datatype)), errdetail("Operator class \"%s\" already is the default.", NameStr(opclass->opcname))));
      }
    }

    systable_endscan(scan);
  }

     
                                              
     
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));

  opclassoid = GetNewOidWithIndex(rel, OpclassOidIndexId, Anum_pg_opclass_oid);
  values[Anum_pg_opclass_oid - 1] = ObjectIdGetDatum(opclassoid);
  values[Anum_pg_opclass_opcmethod - 1] = ObjectIdGetDatum(amoid);
  namestrcpy(&opcName, opcname);
  values[Anum_pg_opclass_opcname - 1] = NameGetDatum(&opcName);
  values[Anum_pg_opclass_opcnamespace - 1] = ObjectIdGetDatum(namespaceoid);
  values[Anum_pg_opclass_opcowner - 1] = ObjectIdGetDatum(GetUserId());
  values[Anum_pg_opclass_opcfamily - 1] = ObjectIdGetDatum(opfamilyoid);
  values[Anum_pg_opclass_opcintype - 1] = ObjectIdGetDatum(typeoid);
  values[Anum_pg_opclass_opcdefault - 1] = BoolGetDatum(stmt->isDefault);
  values[Anum_pg_opclass_opckeytype - 1] = ObjectIdGetDatum(storageoid);

  tup = heap_form_tuple(rel->rd_att, values, nulls);

  CatalogTupleInsert(rel, tup);

  heap_freetuple(tup);

     
                                                                        
                                                         
     
  storeOperators(stmt->opfamilyname, amoid, opfamilyoid, opclassoid, operators, false);
  storeProcedures(stmt->opfamilyname, amoid, opfamilyoid, opclassoid, procedures, false);

                                             
  EventTriggerCollectCreateOpClass(stmt, opclassoid, operators, procedures);

     
                                                                         
                                                                          
     
  myself.classId = OperatorClassRelationId;
  myself.objectId = opclassoid;
  myself.objectSubId = 0;

                               
  referenced.classId = NamespaceRelationId;
  referenced.objectId = namespaceoid;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                              
  referenced.classId = OperatorFamilyRelationId;
  referenced.objectId = opfamilyoid;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_AUTO);

                                      
  referenced.classId = TypeRelationId;
  referenced.objectId = typeoid;
  referenced.objectSubId = 0;
  recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                      
  if (OidIsValid(storageoid))
  {
    referenced.classId = TypeRelationId;
    referenced.objectId = storageoid;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
  }

                           
  recordDependencyOnOwner(OperatorClassRelationId, opclassoid, GetUserId());

                               
  recordDependencyOnCurrentExtension(&myself, false);

                                                 
  InvokeObjectPostCreateHook(OperatorClassRelationId, opclassoid, 0);

  table_close(rel, RowExclusiveLock);

  return myself;
}

   
                  
                                        
   
ObjectAddress
DefineOpFamily(CreateOpFamilyStmt *stmt)
{
  char *opfname;                                         
  Oid amoid,                          
      namespaceoid;                                      
  AclResult aclresult;

                                                     
  namespaceoid = QualifiedNameGetCreationNamespace(stmt->opfamilyname, &opfname);

                                                         
  aclresult = pg_namespace_aclcheck(namespaceoid, GetUserId(), ACL_CREATE);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, OBJECT_SCHEMA, get_namespace_name(namespaceoid));
  }

                                                                     
  amoid = get_index_am_oid(stmt->amname, false);

                                                              

     
                                                                           
                                
     
  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to create an operator family")));
  }

                                        
  return CreateOpFamily(stmt, opfname, namespaceoid, amoid);
}

   
                 
                                                                           
   
                                                                        
                                                                     
                         
   
Oid
AlterOpFamily(AlterOpFamilyStmt *stmt)
{
  Oid amoid,                           
      opfamilyoid;                        
  int maxOpNumber,                           
      maxProcNumber;                      
  HeapTuple tup;
  Form_pg_am amform;
  IndexAmRoutine *amroutine;

                                              
  tup = SearchSysCache1(AMNAME, CStringGetDatum(stmt->amname));
  if (!HeapTupleIsValid(tup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("access method \"%s\" does not exist", stmt->amname)));
  }

  amform = (Form_pg_am)GETSTRUCT(tup);
  amoid = amform->oid;
  amroutine = GetIndexAmRoutineByAmId(amoid, false);
  ReleaseSysCache(tup);

  maxOpNumber = amroutine->amstrategies;
                                                                          
  if (maxOpNumber <= 0)
  {
    maxOpNumber = SHRT_MAX;
  }
  maxProcNumber = amroutine->amsupport;

                                                              

                            
  opfamilyoid = get_opfamily_oid(amoid, stmt->opfamilyname, false);

     
                                                                      
     
                                                                         
     
  if (!superuser())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("must be superuser to alter an operator family")));
  }

     
                                                              
     
  if (stmt->isDrop)
  {
    AlterOpFamilyDrop(stmt, amoid, opfamilyoid, maxOpNumber, maxProcNumber, stmt->items);
  }
  else
  {
    AlterOpFamilyAdd(stmt, amoid, opfamilyoid, maxOpNumber, maxProcNumber, stmt->items);
  }

  return opfamilyoid;
}

   
                               
   
static void
AlterOpFamilyAdd(AlterOpFamilyStmt *stmt, Oid amoid, Oid opfamilyoid, int maxOpNumber, int maxProcNumber, List *items)
{
  List *operators;                                         
  List *procedures;                                            
  ListCell *l;

  operators = NIL;
  procedures = NIL;

     
                                                      
     
  foreach (l, items)
  {
    CreateOpClassItem *item = lfirst_node(CreateOpClassItem, l);
    Oid operOid;
    Oid funcOid;
    Oid sortfamilyOid;
    OpFamilyMember *member;

    switch (item->itemtype)
    {
    case OPCLASS_ITEM_OPERATOR:
      if (item->number <= 0 || item->number > maxOpNumber)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("invalid operator number %d,"
                                                                           " must be between 1 and %d",
                                                                        item->number, maxOpNumber)));
      }
      if (item->name->objargs != NIL)
      {
        operOid = LookupOperWithArgs(item->name, false);
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("operator argument types must be specified in ALTER OPERATOR FAMILY")));
        operOid = InvalidOid;                          
      }

      if (item->order_family)
      {
        sortfamilyOid = get_opfamily_oid(BTREE_AM_OID, item->order_family, false);
      }
      else
      {
        sortfamilyOid = InvalidOid;
      }

#ifdef NOT_USED
                                                                   
                                                                
      if (!pg_oper_ownercheck(operOid, GetUserId()))
      {
        aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_OPERATOR, get_opname(operOid));
      }
      funcOid = get_opcode(operOid);
      if (!pg_proc_ownercheck(funcOid, GetUserId()))
      {
        aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_FUNCTION, get_func_name(funcOid));
      }
#endif

                         
      member = (OpFamilyMember *)palloc0(sizeof(OpFamilyMember));
      member->object = operOid;
      member->number = item->number;
      member->sortfamily = sortfamilyOid;
      assignOperTypes(member, amoid, InvalidOid);
      addFamilyMember(&operators, member, false);
      break;
    case OPCLASS_ITEM_FUNCTION:
      if (item->number <= 0 || item->number > maxProcNumber)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("invalid function number %d,"
                                                                           " must be between 1 and %d",
                                                                        item->number, maxProcNumber)));
      }
      funcOid = LookupFuncWithArgs(OBJECT_FUNCTION, item->name, false);
#ifdef NOT_USED
                                                                   
                                    
      if (!pg_proc_ownercheck(funcOid, GetUserId()))
      {
        aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_FUNCTION, get_func_name(funcOid));
      }
#endif

                         
      member = (OpFamilyMember *)palloc0(sizeof(OpFamilyMember));
      member->object = funcOid;
      member->number = item->number;

                                                               
      if (item->class_args)
      {
        processTypesSpec(item->class_args, &member->lefttype, &member->righttype);
      }

      assignProcTypes(member, amoid, InvalidOid);
      addFamilyMember(&procedures, member, true);
      break;
    case OPCLASS_ITEM_STORAGETYPE:
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("STORAGE cannot be specified in ALTER OPERATOR FAMILY")));
      break;
    default:
      elog(ERROR, "unrecognized item type: %d", item->itemtype);
      break;
    }
  }

     
                                                                    
                                                         
     
  storeOperators(stmt->opfamilyname, amoid, opfamilyoid, InvalidOid, operators, true);
  storeProcedures(stmt->opfamilyname, amoid, opfamilyoid, InvalidOid, procedures, true);

                                                    
  EventTriggerCollectAlterOpFam(stmt, opfamilyoid, operators, procedures);
}

   
                                
   
static void
AlterOpFamilyDrop(AlterOpFamilyStmt *stmt, Oid amoid, Oid opfamilyoid, int maxOpNumber, int maxProcNumber, List *items)
{
  List *operators;                                         
  List *procedures;                                            
  ListCell *l;

  operators = NIL;
  procedures = NIL;

     
                                                      
     
  foreach (l, items)
  {
    CreateOpClassItem *item = lfirst_node(CreateOpClassItem, l);
    Oid lefttype, righttype;
    OpFamilyMember *member;

    switch (item->itemtype)
    {
    case OPCLASS_ITEM_OPERATOR:
      if (item->number <= 0 || item->number > maxOpNumber)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("invalid operator number %d,"
                                                                           " must be between 1 and %d",
                                                                        item->number, maxOpNumber)));
      }
      processTypesSpec(item->class_args, &lefttype, &righttype);
                         
      member = (OpFamilyMember *)palloc0(sizeof(OpFamilyMember));
      member->number = item->number;
      member->lefttype = lefttype;
      member->righttype = righttype;
      addFamilyMember(&operators, member, false);
      break;
    case OPCLASS_ITEM_FUNCTION:
      if (item->number <= 0 || item->number > maxProcNumber)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("invalid function number %d,"
                                                                           " must be between 1 and %d",
                                                                        item->number, maxProcNumber)));
      }
      processTypesSpec(item->class_args, &lefttype, &righttype);
                         
      member = (OpFamilyMember *)palloc0(sizeof(OpFamilyMember));
      member->number = item->number;
      member->lefttype = lefttype;
      member->righttype = righttype;
      addFamilyMember(&procedures, member, true);
      break;
    case OPCLASS_ITEM_STORAGETYPE:
                                                
    default:
      elog(ERROR, "unrecognized item type: %d", item->itemtype);
      break;
    }
  }

     
                                               
     
  dropOperators(stmt->opfamilyname, amoid, opfamilyoid, operators);
  dropProcedures(stmt->opfamilyname, amoid, opfamilyoid, procedures);

                                                    
  EventTriggerCollectAlterOpFam(stmt, opfamilyoid, operators, procedures);
}

   
                                                       
   
static void
processTypesSpec(List *args, Oid *lefttype, Oid *righttype)
{
  TypeName *typeName;

  Assert(args != NIL);

  typeName = (TypeName *)linitial(args);
  *lefttype = typenameTypeId(NULL, typeName);

  if (list_length(args) > 1)
  {
    typeName = (TypeName *)lsecond(args);
    *righttype = typenameTypeId(NULL, typeName);
  }
  else
  {
    *righttype = *lefttype;
  }

  if (list_length(args) > 2)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("one or two argument types must be specified")));
  }
}

   
                                                              
                                               
   
static void
assignOperTypes(OpFamilyMember *member, Oid amoid, Oid typeoid)
{
  Operator optup;
  Form_pg_operator opform;

                                     
  optup = SearchSysCache1(OPEROID, ObjectIdGetDatum(member->object));
  if (!HeapTupleIsValid(optup))
  {
    elog(ERROR, "cache lookup failed for operator %u", member->object);
  }
  opform = (Form_pg_operator)GETSTRUCT(optup);

     
                                        
     
  if (opform->oprkind != 'b')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("index operators must be binary")));
  }

  if (OidIsValid(member->sortfamily))
  {
       
                                                                       
                                                                           
                                                                          
                                                                           
                                                                       
                                                                        
                                                                           
                   
       
    IndexAmRoutine *amroutine = GetIndexAmRoutineByAmId(amoid, false);

    if (!amroutine->amcanorderbyop)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("access method \"%s\" does not support ordering operators", get_am_name(amoid))));
    }
  }
  else
  {
       
                                             
       
    if (opform->oprresult != BOOLOID)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("index search operators must return boolean")));
    }
  }

     
                                                                           
     
  if (!OidIsValid(member->lefttype))
  {
    member->lefttype = opform->oprleft;
  }
  if (!OidIsValid(member->righttype))
  {
    member->righttype = opform->oprright;
  }

  ReleaseSysCache(optup);
}

   
                                                                      
                                               
   
static void
assignProcTypes(OpFamilyMember *member, Oid amoid, Oid typeoid)
{
  HeapTuple proctup;
  Form_pg_proc procform;

                                      
  proctup = SearchSysCache1(PROCOID, ObjectIdGetDatum(member->object));
  if (!HeapTupleIsValid(proctup))
  {
    elog(ERROR, "cache lookup failed for function %u", member->object);
  }
  procform = (Form_pg_proc)GETSTRUCT(proctup);

     
                                                                       
                                                                           
                                                                            
                                                                    
                                               
     
  if (amoid == BTREE_AM_OID)
  {
    if (member->number == BTORDER_PROC)
    {
      if (procform->pronargs != 2)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("btree comparison functions must have two arguments")));
      }
      if (procform->prorettype != INT4OID)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("btree comparison functions must return integer")));
      }

         
                                                                     
               
         
      if (!OidIsValid(member->lefttype))
      {
        member->lefttype = procform->proargtypes.values[0];
      }
      if (!OidIsValid(member->righttype))
      {
        member->righttype = procform->proargtypes.values[1];
      }
    }
    else if (member->number == BTSORTSUPPORT_PROC)
    {
      if (procform->pronargs != 1 || procform->proargtypes.values[0] != INTERNALOID)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("btree sort support functions must accept type \"internal\"")));
      }
      if (procform->prorettype != VOIDOID)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("btree sort support functions must return void")));
      }

         
                                                                       
         
    }
    else if (member->number == BTINRANGE_PROC)
    {
      if (procform->pronargs != 5)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("btree in_range functions must have five arguments")));
      }
      if (procform->prorettype != BOOLOID)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("btree in_range functions must return boolean")));
      }

         
                                                                     
                                                                
         
      if (!OidIsValid(member->lefttype))
      {
        member->lefttype = procform->proargtypes.values[0];
      }
      if (!OidIsValid(member->righttype))
      {
        member->righttype = procform->proargtypes.values[2];
      }
    }
  }
  else if (amoid == HASH_AM_OID)
  {
    if (member->number == HASHSTANDARD_PROC)
    {
      if (procform->pronargs != 1)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("hash function 1 must have one argument")));
      }
      if (procform->prorettype != INT4OID)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("hash function 1 must return integer")));
      }
    }
    else if (member->number == HASHEXTENDED_PROC)
    {
      if (procform->pronargs != 2)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("hash function 2 must have two arguments")));
      }
      if (procform->prorettype != INT8OID)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("hash function 2 must return bigint")));
      }
    }

       
                                                                        
       
    if (!OidIsValid(member->lefttype))
    {
      member->lefttype = procform->proargtypes.values[0];
    }
    if (!OidIsValid(member->righttype))
    {
      member->righttype = procform->proargtypes.values[0];
    }
  }

     
                                                                            
                                                                            
                                                          
     
  if (!OidIsValid(member->lefttype))
  {
    member->lefttype = typeoid;
  }
  if (!OidIsValid(member->righttype))
  {
    member->righttype = typeoid;
  }

  if (!OidIsValid(member->lefttype) || !OidIsValid(member->righttype))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("associated data types must be specified for index support function")));
  }

  ReleaseSysCache(proctup);
}

   
                                                                       
                                       
   
static void
addFamilyMember(List **list, OpFamilyMember *member, bool isProc)
{
  ListCell *l;

  foreach (l, *list)
  {
    OpFamilyMember *old = (OpFamilyMember *)lfirst(l);

    if (old->number == member->number && old->lefttype == member->lefttype && old->righttype == member->righttype)
    {
      if (isProc)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("function number %d for (%s,%s) appears more than once", member->number, format_type_be(member->lefttype), format_type_be(member->righttype))));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("operator number %d for (%s,%s) appears more than once", member->number, format_type_be(member->lefttype), format_type_be(member->righttype))));
      }
    }
  }
  *list = lappend(*list, member);
}

   
                                 
   
                                                                          
                                                                            
                                                 
   
static void
storeOperators(List *opfamilyname, Oid amoid, Oid opfamilyoid, Oid opclassoid, List *operators, bool isAdd)
{
  Relation rel;
  Datum values[Natts_pg_amop];
  bool nulls[Natts_pg_amop];
  HeapTuple tup;
  Oid entryoid;
  ObjectAddress myself, referenced;
  ListCell *l;

  rel = table_open(AccessMethodOperatorRelationId, RowExclusiveLock);

  foreach (l, operators)
  {
    OpFamilyMember *op = (OpFamilyMember *)lfirst(l);
    char oppurpose;

       
                                                                   
                                                                   
       
    if (isAdd && SearchSysCacheExists4(AMOPSTRATEGY, ObjectIdGetDatum(opfamilyoid), ObjectIdGetDatum(op->lefttype), ObjectIdGetDatum(op->righttype), Int16GetDatum(op->number)))
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("operator %d(%s,%s) already exists in operator family \"%s\"", op->number, format_type_be(op->lefttype), format_type_be(op->righttype), NameListToString(opfamilyname))));
    }

    oppurpose = OidIsValid(op->sortfamily) ? AMOP_ORDER : AMOP_SEARCH;

                                  
    memset(values, 0, sizeof(values));
    memset(nulls, false, sizeof(nulls));

    entryoid = GetNewOidWithIndex(rel, AccessMethodOperatorOidIndexId, Anum_pg_amop_oid);
    values[Anum_pg_amop_oid - 1] = ObjectIdGetDatum(entryoid);
    values[Anum_pg_amop_amopfamily - 1] = ObjectIdGetDatum(opfamilyoid);
    values[Anum_pg_amop_amoplefttype - 1] = ObjectIdGetDatum(op->lefttype);
    values[Anum_pg_amop_amoprighttype - 1] = ObjectIdGetDatum(op->righttype);
    values[Anum_pg_amop_amopstrategy - 1] = Int16GetDatum(op->number);
    values[Anum_pg_amop_amoppurpose - 1] = CharGetDatum(oppurpose);
    values[Anum_pg_amop_amopopr - 1] = ObjectIdGetDatum(op->object);
    values[Anum_pg_amop_amopmethod - 1] = ObjectIdGetDatum(amoid);
    values[Anum_pg_amop_amopsortfamily - 1] = ObjectIdGetDatum(op->sortfamily);

    tup = heap_form_tuple(rel->rd_att, values, nulls);

    CatalogTupleInsert(rel, tup);

    heap_freetuple(tup);

                               
    myself.classId = AccessMethodOperatorRelationId;
    myself.objectId = entryoid;
    myself.objectSubId = 0;

    referenced.classId = OperatorRelationId;
    referenced.objectId = op->object;
    referenced.objectSubId = 0;

    if (OidIsValid(opclassoid))
    {
                                                                    
      recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                                  
      referenced.classId = OperatorClassRelationId;
      referenced.objectId = opclassoid;
      referenced.objectSubId = 0;
      recordDependencyOn(&myself, &referenced, DEPENDENCY_INTERNAL);
    }
    else
    {
                                                                  
      recordDependencyOn(&myself, &referenced, DEPENDENCY_AUTO);

                                               
      referenced.classId = OperatorFamilyRelationId;
      referenced.objectId = opfamilyoid;
      referenced.objectSubId = 0;
      recordDependencyOn(&myself, &referenced, DEPENDENCY_AUTO);
    }

                                                                       
    if (OidIsValid(op->sortfamily))
    {
      referenced.classId = OperatorFamilyRelationId;
      referenced.objectId = op->sortfamily;
      referenced.objectSubId = 0;
      recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
    }
                                                         
    InvokeObjectPostCreateHook(AccessMethodOperatorRelationId, entryoid, 0);
  }

  table_close(rel, RowExclusiveLock);
}

   
                                                       
   
                                                                          
                                                                            
                                                 
   
static void
storeProcedures(List *opfamilyname, Oid amoid, Oid opfamilyoid, Oid opclassoid, List *procedures, bool isAdd)
{
  Relation rel;
  Datum values[Natts_pg_amproc];
  bool nulls[Natts_pg_amproc];
  HeapTuple tup;
  Oid entryoid;
  ObjectAddress myself, referenced;
  ListCell *l;

  rel = table_open(AccessMethodProcedureRelationId, RowExclusiveLock);

  foreach (l, procedures)
  {
    OpFamilyMember *proc = (OpFamilyMember *)lfirst(l);

       
                                                                   
                                                                     
       
    if (isAdd && SearchSysCacheExists4(AMPROCNUM, ObjectIdGetDatum(opfamilyoid), ObjectIdGetDatum(proc->lefttype), ObjectIdGetDatum(proc->righttype), Int16GetDatum(proc->number)))
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("function %d(%s,%s) already exists in operator family \"%s\"", proc->number, format_type_be(proc->lefttype), format_type_be(proc->righttype), NameListToString(opfamilyname))));
    }

                                    
    memset(values, 0, sizeof(values));
    memset(nulls, false, sizeof(nulls));

    entryoid = GetNewOidWithIndex(rel, AccessMethodProcedureOidIndexId, Anum_pg_amproc_oid);
    values[Anum_pg_amproc_oid - 1] = ObjectIdGetDatum(entryoid);
    values[Anum_pg_amproc_amprocfamily - 1] = ObjectIdGetDatum(opfamilyoid);
    values[Anum_pg_amproc_amproclefttype - 1] = ObjectIdGetDatum(proc->lefttype);
    values[Anum_pg_amproc_amprocrighttype - 1] = ObjectIdGetDatum(proc->righttype);
    values[Anum_pg_amproc_amprocnum - 1] = Int16GetDatum(proc->number);
    values[Anum_pg_amproc_amproc - 1] = ObjectIdGetDatum(proc->object);

    tup = heap_form_tuple(rel->rd_att, values, nulls);

    CatalogTupleInsert(rel, tup);

    heap_freetuple(tup);

                               
    myself.classId = AccessMethodProcedureRelationId;
    myself.objectId = entryoid;
    myself.objectSubId = 0;

    referenced.classId = ProcedureRelationId;
    referenced.objectId = proc->object;
    referenced.objectSubId = 0;

    if (OidIsValid(opclassoid))
    {
                                                                     
      recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                                  
      referenced.classId = OperatorClassRelationId;
      referenced.objectId = opclassoid;
      referenced.objectSubId = 0;
      recordDependencyOn(&myself, &referenced, DEPENDENCY_INTERNAL);
    }
    else
    {
                                                                   
      recordDependencyOn(&myself, &referenced, DEPENDENCY_AUTO);

                                               
      referenced.classId = OperatorFamilyRelationId;
      referenced.objectId = opfamilyoid;
      referenced.objectSubId = 0;
      recordDependencyOn(&myself, &referenced, DEPENDENCY_AUTO);
    }
                                                     
    InvokeObjectPostCreateHook(AccessMethodProcedureRelationId, entryoid, 0);
  }

  table_close(rel, RowExclusiveLock);
}

   
                                             
   
                                                                        
                                
   
static void
dropOperators(List *opfamilyname, Oid amoid, Oid opfamilyoid, List *operators)
{
  ListCell *l;

  foreach (l, operators)
  {
    OpFamilyMember *op = (OpFamilyMember *)lfirst(l);
    Oid amopid;
    ObjectAddress object;

    amopid = GetSysCacheOid4(AMOPSTRATEGY, Anum_pg_amop_oid, ObjectIdGetDatum(opfamilyoid), ObjectIdGetDatum(op->lefttype), ObjectIdGetDatum(op->righttype), Int16GetDatum(op->number));
    if (!OidIsValid(amopid))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("operator %d(%s,%s) does not exist in operator family \"%s\"", op->number, format_type_be(op->lefttype), format_type_be(op->righttype), NameListToString(opfamilyname))));
    }

    object.classId = AccessMethodOperatorRelationId;
    object.objectId = amopid;
    object.objectSubId = 0;

    performDeletion(&object, DROP_RESTRICT, 0);
  }
}

   
                                              
   
                                                                        
                                
   
static void
dropProcedures(List *opfamilyname, Oid amoid, Oid opfamilyoid, List *procedures)
{
  ListCell *l;

  foreach (l, procedures)
  {
    OpFamilyMember *op = (OpFamilyMember *)lfirst(l);
    Oid amprocid;
    ObjectAddress object;

    amprocid = GetSysCacheOid4(AMPROCNUM, Anum_pg_amproc_oid, ObjectIdGetDatum(opfamilyoid), ObjectIdGetDatum(op->lefttype), ObjectIdGetDatum(op->righttype), Int16GetDatum(op->number));
    if (!OidIsValid(amprocid))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("function %d(%s,%s) does not exist in operator family \"%s\"", op->number, format_type_be(op->lefttype), format_type_be(op->righttype), NameListToString(opfamilyname))));
    }

    object.classId = AccessMethodProcedureRelationId;
    object.objectId = amprocid;
    object.objectSubId = 0;

    performDeletion(&object, DROP_RESTRICT, 0);
  }
}

   
                                                 
   
void
RemoveOpFamilyById(Oid opfamilyOid)
{
  Relation rel;
  HeapTuple tup;

  rel = table_open(OperatorFamilyRelationId, RowExclusiveLock);

  tup = SearchSysCache1(OPFAMILYOID, ObjectIdGetDatum(opfamilyOid));
  if (!HeapTupleIsValid(tup))                        
  {
    elog(ERROR, "cache lookup failed for opfamily %u", opfamilyOid);
  }

  CatalogTupleDelete(rel, &tup->t_self);

  ReleaseSysCache(tup);

  table_close(rel, RowExclusiveLock);
}

void
RemoveOpClassById(Oid opclassOid)
{
  Relation rel;
  HeapTuple tup;

  rel = table_open(OperatorClassRelationId, RowExclusiveLock);

  tup = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclassOid));
  if (!HeapTupleIsValid(tup))                        
  {
    elog(ERROR, "cache lookup failed for opclass %u", opclassOid);
  }

  CatalogTupleDelete(rel, &tup->t_self);

  ReleaseSysCache(tup);

  table_close(rel, RowExclusiveLock);
}

void
RemoveAmOpEntryById(Oid entryOid)
{
  Relation rel;
  HeapTuple tup;
  ScanKeyData skey[1];
  SysScanDesc scan;

  ScanKeyInit(&skey[0], Anum_pg_amop_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(entryOid));

  rel = table_open(AccessMethodOperatorRelationId, RowExclusiveLock);

  scan = systable_beginscan(rel, AccessMethodOperatorOidIndexId, true, NULL, 1, skey);

                                   
  tup = systable_getnext(scan);
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "could not find tuple for amop entry %u", entryOid);
  }

  CatalogTupleDelete(rel, &tup->t_self);

  systable_endscan(scan);
  table_close(rel, RowExclusiveLock);
}

void
RemoveAmProcEntryById(Oid entryOid)
{
  Relation rel;
  HeapTuple tup;
  ScanKeyData skey[1];
  SysScanDesc scan;

  ScanKeyInit(&skey[0], Anum_pg_amproc_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(entryOid));

  rel = table_open(AccessMethodProcedureRelationId, RowExclusiveLock);

  scan = systable_beginscan(rel, AccessMethodProcedureOidIndexId, true, NULL, 1, skey);

                                   
  tup = systable_getnext(scan);
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "could not find tuple for amproc entry %u", entryOid);
  }

  CatalogTupleDelete(rel, &tup->t_self);

  systable_endscan(scan);
  table_close(rel, RowExclusiveLock);
}

   
                                                         
   
                                                                        
                                                                      
   
void
IsThereOpClassInNamespace(const char *opcname, Oid opcmethod, Oid opcnamespace)
{
                                            
  if (SearchSysCacheExists3(CLAAMNAMENSP, ObjectIdGetDatum(opcmethod), CStringGetDatum(opcname), ObjectIdGetDatum(opcnamespace)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("operator class \"%s\" for access method \"%s\" already exists in schema \"%s\"", opcname, get_am_name(opcmethod), get_namespace_name(opcnamespace))));
  }
}

   
                                                          
   
                                                                         
                                                                      
   
void
IsThereOpFamilyInNamespace(const char *opfname, Oid opfmethod, Oid opfnamespace)
{
                                            
  if (SearchSysCacheExists3(OPFAMILYAMNAMENSP, ObjectIdGetDatum(opfmethod), CStringGetDatum(opfname), ObjectIdGetDatum(opfnamespace)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("operator family \"%s\" for access method \"%s\" already exists in schema \"%s\"", opfname, get_am_name(opfmethod), get_namespace_name(opfnamespace))));
  }
}
