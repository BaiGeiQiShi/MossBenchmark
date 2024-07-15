                                                                            
   
          
                                                        
   
                                                                         
                                                                        
   
   
                  
                                
   
   
                      
                                                          
                                                             
                                                                    
   
         
                                                               
                                                                
                                                                     
              
                                                           
                                                                 
                 
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/multixact.h"
#include "access/relation.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/objectaccess.h"
#include "catalog/partition.h"
#include "catalog/pg_am.h"
#include "catalog/pg_attrdef.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_partitioned_table.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_subscription_rel.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "catalog/storage.h"
#include "catalog/storage_xlog.h"
#include "commands/tablecmds.h"
#include "commands/typecmds.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/parse_coerce.h"
#include "parser/parse_collate.h"
#include "parser/parse_expr.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "partitioning/partdesc.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "storage/smgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/ruleutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

                                                     
Oid binary_upgrade_next_heap_pg_class_oid = InvalidOid;
Oid binary_upgrade_next_toast_pg_class_oid = InvalidOid;

static void
AddNewRelationTuple(Relation pg_class_desc, Relation new_rel_desc, Oid new_rel_oid, Oid new_type_oid, Oid reloftype, Oid relowner, char relkind, TransactionId relfrozenxid, TransactionId relminmxid, Datum relacl, Datum reloptions);
static ObjectAddress
AddNewRelationType(const char *typeName, Oid typeNamespace, Oid new_rel_oid, char new_rel_kind, Oid ownerid, Oid new_row_type, Oid new_array_type);
static void
RelationRemoveInheritance(Oid relid);
static Oid
StoreRelCheck(Relation rel, const char *ccname, Node *expr, bool is_validated, bool is_local, int inhcount, bool is_no_inherit, bool is_internal);
static void
StoreConstraints(Relation rel, List *cooked_constraints, bool is_internal);
static bool
MergeWithExistingConstraint(Relation rel, const char *ccname, Node *expr, bool allow_merge, bool is_local, bool is_initially_valid, bool is_no_inherit);
static void
SetRelationNumChecks(Relation rel, int numchecks);
static Node *
cookConstraint(ParseState *pstate, Node *raw_constraint, char *relname);
static List *
insert_ordered_unique_oid(List *list, Oid datum);

                                                                    
                                              
   
                                                              
                                      
                                                                    
   

   
         
                                                                   
                                                                  
                                                             
   

   
                                                                          
                                                                      
                                               
   

static const FormData_pg_attribute a1 = {
    .attname = {"ctid"},
    .atttypid = TIDOID,
    .attlen = sizeof(ItemPointerData),
    .attnum = SelfItemPointerAttributeNumber,
    .attcacheoff = -1,
    .atttypmod = -1,
    .attbyval = false,
    .attstorage = 'p',
    .attalign = 's',
    .attnotnull = true,
    .attislocal = true,
};

static const FormData_pg_attribute a2 = {
    .attname = {"xmin"},
    .atttypid = XIDOID,
    .attlen = sizeof(TransactionId),
    .attnum = MinTransactionIdAttributeNumber,
    .attcacheoff = -1,
    .atttypmod = -1,
    .attbyval = true,
    .attstorage = 'p',
    .attalign = 'i',
    .attnotnull = true,
    .attislocal = true,
};

static const FormData_pg_attribute a3 = {
    .attname = {"cmin"},
    .atttypid = CIDOID,
    .attlen = sizeof(CommandId),
    .attnum = MinCommandIdAttributeNumber,
    .attcacheoff = -1,
    .atttypmod = -1,
    .attbyval = true,
    .attstorage = 'p',
    .attalign = 'i',
    .attnotnull = true,
    .attislocal = true,
};

static const FormData_pg_attribute a4 = {
    .attname = {"xmax"},
    .atttypid = XIDOID,
    .attlen = sizeof(TransactionId),
    .attnum = MaxTransactionIdAttributeNumber,
    .attcacheoff = -1,
    .atttypmod = -1,
    .attbyval = true,
    .attstorage = 'p',
    .attalign = 'i',
    .attnotnull = true,
    .attislocal = true,
};

static const FormData_pg_attribute a5 = {
    .attname = {"cmax"},
    .atttypid = CIDOID,
    .attlen = sizeof(CommandId),
    .attnum = MaxCommandIdAttributeNumber,
    .attcacheoff = -1,
    .atttypmod = -1,
    .attbyval = true,
    .attstorage = 'p',
    .attalign = 'i',
    .attnotnull = true,
    .attislocal = true,
};

   
                                                                
                                                                         
                                                                         
                
   
static const FormData_pg_attribute a6 = {
    .attname = {"tableoid"},
    .atttypid = OIDOID,
    .attlen = sizeof(Oid),
    .attnum = TableOidAttributeNumber,
    .attcacheoff = -1,
    .atttypmod = -1,
    .attbyval = true,
    .attstorage = 'p',
    .attalign = 'i',
    .attnotnull = true,
    .attislocal = true,
};

static const FormData_pg_attribute *SysAtt[] = {&a1, &a2, &a3, &a4, &a5, &a6};

   
                                                                             
                                                                         
                                         
   
const FormData_pg_attribute *
SystemAttributeDefinition(AttrNumber attno)
{
  if (attno >= 0 || attno < -(int)lengthof(SysAtt))
  {
    elog(ERROR, "invalid system attribute number %d", attno);
  }
  return SysAtt[-attno - 1];
}

   
                                                                            
                                                             
   
const FormData_pg_attribute *
SystemAttributeByName(const char *attname)
{
  int j;

  for (j = 0; j < (int)lengthof(SysAtt); j++)
  {
    const FormData_pg_attribute *att = SysAtt[j];

    if (strcmp(NameStr(att->attname), attname) == 0)
    {
      return att;
    }
  }

  return NULL;
}

                                                                    
                                             
                                                                      

                                                                    
                                                       
   
                                                                
                                                                  
                                 
   
                                                              
                                    
                                                                    
   
Relation
heap_create(const char *relname, Oid relnamespace, Oid reltablespace, Oid relid, Oid relfilenode, Oid accessmtd, TupleDesc tupDesc, char relkind, char relpersistence, bool shared_relation, bool mapped_relation, bool allow_system_table_mods, TransactionId *relfrozenxid, MultiXactId *relminmxid)
{
  bool create_storage;
  Relation rel;

                                                              
  Assert(OidIsValid(relid));

     
                                                                           
                                                                            
                                                           
     
                                                                   
                                                                             
                                              
     
  if (!allow_system_table_mods && ((IsCatalogNamespace(relnamespace) && relkind != RELKIND_INDEX) || IsToastNamespace(relnamespace)) && IsNormalProcessingMode())
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to create \"%s.%s\"", get_namespace_name(relnamespace), relname), errdetail("System catalog modifications are currently disallowed.")));
  }

  *relfrozenxid = InvalidTransactionId;
  *relminmxid = InvalidMultiXactId;

                                                   
  switch (relkind)
  {
  case RELKIND_VIEW:
  case RELKIND_COMPOSITE_TYPE:
  case RELKIND_FOREIGN_TABLE:

       
                                                                   
                                                            
       
                                                                  
                                                                     
                                      
       
    reltablespace = InvalidOid;
    break;

  case RELKIND_SEQUENCE:

       
                                                                 
                                                              
       
    reltablespace = InvalidOid;
    break;
  default:
    break;
  }

     
                                                                             
                                                                            
                                            
     
  if (!RELKIND_HAS_STORAGE(relkind) || OidIsValid(relfilenode))
  {
    create_storage = false;
  }
  else
  {
    create_storage = true;
    relfilenode = relid;
  }

     
                                                                       
                                                                         
                                                                     
                                                                           
                                              
     
                                   
     
  if (reltablespace == MyDatabaseTableSpace)
  {
    reltablespace = InvalidOid;
  }

     
                               
     
  rel = RelationBuildLocalRelation(relname, relnamespace, tupDesc, relid, accessmtd, relfilenode, reltablespace, shared_relation, mapped_relation, relpersistence, relkind);

     
                                                                          
     
                                                                             
                                                                            
                
     
  if (create_storage)
  {
    switch (rel->rd_rel->relkind)
    {
    case RELKIND_VIEW:
    case RELKIND_COMPOSITE_TYPE:
    case RELKIND_FOREIGN_TABLE:
    case RELKIND_PARTITIONED_TABLE:
    case RELKIND_PARTITIONED_INDEX:
      Assert(false);
      break;

    case RELKIND_INDEX:
    case RELKIND_SEQUENCE:
      RelationCreateStorage(rel->rd_node, relpersistence);
      break;

    case RELKIND_RELATION:
    case RELKIND_TOASTVALUE:
    case RELKIND_MATVIEW:
      table_relation_set_new_filenode(rel, &rel->rd_node, relpersistence, relfrozenxid, relminmxid);
      break;
    }
  }

     
                                                                          
                                                                           
                                                            
     
  if (!create_storage && reltablespace != InvalidOid)
  {
    recordDependencyOnTablespace(RelationRelationId, relid, reltablespace);
  }

  return rel;
}

                                                                    
                                                            
   
                                    
   
                                                                    
                                                                    
   
                                                  
                                                           
                                 
   
                                                                   
   
                                                                 
                            
   
                                                       
                             
   
                                                         
                                              
   
                                                       
   
                                                           
                    
   
                                                                    
   

                                    
                             
   
                                                                 
                                                                  
                                                                   
   
                                                          
                                                                       
                                    
   
void
CheckAttributeNamesTypes(TupleDesc tupdesc, char relkind, int flags)
{
  int i;
  int j;
  int natts = tupdesc->natts;

                                    
  if (natts < 0 || natts > MaxHeapAttributeNumber)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("tables can have at most %d columns", MaxHeapAttributeNumber)));
  }

     
                                                           
     
                                                                          
                 
     
  if (relkind != RELKIND_VIEW && relkind != RELKIND_COMPOSITE_TYPE)
  {
    for (i = 0; i < natts; i++)
    {
      Form_pg_attribute attr = TupleDescAttr(tupdesc, i);

      if (SystemAttributeByName(NameStr(attr->attname)) != NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_DUPLICATE_COLUMN), errmsg("column name \"%s\" conflicts with a system column name", NameStr(attr->attname))));
      }
    }
  }

     
                                             
     
  for (i = 1; i < natts; i++)
  {
    for (j = 0; j < i; j++)
    {
      if (strcmp(NameStr(TupleDescAttr(tupdesc, j)->attname), NameStr(TupleDescAttr(tupdesc, i)->attname)) == 0)
      {
        ereport(ERROR, (errcode(ERRCODE_DUPLICATE_COLUMN), errmsg("column name \"%s\" specified more than once", NameStr(TupleDescAttr(tupdesc, j)->attname))));
      }
    }
  }

     
                                    
     
  for (i = 0; i < natts; i++)
  {
    CheckAttributeType(NameStr(TupleDescAttr(tupdesc, i)->attname), TupleDescAttr(tupdesc, i)->atttypid, TupleDescAttr(tupdesc, i)->attcollation, NIL,                                          
        flags);
  }
}

                                    
                       
   
                                                                
                                                                     
                                                                       
                                                                    
   
                                                                           
                                                                 
                                                                     
                                                                              
                                  
   
                                                                          
                                                                            
                                                                         
                                                                        
                                                                           
                                                                            
                                                                           
                                          
   
                                                                  
                                                                         
                                                  
                                    
   
void
CheckAttributeType(const char *attname, Oid atttypid, Oid attcollation, List *containing_rowtypes, int flags)
{
  char att_typtype = get_typtype(atttypid);
  Oid att_typelem;

  if (att_typtype == TYPTYPE_PSEUDO)
  {
       
                                                                        
                                                                    
       
                                                                         
                                                                         
                                                                       
                                                                   
                 
       
    if (!((atttypid == ANYARRAYOID && (flags & CHKATYPE_ANYARRAY)) || (atttypid == RECORDOID && (flags & CHKATYPE_ANYRECORD)) || (atttypid == RECORDARRAYOID && (flags & CHKATYPE_ANYRECORD))))
    {
      if (flags & CHKATYPE_IS_PARTKEY)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION),
                                                                              
                           errmsg("partition key column %s has pseudo-type %s", attname, format_type_be(atttypid))));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("column \"%s\" has pseudo-type %s", attname, format_type_be(atttypid))));
      }
    }
  }
  else if (att_typtype == TYPTYPE_DOMAIN)
  {
       
                                                         
       
    CheckAttributeType(attname, getBaseType(atttypid), attcollation, containing_rowtypes, flags);
  }
  else if (att_typtype == TYPTYPE_COMPOSITE)
  {
       
                                                          
       
    Relation relation;
    TupleDesc tupdesc;
    int i;

       
                                                                         
                                                                          
                                                                         
                                                                   
       
    if (list_member_oid(containing_rowtypes, atttypid))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("composite type %s cannot be made a member of itself", format_type_be(atttypid))));
    }

    containing_rowtypes = lcons_oid(atttypid, containing_rowtypes);

    relation = relation_open(get_typ_typrelid(atttypid), AccessShareLock);

    tupdesc = RelationGetDescr(relation);

    for (i = 0; i < tupdesc->natts; i++)
    {
      Form_pg_attribute attr = TupleDescAttr(tupdesc, i);

      if (attr->attisdropped)
      {
        continue;
      }
      CheckAttributeType(NameStr(attr->attname), attr->atttypid, attr->attcollation, containing_rowtypes, flags & ~CHKATYPE_IS_PARTKEY);
    }

    relation_close(relation, AccessShareLock);

    containing_rowtypes = list_delete_first(containing_rowtypes);
  }
  else if (att_typtype == TYPTYPE_RANGE)
  {
       
                                                      
       
    CheckAttributeType(attname, get_range_subtype(atttypid), get_range_collation(atttypid), containing_rowtypes, flags);
  }
  else if (OidIsValid((att_typelem = get_element_type(atttypid))))
  {
       
                                                                       
       
    CheckAttributeType(attname, att_typelem, attcollation, containing_rowtypes, flags);
  }

     
                                                                           
                                                               
     
  if (!OidIsValid(attcollation) && type_is_collatable(atttypid))
  {
    if (flags & CHKATYPE_IS_PARTKEY)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION),
                                                                            
                         errmsg("no collation was derived for partition key column %s with collatable type %s", attname, format_type_be(atttypid)), errhint("Use the COLLATE clause to set the collation explicitly.")));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("no collation was derived for column \"%s\" with collatable type %s", attname, format_type_be(atttypid)), errhint("Use the COLLATE clause to set the collation explicitly.")));
    }
  }
}

   
                          
                                                      
   
                                                                            
                                                                             
                                              
   
                                                                          
                                                                            
                                                                    
               
   
void
InsertPgAttributeTuple(Relation pg_attribute_rel, Form_pg_attribute new_attribute, CatalogIndexState indstate)
{
  Datum values[Natts_pg_attribute];
  bool nulls[Natts_pg_attribute];
  HeapTuple tup;

                                                                         
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));

  values[Anum_pg_attribute_attrelid - 1] = ObjectIdGetDatum(new_attribute->attrelid);
  values[Anum_pg_attribute_attname - 1] = NameGetDatum(&new_attribute->attname);
  values[Anum_pg_attribute_atttypid - 1] = ObjectIdGetDatum(new_attribute->atttypid);
  values[Anum_pg_attribute_attstattarget - 1] = Int32GetDatum(new_attribute->attstattarget);
  values[Anum_pg_attribute_attlen - 1] = Int16GetDatum(new_attribute->attlen);
  values[Anum_pg_attribute_attnum - 1] = Int16GetDatum(new_attribute->attnum);
  values[Anum_pg_attribute_attndims - 1] = Int32GetDatum(new_attribute->attndims);
  values[Anum_pg_attribute_attcacheoff - 1] = Int32GetDatum(-1);
  values[Anum_pg_attribute_atttypmod - 1] = Int32GetDatum(new_attribute->atttypmod);
  values[Anum_pg_attribute_attbyval - 1] = BoolGetDatum(new_attribute->attbyval);
  values[Anum_pg_attribute_attstorage - 1] = CharGetDatum(new_attribute->attstorage);
  values[Anum_pg_attribute_attalign - 1] = CharGetDatum(new_attribute->attalign);
  values[Anum_pg_attribute_attnotnull - 1] = BoolGetDatum(new_attribute->attnotnull);
  values[Anum_pg_attribute_atthasdef - 1] = BoolGetDatum(new_attribute->atthasdef);
  values[Anum_pg_attribute_atthasmissing - 1] = BoolGetDatum(new_attribute->atthasmissing);
  values[Anum_pg_attribute_attidentity - 1] = CharGetDatum(new_attribute->attidentity);
  values[Anum_pg_attribute_attgenerated - 1] = CharGetDatum(new_attribute->attgenerated);
  values[Anum_pg_attribute_attisdropped - 1] = BoolGetDatum(new_attribute->attisdropped);
  values[Anum_pg_attribute_attislocal - 1] = BoolGetDatum(new_attribute->attislocal);
  values[Anum_pg_attribute_attinhcount - 1] = Int32GetDatum(new_attribute->attinhcount);
  values[Anum_pg_attribute_attcollation - 1] = ObjectIdGetDatum(new_attribute->attcollation);

                                                          
  nulls[Anum_pg_attribute_attacl - 1] = true;
  nulls[Anum_pg_attribute_attoptions - 1] = true;
  nulls[Anum_pg_attribute_attfdwoptions - 1] = true;
  nulls[Anum_pg_attribute_attmissingval - 1] = true;

  tup = heap_form_tuple(RelationGetDescr(pg_attribute_rel), values, nulls);

                                                                      
  if (indstate != NULL)
  {
    CatalogTupleInsertWithInfo(pg_attribute_rel, tup, indstate);
  }
  else
  {
    CatalogTupleInsert(pg_attribute_rel, tup);
  }

  heap_freetuple(tup);
}

                                    
                          
   
                                                       
                            
                                    
   
static void
AddNewAttributeTuples(Oid new_rel_oid, TupleDesc tupdesc, char relkind)
{
  Form_pg_attribute attr;
  int i;
  Relation rel;
  CatalogIndexState indstate;
  int natts = tupdesc->natts;
  ObjectAddress myself, referenced;

     
                                        
     
  rel = table_open(AttributeRelationId, RowExclusiveLock);

  indstate = CatalogOpenIndexes(rel);

     
                                                                           
                                                         
     
  for (i = 0; i < natts; i++)
  {
    attr = TupleDescAttr(tupdesc, i);
                                          
    attr->attrelid = new_rel_oid;
                                   
    attr->attstattarget = -1;

    InsertPgAttributeTuple(rel, attr, indstate);

                             
    myself.classId = RelationRelationId;
    myself.objectId = new_rel_oid;
    myself.objectSubId = i + 1;
    referenced.classId = TypeRelationId;
    referenced.objectId = attr->atttypid;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                                                       
    if (OidIsValid(attr->attcollation) && attr->attcollation != DEFAULT_COLLATION_OID)
    {
      referenced.classId = CollationRelationId;
      referenced.objectId = attr->attcollation;
      referenced.objectSubId = 0;
      recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
    }
  }

     
                                                                           
                                                                            
                                                                     
     
  if (relkind != RELKIND_VIEW && relkind != RELKIND_COMPOSITE_TYPE)
  {
    for (i = 0; i < (int)lengthof(SysAtt); i++)
    {
      FormData_pg_attribute attStruct;

      memcpy(&attStruct, SysAtt[i], sizeof(FormData_pg_attribute));

                                                                
      attStruct.attrelid = new_rel_oid;

      InsertPgAttributeTuple(rel, &attStruct, indstate);
    }
  }

     
              
     
  CatalogCloseIndexes(indstate);

  table_close(rel, RowExclusiveLock);
}

                                    
                       
   
                                                  
   
                                                  
                                                                 
                                                                    
                                                                   
                                                                       
            
                                    
   
void
InsertPgClassTuple(Relation pg_class_desc, Relation new_rel_desc, Oid new_rel_oid, Datum relacl, Datum reloptions)
{
  Form_pg_class rd_rel = new_rel_desc->rd_rel;
  Datum values[Natts_pg_class];
  bool nulls[Natts_pg_class];
  HeapTuple tup;

                                                                         
  memset(values, 0, sizeof(values));
  memset(nulls, false, sizeof(nulls));

  values[Anum_pg_class_oid - 1] = ObjectIdGetDatum(new_rel_oid);
  values[Anum_pg_class_relname - 1] = NameGetDatum(&rd_rel->relname);
  values[Anum_pg_class_relnamespace - 1] = ObjectIdGetDatum(rd_rel->relnamespace);
  values[Anum_pg_class_reltype - 1] = ObjectIdGetDatum(rd_rel->reltype);
  values[Anum_pg_class_reloftype - 1] = ObjectIdGetDatum(rd_rel->reloftype);
  values[Anum_pg_class_relowner - 1] = ObjectIdGetDatum(rd_rel->relowner);
  values[Anum_pg_class_relam - 1] = ObjectIdGetDatum(rd_rel->relam);
  values[Anum_pg_class_relfilenode - 1] = ObjectIdGetDatum(rd_rel->relfilenode);
  values[Anum_pg_class_reltablespace - 1] = ObjectIdGetDatum(rd_rel->reltablespace);
  values[Anum_pg_class_relpages - 1] = Int32GetDatum(rd_rel->relpages);
  values[Anum_pg_class_reltuples - 1] = Float4GetDatum(rd_rel->reltuples);
  values[Anum_pg_class_relallvisible - 1] = Int32GetDatum(rd_rel->relallvisible);
  values[Anum_pg_class_reltoastrelid - 1] = ObjectIdGetDatum(rd_rel->reltoastrelid);
  values[Anum_pg_class_relhasindex - 1] = BoolGetDatum(rd_rel->relhasindex);
  values[Anum_pg_class_relisshared - 1] = BoolGetDatum(rd_rel->relisshared);
  values[Anum_pg_class_relpersistence - 1] = CharGetDatum(rd_rel->relpersistence);
  values[Anum_pg_class_relkind - 1] = CharGetDatum(rd_rel->relkind);
  values[Anum_pg_class_relnatts - 1] = Int16GetDatum(rd_rel->relnatts);
  values[Anum_pg_class_relchecks - 1] = Int16GetDatum(rd_rel->relchecks);
  values[Anum_pg_class_relhasrules - 1] = BoolGetDatum(rd_rel->relhasrules);
  values[Anum_pg_class_relhastriggers - 1] = BoolGetDatum(rd_rel->relhastriggers);
  values[Anum_pg_class_relrowsecurity - 1] = BoolGetDatum(rd_rel->relrowsecurity);
  values[Anum_pg_class_relforcerowsecurity - 1] = BoolGetDatum(rd_rel->relforcerowsecurity);
  values[Anum_pg_class_relhassubclass - 1] = BoolGetDatum(rd_rel->relhassubclass);
  values[Anum_pg_class_relispopulated - 1] = BoolGetDatum(rd_rel->relispopulated);
  values[Anum_pg_class_relreplident - 1] = CharGetDatum(rd_rel->relreplident);
  values[Anum_pg_class_relispartition - 1] = BoolGetDatum(rd_rel->relispartition);
  values[Anum_pg_class_relrewrite - 1] = ObjectIdGetDatum(rd_rel->relrewrite);
  values[Anum_pg_class_relfrozenxid - 1] = TransactionIdGetDatum(rd_rel->relfrozenxid);
  values[Anum_pg_class_relminmxid - 1] = MultiXactIdGetDatum(rd_rel->relminmxid);
  if (relacl != (Datum)0)
  {
    values[Anum_pg_class_relacl - 1] = relacl;
  }
  else
  {
    nulls[Anum_pg_class_relacl - 1] = true;
  }
  if (reloptions != (Datum)0)
  {
    values[Anum_pg_class_reloptions - 1] = reloptions;
  }
  else
  {
    nulls[Anum_pg_class_reloptions - 1] = true;
  }

                                                                
  nulls[Anum_pg_class_relpartbound - 1] = true;

  tup = heap_form_tuple(RelationGetDescr(pg_class_desc), values, nulls);

                                                                      
  CatalogTupleInsert(pg_class_desc, tup);

  heap_freetuple(tup);
}

                                    
                        
   
                                                       
                                
                                    
   
static void
AddNewRelationTuple(Relation pg_class_desc, Relation new_rel_desc, Oid new_rel_oid, Oid new_type_oid, Oid reloftype, Oid relowner, char relkind, TransactionId relfrozenxid, TransactionId relminmxid, Datum relacl, Datum reloptions)
{
  Form_pg_class new_rel_reltup;

     
                                                                           
                          
     
  new_rel_reltup = new_rel_desc->rd_rel;

  switch (relkind)
  {
  case RELKIND_RELATION:
  case RELKIND_MATVIEW:
  case RELKIND_INDEX:
  case RELKIND_TOASTVALUE:
                                                
    new_rel_reltup->relpages = 0;
    new_rel_reltup->reltuples = 0;
    new_rel_reltup->relallvisible = 0;
    break;
  case RELKIND_SEQUENCE:
                                            
    new_rel_reltup->relpages = 1;
    new_rel_reltup->reltuples = 1;
    new_rel_reltup->relallvisible = 0;
    break;
  default:
                                          
    new_rel_reltup->relpages = 0;
    new_rel_reltup->reltuples = 0;
    new_rel_reltup->relallvisible = 0;
    break;
  }

  new_rel_reltup->relfrozenxid = relfrozenxid;
  new_rel_reltup->relminmxid = relminmxid;
  new_rel_reltup->relowner = relowner;
  new_rel_reltup->reltype = new_type_oid;
  new_rel_reltup->reloftype = reloftype;

                                                                 
  new_rel_reltup->relispartition = false;

  new_rel_desc->rd_att->tdtypeid = new_type_oid;

                                      
  InsertPgClassTuple(pg_class_desc, new_rel_desc, new_rel_oid, relacl, reloptions);
}

                                    
                         
   
                                                              
                                    
   
static ObjectAddress
AddNewRelationType(const char *typeName, Oid typeNamespace, Oid new_rel_oid, char new_rel_kind, Oid ownerid, Oid new_row_type, Oid new_array_type)
{
  return TypeCreate(new_row_type,                                 
      typeName,                                  
      typeNamespace,                                  
      new_rel_oid,                                  
      new_rel_kind,                                  
      ownerid,                                    
      -1,                                                      
      TYPTYPE_COMPOSITE,                                     
      TYPCATEGORY_COMPOSITE,                                 
      false,                                                               
      DEFAULT_TYPDELIM,                                        
      F_RECORD_IN,                                     
      F_RECORD_OUT,                                     
      F_RECORD_RECV,                                     
      F_RECORD_SEND,                                  
      InvalidOid,                                                
      InvalidOid,                                                 
      InvalidOid,                                                  
      InvalidOid,                                                      
      false,                                                     
      new_array_type,                                    
      InvalidOid,                                                    
      NULL,                                                 
      NULL,                                                          
      false,                                               
      'd',                                                              
      'x',                                             
      -1,                                     
      0,                                                                
      false,                                         
      InvalidOid);                                                     
}

                                    
                             
   
                                                           
   
              
                                    
                                             
                                               
                                                                      
                                                                          
                                                                          
                                   
                                                            
                                                                        
                                
                                                                           
                                                         
                                                                      
                                                                    
                                                              
                                                                           
                                        
                                                                        
                                                    
   
                      
                                                                             
   
                                       
                                    
   
Oid
heap_create_with_catalog(const char *relname, Oid relnamespace, Oid reltablespace, Oid relid, Oid reltypeid, Oid reloftypeid, Oid ownerid, Oid accessmtd, TupleDesc tupdesc, List *cooked_constraints, char relkind, char relpersistence, bool shared_relation, bool mapped_relation, OnCommitAction oncommit, Datum reloptions, bool use_user_acl, bool allow_system_table_mods, bool is_internal, Oid relrewrite, ObjectAddress *typaddress)
{
  Relation pg_class_desc;
  Relation new_rel_desc;
  Acl *relacl;
  Oid existing_relid;
  Oid old_type_oid;
  Oid new_type_oid;
  ObjectAddress new_type_addr;
  Oid new_array_oid = InvalidOid;
  TransactionId relfrozenxid;
  MultiXactId relminmxid;

  pg_class_desc = table_open(RelationRelationId, RowExclusiveLock);

     
                   
     
  Assert(IsNormalProcessingMode() || IsBootstrapProcessingMode());

     
                                                            
                                                                         
                                                                            
     
  CheckAttributeNamesTypes(tupdesc, relkind, allow_system_table_mods ? CHKATYPE_ANYARRAY : 0);

     
                                                                           
                                                            
     
  existing_relid = get_relname_relid(relname, relnamespace);
  if (existing_relid != InvalidOid)
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_TABLE), errmsg("relation \"%s\" already exists", relname)));
  }

     
                                                                    
                                                                        
                                                                            
                                         
     
  old_type_oid = GetSysCacheOid2(TYPENAMENSP, Anum_pg_type_oid, CStringGetDatum(relname), ObjectIdGetDatum(relnamespace));
  if (OidIsValid(old_type_oid))
  {
    if (!moveArrayTypeName(old_type_oid, relname, relnamespace))
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("type \"%s\" already exists", relname),
                         errhint("A relation has an associated type of the same name, "
                                 "so you must use a name that doesn't conflict "
                                 "with any existing type.")));
    }
  }

     
                                                              
     
  if (shared_relation && reltablespace != GLOBALTABLESPACE_OID)
  {
    elog(ERROR, "shared relations must be placed in pg_global tablespace");
  }

     
                                                                        
     
                                                                      
                                                                   
     
  if (!OidIsValid(relid))
  {
                                                                   
    if (IsBinaryUpgrade && (relkind == RELKIND_RELATION || relkind == RELKIND_SEQUENCE || relkind == RELKIND_VIEW || relkind == RELKIND_MATVIEW || relkind == RELKIND_COMPOSITE_TYPE || relkind == RELKIND_FOREIGN_TABLE || relkind == RELKIND_PARTITIONED_TABLE))
    {
      if (!OidIsValid(binary_upgrade_next_heap_pg_class_oid))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("pg_class heap OID value not set when in binary upgrade mode")));
      }

      relid = binary_upgrade_next_heap_pg_class_oid;
      binary_upgrade_next_heap_pg_class_oid = InvalidOid;
    }
                                                                   
    else if (IsBinaryUpgrade && OidIsValid(binary_upgrade_next_toast_pg_class_oid) && relkind == RELKIND_TOASTVALUE)
    {
      relid = binary_upgrade_next_toast_pg_class_oid;
      binary_upgrade_next_toast_pg_class_oid = InvalidOid;
    }
    else
    {
      relid = GetNewRelFileNode(reltablespace, pg_class_desc, relpersistence);
    }
  }

     
                                                   
     
  if (use_user_acl)
  {
    switch (relkind)
    {
    case RELKIND_RELATION:
    case RELKIND_VIEW:
    case RELKIND_MATVIEW:
    case RELKIND_FOREIGN_TABLE:
    case RELKIND_PARTITIONED_TABLE:
      relacl = get_user_default_acl(OBJECT_TABLE, ownerid, relnamespace);
      break;
    case RELKIND_SEQUENCE:
      relacl = get_user_default_acl(OBJECT_SEQUENCE, ownerid, relnamespace);
      break;
    default:
      relacl = NULL;
      break;
    }
  }
  else
  {
    relacl = NULL;
  }

     
                                                                             
                                                                             
                                  
     
  new_rel_desc = heap_create(relname, relnamespace, reltablespace, relid, InvalidOid, accessmtd, tupdesc, relkind, relpersistence, shared_relation, mapped_relation, allow_system_table_mods, &relfrozenxid, &relminmxid);

  Assert(relid == RelationGetRelid(new_rel_desc));

  new_rel_desc->rd_rel->relrewrite = relrewrite;

     
                                                                            
                                                                       
                                                                          
                                                                            
     
  if (IsUnderPostmaster && (relkind == RELKIND_RELATION || relkind == RELKIND_VIEW || relkind == RELKIND_MATVIEW || relkind == RELKIND_FOREIGN_TABLE || relkind == RELKIND_COMPOSITE_TYPE || relkind == RELKIND_PARTITIONED_TABLE))
  {
    new_array_oid = AssignTypeArrayOid();
  }

     
                                                                         
                                                                             
                                                                         
                                
     
                                                                             
                                                                           
                                            
     
  new_type_addr = AddNewRelationType(relname, relnamespace, relid, relkind, ownerid, reltypeid, new_array_oid);
  new_type_oid = new_type_addr.objectId;
  if (typaddress)
  {
    *typaddress = new_type_addr;
  }

     
                                        
     
  if (OidIsValid(new_array_oid))
  {
    char *relarrayname;

    relarrayname = makeArrayTypeName(relname, relnamespace);

    TypeCreate(new_array_oid,                                   
        relarrayname,                              
        relnamespace,                                       
        InvalidOid,                                              
        0,                                                
        ownerid,                              
        -1,                                                
        TYPTYPE_BASE,                                         
        TYPCATEGORY_ARRAY,                               
        false,                                                     
        DEFAULT_TYPDELIM,                                  
        F_ARRAY_IN,                                 
        F_ARRAY_OUT,                                 
        F_ARRAY_RECV,                                    
        F_ARRAY_SEND,                                    
        InvalidOid,                                          
        InvalidOid,                                           
        F_ARRAY_TYPANALYZE,                                
        new_type_oid,                                               
        true,                                                 
        InvalidOid,                                       
        InvalidOid,                                              
        NULL,                                           
        NULL,                                                    
        false,                                         
        'd',                                                        
        'x',                                       
        -1,                               
        0,                                                          
        false,                                   
        InvalidOid);                                               

    pfree(relarrayname);
  }

     
                                                       
     
                                                                             
                                                                          
                                                 
     
  AddNewRelationTuple(pg_class_desc, new_rel_desc, relid, new_type_oid, reloftypeid, ownerid, relkind, relfrozenxid, relminmxid, PointerGetDatum(relacl), reloptions);

     
                                                                            
     
  AddNewAttributeTuples(relid, new_rel_desc->rd_att, relkind);

     
                                                                       
                                                                         
                                                              
     
                                                                         
                                                                          
                                                                          
                                                                             
                                                                       
                   
     
                                                                         
                          
     
  if (relkind != RELKIND_COMPOSITE_TYPE && relkind != RELKIND_TOASTVALUE && !IsBootstrapProcessingMode())
  {
    ObjectAddress myself, referenced;

    myself.classId = RelationRelationId;
    myself.objectId = relid;
    myself.objectSubId = 0;

    referenced.classId = NamespaceRelationId;
    referenced.objectId = relnamespace;
    referenced.objectSubId = 0;
    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

    recordDependencyOnOwner(RelationRelationId, relid, ownerid);

    recordDependencyOnNewAcl(RelationRelationId, relid, 0, ownerid, relacl);

    recordDependencyOnCurrentExtension(&myself, false);

    if (reloftypeid)
    {
      referenced.classId = TypeRelationId;
      referenced.objectId = reloftypeid;
      referenced.objectSubId = 0;
      recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
    }

       
                                                                         
                                                                           
       
                                                                         
                                 
       
    if (relkind == RELKIND_RELATION || relkind == RELKIND_MATVIEW)
    {
      referenced.classId = AccessMethodRelationId;
      referenced.objectId = accessmtd;
      referenced.objectSubId = 0;
      recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
    }
  }

                                           
  InvokeObjectPostCreateHookArg(RelationRelationId, relid, 0, is_internal);

     
                                                  
     
                                                                        
                                                                             
                                                                         
     
  StoreConstraints(new_rel_desc, cooked_constraints, is_internal);

     
                                                        
     
  if (oncommit != ONCOMMIT_NOOP)
  {
    register_on_commit_action(relid, oncommit);
  }

     
                                                                            
                                            
     
  table_close(new_rel_desc, NoLock);                                     
  table_close(pg_class_desc, RowExclusiveLock);

  return relid;
}

   
                              
   
                                                                      
                                                                        
                                                                     
                                                                      
                                           
   
static void
RelationRemoveInheritance(Oid relid)
{
  Relation catalogRelation;
  SysScanDesc scan;
  ScanKeyData key;
  HeapTuple tuple;

  catalogRelation = table_open(InheritsRelationId, RowExclusiveLock);

  ScanKeyInit(&key, Anum_pg_inherits_inhrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));

  scan = systable_beginscan(catalogRelation, InheritsRelidSeqnoIndexId, true, NULL, 1, &key);

  while (HeapTupleIsValid(tuple = systable_getnext(scan)))
  {
    CatalogTupleDelete(catalogRelation, &tuple->t_self);
  }

  systable_endscan(scan);
  table_close(catalogRelation, RowExclusiveLock);
}

   
                        
   
                                            
   
                                                                       
                                       
   
void
DeleteRelationTuple(Oid relid)
{
  Relation pg_class_desc;
  HeapTuple tup;

                                                         
  pg_class_desc = table_open(RelationRelationId, RowExclusiveLock);

  tup = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }

                                                              
  CatalogTupleDelete(pg_class_desc, &tup->t_self);

  ReleaseSysCache(tup);

  table_close(pg_class_desc, RowExclusiveLock);
}

   
                          
   
                                                 
   
                                                                       
                                       
   
void
DeleteAttributeTuples(Oid relid)
{
  Relation attrel;
  SysScanDesc scan;
  ScanKeyData key[1];
  HeapTuple atttup;

                                                             
  attrel = table_open(AttributeRelationId, RowExclusiveLock);

                                                                    
  ScanKeyInit(&key[0], Anum_pg_attribute_attrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));

  scan = systable_beginscan(attrel, AttributeRelidNumIndexId, true, NULL, 1, key);

                                      
  while ((atttup = systable_getnext(scan)) != NULL)
  {
    CatalogTupleDelete(attrel, &atttup->t_self);
  }

                               
  systable_endscan(scan);
  table_close(attrel, RowExclusiveLock);
}

   
                                
   
                                                                   
   
                                                                           
                                                                    
   
void
DeleteSystemAttributeTuples(Oid relid)
{
  Relation attrel;
  SysScanDesc scan;
  ScanKeyData key[2];
  HeapTuple atttup;

                                                             
  attrel = table_open(AttributeRelationId, RowExclusiveLock);

                                                                           
  ScanKeyInit(&key[0], Anum_pg_attribute_attrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));
  ScanKeyInit(&key[1], Anum_pg_attribute_attnum, BTLessEqualStrategyNumber, F_INT2LE, Int16GetDatum(0));

  scan = systable_beginscan(attrel, AttributeRelidNumIndexId, true, NULL, 2, key);

                                      
  while ((atttup = systable_getnext(scan)) != NULL)
  {
    CatalogTupleDelete(attrel, &atttup->t_self);
  }

                               
  systable_endscan(scan);
  table_close(attrel, RowExclusiveLock);
}

   
                        
   
                                                                            
                                                                         
                                                                         
                                
   
void
RemoveAttributeById(Oid relid, AttrNumber attnum)
{
  Relation rel;
  Relation attr_rel;
  HeapTuple tuple;
  Form_pg_attribute attStruct;
  char newattname[NAMEDATALEN];

     
                                                                           
                                                                          
                                                                          
                                                                            
     
  rel = relation_open(relid, AccessExclusiveLock);

  attr_rel = table_open(AttributeRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopy2(ATTNUM, ObjectIdGetDatum(relid), Int16GetDatum(attnum));
  if (!HeapTupleIsValid(tuple))                       
  {
    elog(ERROR, "cache lookup failed for attribute %d of relation %u", attnum, relid);
  }
  attStruct = (Form_pg_attribute)GETSTRUCT(tuple);

  if (attnum < 0)
  {
                                                                 

    CatalogTupleDelete(attr_rel, &tuple->t_self);
  }
  else
  {
                                                 

                                       
    attStruct->attisdropped = true;

       
                                                                     
                                                                          
                                                                     
                                                                       
                                                                           
                                                                          
                               
       
    attStruct->atttypid = InvalidOid;

                                                            
    attStruct->attnotnull = false;

                                                    
    attStruct->attstattarget = 0;

                                                                         
    attStruct->attgenerated = '\0';

       
                                                                         
       
    snprintf(newattname, sizeof(newattname), "........pg.dropped.%d........", attnum);
    namestrcpy(&(attStruct->attname), newattname);

                                        
    if (attStruct->atthasmissing)
    {
      Datum valuesAtt[Natts_pg_attribute];
      bool nullsAtt[Natts_pg_attribute];
      bool replacesAtt[Natts_pg_attribute];

                                                                  
      MemSet(valuesAtt, 0, sizeof(valuesAtt));
      MemSet(nullsAtt, false, sizeof(nullsAtt));
      MemSet(replacesAtt, false, sizeof(replacesAtt));

      valuesAtt[Anum_pg_attribute_atthasmissing - 1] = BoolGetDatum(false);
      replacesAtt[Anum_pg_attribute_atthasmissing - 1] = true;
      valuesAtt[Anum_pg_attribute_attmissingval - 1] = (Datum)0;
      nullsAtt[Anum_pg_attribute_attmissingval - 1] = true;
      replacesAtt[Anum_pg_attribute_attmissingval - 1] = true;

      tuple = heap_modify_tuple(tuple, RelationGetDescr(attr_rel), valuesAtt, nullsAtt, replacesAtt);
    }

    CatalogTupleUpdate(attr_rel, &tuple->t_self, tuple);
  }

     
                                                                             
                                                                       
                             
     

  table_close(attr_rel, RowExclusiveLock);

  if (attnum > 0)
  {
    RemoveStatistics(relid, attnum);
  }

  relation_close(rel, NoLock);
}

   
                      
   
                                                                 
                                                                          
   
void
RemoveAttrDefault(Oid relid, AttrNumber attnum, DropBehavior behavior, bool complain, bool internal)
{
  Relation attrdef_rel;
  ScanKeyData scankeys[2];
  SysScanDesc scan;
  HeapTuple tuple;
  bool found = false;

  attrdef_rel = table_open(AttrDefaultRelationId, RowExclusiveLock);

  ScanKeyInit(&scankeys[0], Anum_pg_attrdef_adrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));
  ScanKeyInit(&scankeys[1], Anum_pg_attrdef_adnum, BTEqualStrategyNumber, F_INT2EQ, Int16GetDatum(attnum));

  scan = systable_beginscan(attrdef_rel, AttrDefaultIndexId, true, NULL, 2, scankeys);

                                                                      
  while (HeapTupleIsValid(tuple = systable_getnext(scan)))
  {
    ObjectAddress object;
    Form_pg_attrdef attrtuple = (Form_pg_attrdef)GETSTRUCT(tuple);

    object.classId = AttrDefaultRelationId;
    object.objectId = attrtuple->oid;
    object.objectSubId = 0;

    performDeletion(&object, behavior, internal ? PERFORM_DELETION_INTERNAL : 0);

    found = true;
  }

  systable_endscan(scan);
  table_close(attrdef_rel, RowExclusiveLock);

  if (complain && !found)
  {
    elog(ERROR, "could not find attrdef tuple for relation %u attnum %d", relid, attnum);
  }
}

   
                          
   
                                                                    
                                                                             
                 
   
void
RemoveAttrDefaultById(Oid attrdefId)
{
  Relation attrdef_rel;
  Relation attr_rel;
  Relation myrel;
  ScanKeyData scankeys[1];
  SysScanDesc scan;
  HeapTuple tuple;
  Oid myrelid;
  AttrNumber myattnum;

                                                           
  attrdef_rel = table_open(AttrDefaultRelationId, RowExclusiveLock);

                                 
  ScanKeyInit(&scankeys[0], Anum_pg_attrdef_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(attrdefId));

  scan = systable_beginscan(attrdef_rel, AttrDefaultOidIndexId, true, NULL, 1, scankeys);

  tuple = systable_getnext(scan);
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "could not find tuple for attrdef %u", attrdefId);
  }

  myrelid = ((Form_pg_attrdef)GETSTRUCT(tuple))->adrelid;
  myattnum = ((Form_pg_attrdef)GETSTRUCT(tuple))->adnum;

                                                                  
  myrel = relation_open(myrelid, AccessExclusiveLock);

                                            
  CatalogTupleDelete(attrdef_rel, &tuple->t_self);

  systable_endscan(scan);
  table_close(attrdef_rel, RowExclusiveLock);

                                
  attr_rel = table_open(AttributeRelationId, RowExclusiveLock);

  tuple = SearchSysCacheCopy2(ATTNUM, ObjectIdGetDatum(myrelid), Int16GetDatum(myattnum));
  if (!HeapTupleIsValid(tuple))                       
  {
    elog(ERROR, "cache lookup failed for attribute %d of relation %u", myattnum, myrelid);
  }

  ((Form_pg_attribute)GETSTRUCT(tuple))->atthasdef = false;

  CatalogTupleUpdate(attr_rel, &tuple->t_self, tuple);

     
                                                                          
                                      
     
  table_close(attr_rel, RowExclusiveLock);

                                                      
  relation_close(myrel, NoLock);
}

   
                                                                     
   
                                                                           
                                                                           
                                                                       
                                                                               
                                                                          
   
void
heap_drop_with_catalog(Oid relid)
{
  Relation rel;
  HeapTuple tuple;
  Oid parentOid = InvalidOid, defaultPartOid = InvalidOid;

     
                                                                            
                                                                             
                                                                             
                                                                            
                                                                           
                                                                 
                                                                          
                  
     
  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", relid);
  }
  if (((Form_pg_class)GETSTRUCT(tuple))->relispartition)
  {
    parentOid = get_partition_parent(relid);
    LockRelationOid(parentOid, AccessExclusiveLock);

       
                                                                         
                                                                     
       
    defaultPartOid = get_default_partition_oid(parentOid);
    if (OidIsValid(defaultPartOid) && relid != defaultPartOid)
    {
      LockRelationOid(defaultPartOid, AccessExclusiveLock);
    }
  }

  ReleaseSysCache(tuple);

     
                                 
     
  rel = relation_open(relid, AccessExclusiveLock);

     
                                                                        
                                                                             
                      
     
  CheckTableNotInUse(rel, "DROP TABLE");

     
                                                                          
                                                                             
                                                                           
                
     
  CheckTableForSerializableConflictIn(rel);

     
                                          
     
  if (rel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
  {
    Relation rel;
    HeapTuple tuple;

    rel = table_open(ForeignTableRelationId, RowExclusiveLock);

    tuple = SearchSysCache1(FOREIGNTABLEREL, ObjectIdGetDatum(relid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for foreign table %u", relid);
    }

    CatalogTupleDelete(rel, &tuple->t_self);

    ReleaseSysCache(tuple);
    table_close(rel, RowExclusiveLock);
  }

     
                                                                    
     
  if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    RemovePartitionKeyByRelId(relid);
  }

     
                                                                    
                                                   
     
  if (relid == defaultPartOid)
  {
    update_default_partition_oid(parentOid, InvalidOid);
  }

     
                                                                    
     
  if (rel->rd_rel->relkind != RELKIND_VIEW && rel->rd_rel->relkind != RELKIND_COMPOSITE_TYPE && rel->rd_rel->relkind != RELKIND_FOREIGN_TABLE && rel->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
  {
    RelationDropStorage(rel);
  }

     
                                                                          
                                                                        
                                         
     
  relation_close(rel, NoLock);

     
                                                            
     
  RemoveSubscriptionRel(InvalidOid, relid);

     
                                             
     
  remove_on_commit_action(relid);

     
                                                                      
                                                                             
                                                                         
                                                                          
            
     
  RelationForgetRelation(relid);

     
                                    
     
  RelationRemoveInheritance(relid);

     
                       
     
  RemoveStatistics(relid, 0);

     
                             
     
  DeleteAttributeTuples(relid);

     
                           
     
  DeleteRelationTuple(relid);

  if (OidIsValid(parentOid))
  {
       
                                                                         
                                                                           
                                                          
       
    if (OidIsValid(defaultPartOid) && relid != defaultPartOid)
    {
      CacheInvalidateRelcacheByRelid(defaultPartOid);
    }

       
                                                                           
                                             
       
    CacheInvalidateRelcacheByRelid(parentOid);
                       
  }
}

   
                        
   
                                                                        
                                                                         
                                                                               
                                                                    
   
                                                                 
   
void
RelationClearMissing(Relation rel)
{
  Relation attr_rel;
  Oid relid = RelationGetRelid(rel);
  int natts = RelationGetNumberOfAttributes(rel);
  int attnum;
  Datum repl_val[Natts_pg_attribute];
  bool repl_null[Natts_pg_attribute];
  bool repl_repl[Natts_pg_attribute];
  Form_pg_attribute attrtuple;
  HeapTuple tuple, newtuple;

  memset(repl_val, 0, sizeof(repl_val));
  memset(repl_null, false, sizeof(repl_null));
  memset(repl_repl, false, sizeof(repl_repl));

  repl_val[Anum_pg_attribute_atthasmissing - 1] = BoolGetDatum(false);
  repl_null[Anum_pg_attribute_attmissingval - 1] = true;

  repl_repl[Anum_pg_attribute_atthasmissing - 1] = true;
  repl_repl[Anum_pg_attribute_attmissingval - 1] = true;

                                  
  attr_rel = table_open(AttributeRelationId, RowExclusiveLock);

                                                                        
  for (attnum = 1; attnum <= natts; attnum++)
  {
    tuple = SearchSysCache2(ATTNUM, ObjectIdGetDatum(relid), Int16GetDatum(attnum));
    if (!HeapTupleIsValid(tuple))                       
    {
      elog(ERROR, "cache lookup failed for attribute %d of relation %u", attnum, relid);
    }

    attrtuple = (Form_pg_attribute)GETSTRUCT(tuple);

                                                    
    if (attrtuple->atthasmissing)
    {
      newtuple = heap_modify_tuple(tuple, RelationGetDescr(attr_rel), repl_val, repl_null, repl_repl);

      CatalogTupleUpdate(attr_rel, &newtuple->t_self, newtuple);

      heap_freetuple(newtuple);
    }

    ReleaseSysCache(tuple);
  }

     
                                                                           
                                      
     
  table_close(attr_rel, RowExclusiveLock);
}

   
                  
   
                                                                            
                                                                            
              
   
void
SetAttrMissing(Oid relid, char *attname, char *value)
{
  Datum valuesAtt[Natts_pg_attribute];
  bool nullsAtt[Natts_pg_attribute];
  bool replacesAtt[Natts_pg_attribute];
  Datum missingval;
  Form_pg_attribute attStruct;
  Relation attrrel, tablerel;
  HeapTuple atttup, newtup;

                                               
  tablerel = table_open(relid, AccessExclusiveLock);

                                                   
  if (tablerel->rd_rel->relkind != RELKIND_RELATION)
  {
    table_close(tablerel, AccessExclusiveLock);
    return;
  }

                                               
  attrrel = table_open(AttributeRelationId, RowExclusiveLock);
  atttup = SearchSysCacheAttName(relid, attname);
  if (!HeapTupleIsValid(atttup))
  {
    elog(ERROR, "cache lookup failed for attribute %s of relation %u", attname, relid);
  }
  attStruct = (Form_pg_attribute)GETSTRUCT(atttup);

                                                
  missingval = OidFunctionCall3(F_ARRAY_IN, CStringGetDatum(value), ObjectIdGetDatum(attStruct->atttypid), Int32GetDatum(attStruct->atttypmod));

                                                              
  MemSet(valuesAtt, 0, sizeof(valuesAtt));
  MemSet(nullsAtt, false, sizeof(nullsAtt));
  MemSet(replacesAtt, false, sizeof(replacesAtt));

  valuesAtt[Anum_pg_attribute_atthasmissing - 1] = BoolGetDatum(true);
  replacesAtt[Anum_pg_attribute_atthasmissing - 1] = true;
  valuesAtt[Anum_pg_attribute_attmissingval - 1] = missingval;
  replacesAtt[Anum_pg_attribute_attmissingval - 1] = true;

  newtup = heap_modify_tuple(atttup, RelationGetDescr(attrrel), valuesAtt, nullsAtt, replacesAtt);
  CatalogTupleUpdate(attrrel, &newtup->t_self, newtup);

                
  ReleaseSysCache(atttup);
  table_close(attrrel, RowExclusiveLock);
  table_close(tablerel, AccessExclusiveLock);
}

   
                                                                 
   
                                                
   
                                                                        
                                                                              
                                                                             
                                                                             
                                          
   
Oid
StoreAttrDefault(Relation rel, AttrNumber attnum, Node *expr, bool is_internal, bool add_column_mode)
{
  char *adbin;
  Relation adrel;
  HeapTuple tuple;
  Datum values[4];
  static bool nulls[4] = {false, false, false, false};
  Relation attrrel;
  HeapTuple atttup;
  Form_pg_attribute attStruct;
  char attgenerated;
  Oid attrdefOid;
  ObjectAddress colobject, defobject;

  adrel = table_open(AttrDefaultRelationId, RowExclusiveLock);

     
                                                    
     
  adbin = nodeToString(expr);

     
                                
     
  attrdefOid = GetNewOidWithIndex(adrel, AttrDefaultOidIndexId, Anum_pg_attrdef_oid);
  values[Anum_pg_attrdef_oid - 1] = ObjectIdGetDatum(attrdefOid);
  values[Anum_pg_attrdef_adrelid - 1] = RelationGetRelid(rel);
  values[Anum_pg_attrdef_adnum - 1] = attnum;
  values[Anum_pg_attrdef_adbin - 1] = CStringGetTextDatum(adbin);

  tuple = heap_form_tuple(adrel->rd_att, values, nulls);
  CatalogTupleInsert(adrel, tuple);

  defobject.classId = AttrDefaultRelationId;
  defobject.objectId = attrdefOid;
  defobject.objectSubId = 0;

  table_close(adrel, RowExclusiveLock);

                                                      
  pfree(DatumGetPointer(values[Anum_pg_attrdef_adbin - 1]));
  heap_freetuple(tuple);
  pfree(adbin);

     
                                                                         
             
     
  attrrel = table_open(AttributeRelationId, RowExclusiveLock);
  atttup = SearchSysCacheCopy2(ATTNUM, ObjectIdGetDatum(RelationGetRelid(rel)), Int16GetDatum(attnum));
  if (!HeapTupleIsValid(atttup))
  {
    elog(ERROR, "cache lookup failed for attribute %d of relation %u", attnum, RelationGetRelid(rel));
  }
  attStruct = (Form_pg_attribute)GETSTRUCT(atttup);
  attgenerated = attStruct->attgenerated;
  if (!attStruct->atthasdef)
  {
    Form_pg_attribute defAttStruct;

    ExprState *exprState;
    Expr *expr2 = (Expr *)expr;
    EState *estate = NULL;
    ExprContext *econtext;
    Datum valuesAtt[Natts_pg_attribute];
    bool nullsAtt[Natts_pg_attribute];
    bool replacesAtt[Natts_pg_attribute];
    Datum missingval = (Datum)0;
    bool missingIsNull = true;

    MemSet(valuesAtt, 0, sizeof(valuesAtt));
    MemSet(nullsAtt, false, sizeof(nullsAtt));
    MemSet(replacesAtt, false, sizeof(replacesAtt));
    valuesAtt[Anum_pg_attribute_atthasdef - 1] = true;
    replacesAtt[Anum_pg_attribute_atthasdef - 1] = true;

    if (rel->rd_rel->relkind == RELKIND_RELATION && add_column_mode && !attgenerated)
    {
      expr2 = expression_planner(expr2);
      estate = CreateExecutorState();
      exprState = ExecPrepareExpr(expr2, estate);
      econtext = GetPerTupleExprContext(estate);

      missingval = ExecEvalExpr(exprState, econtext, &missingIsNull);

      FreeExecutorState(estate);

      defAttStruct = TupleDescAttr(rel->rd_att, attnum - 1);

      if (missingIsNull)
      {
                                                                       
        missingval = (Datum)0;
      }
      else
      {
                                                             
        missingval = PointerGetDatum(construct_array(&missingval, 1, defAttStruct->atttypid, defAttStruct->attlen, defAttStruct->attbyval, defAttStruct->attalign));
      }

      valuesAtt[Anum_pg_attribute_atthasmissing - 1] = !missingIsNull;
      replacesAtt[Anum_pg_attribute_atthasmissing - 1] = true;
      valuesAtt[Anum_pg_attribute_attmissingval - 1] = missingval;
      replacesAtt[Anum_pg_attribute_attmissingval - 1] = true;
      nullsAtt[Anum_pg_attribute_attmissingval - 1] = missingIsNull;
    }
    atttup = heap_modify_tuple(atttup, RelationGetDescr(attrrel), valuesAtt, nullsAtt, replacesAtt);

    CatalogTupleUpdate(attrrel, &atttup->t_self, atttup);

    if (!missingIsNull)
    {
      pfree(DatumGetPointer(missingval));
    }
  }
  table_close(attrrel, RowExclusiveLock);
  heap_freetuple(atttup);

     
                                                                            
                                  
     
  colobject.classId = RelationRelationId;
  colobject.objectId = RelationGetRelid(rel);
  colobject.objectSubId = attnum;

  recordDependencyOn(&defobject, &colobject, DEPENDENCY_AUTO);

     
                                                                 
     
  if (attgenerated)
  {
       
                                                                          
                                                           
       
    recordDependencyOnSingleRelExpr(&colobject, expr, RelationGetRelid(rel), DEPENDENCY_AUTO, DEPENDENCY_AUTO, false);
  }
  else
  {
       
                                                                    
                                                    
       
    recordDependencyOnSingleRelExpr(&defobject, expr, RelationGetRelid(rel), DEPENDENCY_NORMAL, DEPENDENCY_NORMAL, false);
  }

     
                                                
     
                                                                          
                                                                          
                                                                           
                           
     
  InvokeObjectPostCreateHookArg(AttrDefaultRelationId, RelationGetRelid(rel), attnum, is_internal);

  return attrdefOid;
}

   
                                                               
   
                                                               
                                           
   
                                              
   
static Oid
StoreRelCheck(Relation rel, const char *ccname, Node *expr, bool is_validated, bool is_local, int inhcount, bool is_no_inherit, bool is_internal)
{
  char *ccbin;
  List *varList;
  int keycount;
  int16 *attNos;
  Oid constrOid;

     
                                                    
     
  ccbin = nodeToString(expr);

     
                                               
     
                                                                             
                                                                    
                 
     
  varList = pull_var_clause(expr, 0);
  keycount = list_length(varList);

  if (keycount > 0)
  {
    ListCell *vl;
    int i = 0;

    attNos = (int16 *)palloc(keycount * sizeof(int16));
    foreach (vl, varList)
    {
      Var *var = (Var *)lfirst(vl);
      int j;

      for (j = 0; j < i; j++)
      {
        if (attNos[j] == var->varattno)
        {
          break;
        }
      }
      if (j == i)
      {
        attNos[i++] = var->varattno;
      }
    }
    keycount = i;
  }
  else
  {
    attNos = NULL;
  }

     
                                                                            
                                
     
  if (is_no_inherit && rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("cannot add NO INHERIT constraint to partitioned table \"%s\"", RelationGetRelationName(rel))));
  }

     
                                 
     
  constrOid = CreateConstraintEntry(ccname,                                
      RelationGetNamespace(rel),                                     
      CONSTRAINT_CHECK,                                                    
      false,                                                             
      false,                                                           
      is_validated, InvalidOid,                                                 
      RelationGetRelid(rel),                                        
      attNos,                                                                      
      keycount,                                                                          
      keycount,                                                                            
      InvalidOid,                                                                  
      InvalidOid,                                                              
      InvalidOid,                                                             
      NULL, NULL, NULL, NULL, 0, ' ', ' ', ' ', NULL,                                  
      expr,                                                                              
      ccbin,                                                                               
      is_local,                                                       
      inhcount,                                                        
      is_no_inherit,                                                    
      is_internal);                                                                

  pfree(ccbin);

  return constrOid;
}

   
                                                                          
   
                                                                                
   
                                                                          
                                                                      
                                                                       
                                                        
   
static void
StoreConstraints(Relation rel, List *cooked_constraints, bool is_internal)
{
  int numchecks = 0;
  ListCell *lc;

  if (cooked_constraints == NIL)
  {
    return;                    
  }

     
                                                                           
                                                                           
                                                                          
     
  CommandCounterIncrement();

  foreach (lc, cooked_constraints)
  {
    CookedConstraint *con = (CookedConstraint *)lfirst(lc);

    switch (con->contype)
    {
    case CONSTR_DEFAULT:
      con->conoid = StoreAttrDefault(rel, con->attnum, con->expr, is_internal, false);
      break;
    case CONSTR_CHECK:
      con->conoid = StoreRelCheck(rel, con->name, con->expr, !con->skip_validation, con->is_local, con->inhcount, con->is_no_inherit, is_internal);
      numchecks++;
      break;
    default:
      elog(ERROR, "unrecognized constraint type: %d", (int)con->contype);
    }
  }

  if (numchecks > 0)
  {
    SetRelationNumChecks(rel, numchecks);
  }
}

   
                             
   
                                                                          
                                                                          
                                                                             
                
   
                                
                                                       
                                            
                                                                           
                                                                  
                                                                            
   
                                                                               
                                                         
   
                                                                          
                                                                 
   
                                                                          
                                                                           
                                                                              
                   
   
List *
AddRelationNewConstraints(Relation rel, List *newColDefaults, List *newConstraints, bool allow_merge, bool is_local, bool is_internal, const char *queryString)
{
  List *cookedConstraints = NIL;
  TupleDesc tupleDesc;
  TupleConstr *oldconstr;
  int numoldchecks;
  ParseState *pstate;
  RangeTblEntry *rte;
  int numchecks;
  List *checknames;
  ListCell *cell;
  Node *expr;
  CookedConstraint *cooked;

     
                                          
     
  tupleDesc = RelationGetDescr(rel);
  oldconstr = tupleDesc->constr;
  if (oldconstr)
  {
    numoldchecks = oldconstr->num_check;
  }
  else
  {
    numoldchecks = 0;
  }

     
                                                                          
                                                                
     
  pstate = make_parsestate(NULL);
  pstate->p_sourcetext = queryString;
  rte = addRangeTableEntryForRelation(pstate, rel, AccessShareLock, NULL, false, true);
  addRTEtoQuery(pstate, rte, true, true, true);

     
                                         
     
  foreach (cell, newColDefaults)
  {
    RawColumnDefault *colDef = (RawColumnDefault *)lfirst(cell);
    Form_pg_attribute atp = TupleDescAttr(rel->rd_att, colDef->attnum - 1);
    Oid defOid;

    expr = cookDefault(pstate, colDef->raw_default, atp->atttypid, atp->atttypmod, NameStr(atp->attname), atp->attgenerated);

       
                                                                           
                                                                   
                                                                 
                               
       
                                                                      
                                                                  
                                                                         
                                                                   
                                                        
       
    if (expr == NULL || (!colDef->generated && IsA(expr, Const) && castNode(Const, expr)->constisnull))
    {
      continue;
    }

                                                                  
    if (colDef->missingMode && contain_volatile_functions((Node *)expr))
    {
      colDef->missingMode = false;
    }

    defOid = StoreAttrDefault(rel, colDef->attnum, expr, is_internal, colDef->missingMode);

    cooked = (CookedConstraint *)palloc(sizeof(CookedConstraint));
    cooked->contype = CONSTR_DEFAULT;
    cooked->conoid = defOid;
    cooked->name = NULL;
    cooked->attnum = colDef->attnum;
    cooked->expr = expr;
    cooked->skip_validation = false;
    cooked->is_local = is_local;
    cooked->inhcount = is_local ? 0 : 1;
    cooked->is_no_inherit = false;
    cookedConstraints = lappend(cookedConstraints, cooked);
  }

     
                                     
     
  numchecks = numoldchecks;
  checknames = NIL;
  foreach (cell, newConstraints)
  {
    Constraint *cdef = (Constraint *)lfirst(cell);
    char *ccname;
    Oid constrOid;

    if (cdef->contype != CONSTR_CHECK)
    {
      continue;
    }

    if (cdef->raw_expr != NULL)
    {
      Assert(cdef->cooked_expr == NULL);

         
                                                                      
                                           
         
      expr = cookConstraint(pstate, cdef->raw_expr, RelationGetRelationName(rel));
    }
    else
    {
      Assert(cdef->cooked_expr != NULL);

         
                                                                  
                                                       
         
      expr = stringToNode(cdef->cooked_expr);
    }

       
                                                                    
       
    if (cdef->conname != NULL)
    {
      ListCell *cell2;

      ccname = cdef->conname;
                                               
                                                                      
      foreach (cell2, checknames)
      {
        if (strcmp((char *)lfirst(cell2), ccname) == 0)
        {
          ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("check constraint \"%s\" already exists", ccname)));
        }
      }

                                       
      checknames = lappend(checknames, ccname);

         
                                                                       
                                                                        
                                                                     
                                           
         
      if (MergeWithExistingConstraint(rel, ccname, expr, allow_merge, is_local, cdef->initially_valid, cdef->is_no_inherit))
      {
        continue;
      }
    }
    else
    {
         
                                                                         
                                                                       
                                                                        
                                                                         
                                                                   
                                                         
         
                                                                       
                                                                     
                             
         
      List *vars;
      char *colname;

      vars = pull_var_clause(expr, 0);

                                
      vars = list_union(NIL, vars);

      if (list_length(vars) == 1)
      {
        colname = get_attname(RelationGetRelid(rel), ((Var *)linitial(vars))->varattno, true);
      }
      else
      {
        colname = NULL;
      }

      ccname = ChooseConstraintName(RelationGetRelationName(rel), colname, "check", RelationGetNamespace(rel), checknames);

                                       
      checknames = lappend(checknames, ccname);
    }

       
                     
       
    constrOid = StoreRelCheck(rel, ccname, expr, cdef->initially_valid, is_local, is_local ? 0 : 1, cdef->is_no_inherit, is_internal);

    numchecks++;

    cooked = (CookedConstraint *)palloc(sizeof(CookedConstraint));
    cooked->contype = CONSTR_CHECK;
    cooked->conoid = constrOid;
    cooked->name = ccname;
    cooked->attnum = 0;
    cooked->expr = expr;
    cooked->skip_validation = cdef->skip_validation;
    cooked->is_local = is_local;
    cooked->inhcount = is_local ? 0 : 1;
    cooked->is_no_inherit = cdef->is_no_inherit;
    cookedConstraints = lappend(cookedConstraints, cooked);
  }

     
                                                                             
                                                                            
                                                                        
                                                                      
                                                         
     
  SetRelationNumChecks(rel, numchecks);

  return cookedConstraints;
}

   
                                                                            
                                                                           
                    
   
                                                                        
                                                          
   
                                                                     
   
static bool
MergeWithExistingConstraint(Relation rel, const char *ccname, Node *expr, bool allow_merge, bool is_local, bool is_initially_valid, bool is_no_inherit)
{
  bool found;
  Relation conDesc;
  SysScanDesc conscan;
  ScanKeyData skey[3];
  HeapTuple tup;

                                                                    
  conDesc = table_open(ConstraintRelationId, RowExclusiveLock);

  found = false;

  ScanKeyInit(&skey[0], Anum_pg_constraint_conrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(rel)));
  ScanKeyInit(&skey[1], Anum_pg_constraint_contypid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(InvalidOid));
  ScanKeyInit(&skey[2], Anum_pg_constraint_conname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(ccname));

  conscan = systable_beginscan(conDesc, ConstraintRelidTypidNameIndexId, true, NULL, 3, skey);

                                             
  if (HeapTupleIsValid(tup = systable_getnext(conscan)))
  {
    Form_pg_constraint con = (Form_pg_constraint)GETSTRUCT(tup);

                                                                
    if (con->contype == CONSTRAINT_CHECK)
    {
      Datum val;
      bool isnull;

      val = fastgetattr(tup, Anum_pg_constraint_conbin, conDesc->rd_att, &isnull);
      if (isnull)
      {
        elog(ERROR, "null conbin for rel %s", RelationGetRelationName(rel));
      }
      if (equal(expr, stringToNode(TextDatumGetCString(val))))
      {
        found = true;
      }
    }

       
                                                                
                                                                      
                                                                          
                                                                           
                                                                         
                                                    
       
    if (is_local && !con->conislocal && !rel->rd_rel->relispartition)
    {
      allow_merge = true;
    }

    if (!found || !allow_merge)
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("constraint \"%s\" for relation \"%s\" already exists", ccname, RelationGetRelationName(rel))));
    }

                                                                   
    if (con->connoinherit)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("constraint \"%s\" conflicts with non-inherited constraint on relation \"%s\"", ccname, RelationGetRelationName(rel))));
    }

       
                                                                        
                                                                       
                                          
       
    if (con->coninhcount > 0 && is_no_inherit)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("constraint \"%s\" conflicts with inherited constraint on relation \"%s\"", ccname, RelationGetRelationName(rel))));
    }

       
                                                                       
                                
       
    if (is_initially_valid && !con->convalidated)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("constraint \"%s\" conflicts with NOT VALID constraint on relation \"%s\"", ccname, RelationGetRelationName(rel))));
    }

                                
    ereport(NOTICE, (errmsg("merging constraint \"%s\" with inherited definition", ccname)));

    tup = heap_copytuple(tup);
    con = (Form_pg_constraint)GETSTRUCT(tup);

       
                                                                        
                                                                       
                         
       
    if (rel->rd_rel->relispartition)
    {
      con->coninhcount = 1;
      con->conislocal = false;
    }
    else
    {
      if (is_local)
      {
        con->conislocal = true;
      }
      else
      {
        con->coninhcount++;
      }
    }

    if (is_no_inherit)
    {
      Assert(is_local);
      con->connoinherit = true;
    }

    CatalogTupleUpdate(conDesc, &tup->t_self, tup);
  }

  systable_endscan(conscan);
  table_close(conDesc, RowExclusiveLock);

  return found;
}

   
                                                                     
   
                                                          
   
                                                                             
                                                                        
                                                                      
                                                           
   
static void
SetRelationNumChecks(Relation rel, int numchecks)
{
  Relation relrel;
  HeapTuple reltup;
  Form_pg_class relStruct;

  relrel = table_open(RelationRelationId, RowExclusiveLock);
  reltup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(RelationGetRelid(rel)));
  if (!HeapTupleIsValid(reltup))
  {
    elog(ERROR, "cache lookup failed for relation %u", RelationGetRelid(rel));
  }
  relStruct = (Form_pg_class)GETSTRUCT(reltup);

  if (relStruct->relchecks != numchecks)
  {
    relStruct->relchecks = numchecks;

    CatalogTupleUpdate(relrel, &reltup->t_self, reltup);
  }
  else
  {
                                                               
    CacheInvalidateRelcache(rel);
  }

  heap_freetuple(reltup);
  table_close(relrel, RowExclusiveLock);
}

   
                                             
   
static bool
check_nested_generated_walker(Node *node, void *context)
{
  ParseState *pstate = context;

  if (node == NULL)
  {
    return false;
  }
  else if (IsA(node, Var))
  {
    Var *var = (Var *)node;
    Oid relid;
    AttrNumber attnum;

    relid = rt_fetch(var->varno, pstate->p_rtable)->relid;
    if (!OidIsValid(relid))
    {
      return false;                                       
    }

    attnum = var->varattno;

    if (attnum > 0 && get_attgenerated(relid, attnum))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("cannot use generated column \"%s\" in column generation expression", get_attname(relid, attnum, false)), errdetail("A generated column cannot reference another generated column."), parser_errposition(pstate, var->location)));
    }
                                                                       
    if (attnum == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("cannot use whole-row variable in column generation expression"), errdetail("This would cause the generated column to depend on its own value."), parser_errposition(pstate, var->location)));
    }
                                                           

    return false;
  }
  else
  {
    return expression_tree_walker(node, check_nested_generated_walker, (void *)context);
  }
}

static void
check_nested_generated(ParseState *pstate, Node *node)
{
  check_nested_generated_walker(node, pstate);
}

   
                                                                  
            
   
                                                                        
                                                                      
                                                                        
   
                                                                         
                                                                       
                                            
   
Node *
cookDefault(ParseState *pstate, Node *raw_default, Oid atttypid, int32 atttypmod, const char *attname, char attgenerated)
{
  Node *expr;

  Assert(raw_default != NULL);

     
                                                       
     
  expr = transformExpr(pstate, raw_default, attgenerated ? EXPR_KIND_GENERATED_COLUMN : EXPR_KIND_COLUMN_DEFAULT);

  if (attgenerated)
  {
    check_nested_generated(pstate, expr);

    if (contain_mutable_functions(expr))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("generation expression is not immutable")));
    }
  }
  else
  {
       
                                                                      
                          
       
    Assert(!contain_var_clause(expr));
  }

     
                                                                          
                                                                           
                                  
     
  if (OidIsValid(atttypid))
  {
    Oid type_id = exprType(expr);

    expr = coerce_to_target_type(pstate, expr, type_id, atttypid, atttypmod, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
    if (expr == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                         errmsg("column \"%s\" is of type %s"
                                " but default expression is of type %s",
                             attname, format_type_be(atttypid), format_type_be(type_id)),
                         errhint("You will need to rewrite or cast the expression.")));
    }
  }

     
                                                                  
     
  assign_expr_collations(pstate, expr);

  return expr;
}

   
                                                                            
                      
   
                                                                      
                      
   
static Node *
cookConstraint(ParseState *pstate, Node *raw_constraint, char *relname)
{
  Node *expr;

     
                                                       
     
  expr = transformExpr(pstate, raw_constraint, EXPR_KIND_CHECK_CONSTRAINT);

     
                                           
     
  expr = coerce_to_boolean(pstate, expr, "CHECK");

     
                              
     
  assign_expr_collations(pstate, expr);

     
                                                                           
                                                 
     
  if (list_length(pstate->p_rtable) != 1)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("only table \"%s\" can be referenced in check constraint", relname)));
  }

  return expr;
}

   
                                                                           
   
void
CopyStatistics(Oid fromrelid, Oid torelid)
{
  HeapTuple tup;
  SysScanDesc scan;
  ScanKeyData key[1];
  Relation statrel;

  statrel = table_open(StatisticRelationId, RowExclusiveLock);

                                   
  ScanKeyInit(&key[0], Anum_pg_statistic_starelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(fromrelid));

  scan = systable_beginscan(statrel, StatisticRelidAttnumInhIndexId, true, NULL, 1, key);

  while (HeapTupleIsValid((tup = systable_getnext(scan))))
  {
    Form_pg_statistic statform;

                                
    tup = heap_copytuple(tup);
    statform = (Form_pg_statistic)GETSTRUCT(tup);

                                                    
    statform->starelid = torelid;
    CatalogTupleInsert(statrel, tup);

    heap_freetuple(tup);
  }

  systable_endscan(scan);

  table_close(statrel, RowExclusiveLock);
}

   
                                                                           
   
                                                                              
                    
   
void
RemoveStatistics(Oid relid, AttrNumber attnum)
{
  Relation pgstatistic;
  SysScanDesc scan;
  ScanKeyData key[2];
  int nkeys;
  HeapTuple tuple;

  pgstatistic = table_open(StatisticRelationId, RowExclusiveLock);

  ScanKeyInit(&key[0], Anum_pg_statistic_starelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));

  if (attnum == 0)
  {
    nkeys = 1;
  }
  else
  {
    ScanKeyInit(&key[1], Anum_pg_statistic_staattnum, BTEqualStrategyNumber, F_INT2EQ, Int16GetDatum(attnum));
    nkeys = 2;
  }

  scan = systable_beginscan(pgstatistic, StatisticRelidAttnumInhIndexId, true, NULL, nkeys, key);

                                                                      
  while (HeapTupleIsValid(tuple = systable_getnext(scan)))
  {
    CatalogTupleDelete(pgstatistic, &tuple->t_self);
  }

  systable_endscan(scan);

  table_close(pgstatistic, RowExclusiveLock);
}

   
                                                             
                                          
   
                                                                 
                                                                    
   
static void
RelationTruncateIndexes(Relation heapRelation)
{
  ListCell *indlist;

                                                                    
  foreach (indlist, RelationGetIndexList(heapRelation))
  {
    Oid indexId = lfirst_oid(indlist);
    Relation currentIndex;
    IndexInfo *indexInfo;

                                                                      
    currentIndex = index_open(indexId, AccessExclusiveLock);

       
                                                                      
                                                                         
                                                                         
                                                                  
                                                                 
                                                                
       
    indexInfo = BuildDummyIndexInfo(currentIndex);

       
                                                           
       
    RelationTruncate(currentIndex, 0);

                                          
                                                           
    index_build(heapRelation, currentIndex, indexInfo, true, false);

                                    
    index_close(currentIndex, NoLock);
  }
}

   
                  
   
                                                                      
   
                                                                     
                                                                     
                                                                      
   
void
heap_truncate(List *relids)
{
  List *relations = NIL;
  ListCell *cell;

                                                                        
  foreach (cell, relids)
  {
    Oid rid = lfirst_oid(cell);
    Relation rel;

    rel = table_open(rid, AccessExclusiveLock);
    relations = lappend(relations, rel);
  }

                                                                          
  heap_truncate_check_FKs(relations, true);

                   
  foreach (cell, relations)
  {
    Relation rel = lfirst(cell);

                               
    heap_truncate_one_rel(rel);

                                                                        
    table_close(rel, NoLock);
  }
}

   
                          
   
                                                                 
   
                                                                            
                                                                      
                                                                        
   
void
heap_truncate_one_rel(Relation rel)
{
  Oid toastrelid;

     
                                                                             
                                  
     
  if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    return;
  }

                                        
  table_relation_nontransactional_truncate(rel);

                                                             
  RelationTruncateIndexes(rel);

                                                    
  toastrelid = rel->rd_rel->reltoastrelid;
  if (OidIsValid(toastrelid))
  {
    Relation toastrel = table_open(toastrelid, AccessExclusiveLock);

    table_relation_nontransactional_truncate(toastrel);
    RelationTruncateIndexes(toastrel);
                          
    table_close(toastrel, NoLock);
  }
}

   
                           
                                                                
                                                          
   
                                                                             
                                                                     
   
                                                                              
                                                                
   
                                                                   
   
void
heap_truncate_check_FKs(List *relations, bool tempTables)
{
  List *oids = NIL;
  List *dependents;
  ListCell *cell;

     
                                                        
     
                                                                        
                                                                      
                                                                       
             
     
  foreach (cell, relations)
  {
    Relation rel = lfirst(cell);

    if (rel->rd_rel->relhastriggers || rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
    {
      oids = lappend_oid(oids, RelationGetRelid(rel));
    }
  }

     
                                                                  
     
  if (oids == NIL)
  {
    return;
  }

     
                                                                        
                                                                    
     
  dependents = heap_truncate_find_FKs(oids);
  if (dependents == NIL)
  {
    return;
  }

     
                                                                             
                                                                    
                                                                          
                                                                         
                                                             
     
  foreach (cell, oids)
  {
    Oid relid = lfirst_oid(cell);
    ListCell *cell2;

    dependents = heap_truncate_find_FKs(list_make1_oid(relid));

    foreach (cell2, dependents)
    {
      Oid relid2 = lfirst_oid(cell2);

      if (!list_member_oid(oids, relid2))
      {
        char *relname = get_rel_name(relid);
        char *relname2 = get_rel_name(relid2);

        if (tempTables)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("unsupported ON COMMIT and foreign key combination"), errdetail("Table \"%s\" references \"%s\", but they do not have the same ON COMMIT setting.", relname2, relname)));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot truncate a table referenced in a foreign key constraint"), errdetail("Table \"%s\" references \"%s\".", relname2, relname),
                             errhint("Truncate table \"%s\" at the same time, "
                                     "or use TRUNCATE ... CASCADE.",
                                 relname2)));
        }
      }
    }
  }
}

   
                          
                                                                         
   
                                                                          
                                                                             
                                                                            
                                                                           
                                                                               
   
                                                                           
                                                                           
                                                              
   
List *
heap_truncate_find_FKs(List *relationIds)
{
  List *result = NIL;
  List *oids = list_copy(relationIds);
  List *parent_cons;
  ListCell *cell;
  ScanKeyData key;
  Relation fkeyRel;
  SysScanDesc fkeyScan;
  HeapTuple tuple;
  bool restart;

  oids = list_copy(relationIds);

     
                                                                           
                                      
     
  fkeyRel = table_open(ConstraintRelationId, AccessShareLock);

restart:
  restart = false;
  parent_cons = NIL;

  fkeyScan = systable_beginscan(fkeyRel, InvalidOid, false, NULL, 0, NULL);

  while (HeapTupleIsValid(tuple = systable_getnext(fkeyScan)))
  {
    Form_pg_constraint con = (Form_pg_constraint)GETSTRUCT(tuple);

                           
    if (con->contype != CONSTRAINT_FOREIGN)
    {
      continue;
    }

                                                   
    if (!list_member_oid(oids, con->confrelid))
    {
      continue;
    }

       
                                                                         
                                                                          
                                                                           
                                                                       
              
       
    if (OidIsValid(con->conparentid) && !list_member_oid(parent_cons, con->conparentid))
    {
      parent_cons = lappend_oid(parent_cons, con->conparentid);
    }

       
                                                                           
             
       
    if (!list_member_oid(relationIds, con->conrelid))
    {
      result = insert_ordered_unique_oid(result, con->conrelid);
    }
  }

  systable_endscan(fkeyScan);

     
                                                                           
                                                                    
                                                                            
                                                                      
                                            
     
  foreach (cell, parent_cons)
  {
    Oid parent = lfirst_oid(cell);

    ScanKeyInit(&key, Anum_pg_constraint_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(parent));

    fkeyScan = systable_beginscan(fkeyRel, ConstraintOidIndexId, true, NULL, 1, &key);

    tuple = systable_getnext(fkeyScan);
    if (HeapTupleIsValid(tuple))
    {
      Form_pg_constraint con = (Form_pg_constraint)GETSTRUCT(tuple);

         
                                                                      
                                                                       
                                                                     
                     
         
                                                                 
                                                                     
                                                                   
                                                                      
                                                
         
      if (OidIsValid(con->conparentid))
      {
        parent_cons = list_append_unique_oid(parent_cons, con->conparentid);
      }
      else if (!list_member_oid(oids, con->confrelid))
      {
        oids = lappend_oid(oids, con->confrelid);
        restart = true;
      }
    }

    systable_endscan(fkeyScan);
  }

  list_free(parent_cons);
  if (restart)
  {
    goto restart;
  }

  table_close(fkeyRel, AccessShareLock);
  list_free(oids);

  return result;
}

   
                             
                                                                      
                               
   
                                                                         
                                                                        
                                                                            
                                                                     
   
static List *
insert_ordered_unique_oid(List *list, Oid datum)
{
  ListCell *prev;

                                           
  if (list == NIL || datum < linitial_oid(list))
  {
    return lcons_oid(datum, list);
  }
                                      
  if (datum == linitial_oid(list))
  {
    return list;                                 
  }
                                              
  prev = list_head(list);
  for (;;)
  {
    ListCell *curr = lnext(prev);

    if (curr == NULL || datum < lfirst_oid(curr))
    {
      break;                                             
    }

    if (datum == lfirst_oid(curr))
    {
      return list;                                 
    }

    prev = curr;
  }
                                           
  lappend_cell_oid(list, prev, datum);
  return list;
}

   
                     
                                                                   
   
void
StorePartitionKey(Relation rel, char strategy, int16 partnatts, AttrNumber *partattrs, List *partexprs, Oid *partopclass, Oid *partcollation)
{
  int i;
  int2vector *partattrs_vec;
  oidvector *partopclass_vec;
  oidvector *partcollation_vec;
  Datum partexprDatum;
  Relation pg_partitioned_table;
  HeapTuple tuple;
  Datum values[Natts_pg_partitioned_table];
  bool nulls[Natts_pg_partitioned_table];
  ObjectAddress myself;
  ObjectAddress referenced;

  Assert(rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE);

                                                                      
  partattrs_vec = buildint2vector(partattrs, partnatts);
  partopclass_vec = buildoidvector(partopclass, partnatts);
  partcollation_vec = buildoidvector(partcollation, partnatts);

                                                        
  if (partexprs)
  {
    char *exprString;

    exprString = nodeToString(partexprs);
    partexprDatum = CStringGetTextDatum(exprString);
    pfree(exprString);
  }
  else
  {
    partexprDatum = (Datum)0;
  }

  pg_partitioned_table = table_open(PartitionedRelationId, RowExclusiveLock);

  MemSet(nulls, false, sizeof(nulls));

                                  
  if (!partexprDatum)
  {
    nulls[Anum_pg_partitioned_table_partexprs - 1] = true;
  }

  values[Anum_pg_partitioned_table_partrelid - 1] = ObjectIdGetDatum(RelationGetRelid(rel));
  values[Anum_pg_partitioned_table_partstrat - 1] = CharGetDatum(strategy);
  values[Anum_pg_partitioned_table_partnatts - 1] = Int16GetDatum(partnatts);
  values[Anum_pg_partitioned_table_partdefid - 1] = ObjectIdGetDatum(InvalidOid);
  values[Anum_pg_partitioned_table_partattrs - 1] = PointerGetDatum(partattrs_vec);
  values[Anum_pg_partitioned_table_partclass - 1] = PointerGetDatum(partopclass_vec);
  values[Anum_pg_partitioned_table_partcollation - 1] = PointerGetDatum(partcollation_vec);
  values[Anum_pg_partitioned_table_partexprs - 1] = partexprDatum;

  tuple = heap_form_tuple(RelationGetDescr(pg_partitioned_table), values, nulls);

  CatalogTupleInsert(pg_partitioned_table, tuple);
  table_close(pg_partitioned_table, RowExclusiveLock);

                                                                  
  myself.classId = RelationRelationId;
  myself.objectId = RelationGetRelid(rel);
  myself.objectSubId = 0;

                                                   
  for (i = 0; i < partnatts; i++)
  {
    referenced.classId = OperatorClassRelationId;
    referenced.objectId = partopclass[i];
    referenced.objectSubId = 0;

    recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);

                                                                       
    if (OidIsValid(partcollation[i]) && partcollation[i] != DEFAULT_COLLATION_OID)
    {
      referenced.classId = CollationRelationId;
      referenced.objectId = partcollation[i];
      referenced.objectSubId = 0;

      recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
    }
  }

     
                                                                          
                                                                          
                                                                             
                                       
     
  for (i = 0; i < partnatts; i++)
  {
    if (partattrs[i] == 0)
    {
      continue;                              
    }

    referenced.classId = RelationRelationId;
    referenced.objectId = RelationGetRelid(rel);
    referenced.objectSubId = partattrs[i];

    recordDependencyOn(&referenced, &myself, DEPENDENCY_INTERNAL);
  }

     
                                                                          
                                                                         
                                                                             
                                                                        
     
  if (partexprs)
  {
    recordDependencyOnSingleRelExpr(&myself, (Node *)partexprs, RelationGetRelid(rel), DEPENDENCY_NORMAL, DEPENDENCY_INTERNAL, true                            );
  }

     
                                                      
                                                                           
                                                
     
  CacheInvalidateRelcache(rel);
}

   
                             
                                                     
   
void
RemovePartitionKeyByRelId(Oid relid)
{
  Relation rel;
  HeapTuple tuple;

  rel = table_open(PartitionedRelationId, RowExclusiveLock);

  tuple = SearchSysCache1(PARTRELID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for partition key of relation %u", relid);
  }

  CatalogTupleDelete(rel, &tuple->t_self);

  ReleaseSysCache(tuple);
  table_close(rel, RowExclusiveLock);
}

   
                       
                                                                      
                           
   
                                                                              
                         
   
                                                                              
                                                                          
                                                                     
   
void
StorePartitionBound(Relation rel, Relation parent, PartitionBoundSpec *bound)
{
  Relation classRel;
  HeapTuple tuple, newtuple;
  Datum new_val[Natts_pg_class];
  bool new_null[Natts_pg_class], new_repl[Natts_pg_class];
  Oid defaultPartOid;

                             
  classRel = table_open(RelationRelationId, RowExclusiveLock);
  tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(RelationGetRelid(rel)));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", RelationGetRelid(rel));
  }

#ifdef USE_ASSERT_CHECKING
  {
    Form_pg_class classForm;
    bool isnull;

    classForm = (Form_pg_class)GETSTRUCT(tuple);
    Assert(!classForm->relispartition);
    (void)SysCacheGetAttr(RELOID, tuple, Anum_pg_class_relpartbound, &isnull);
    Assert(isnull);
  }
#endif

                                  
  memset(new_val, 0, sizeof(new_val));
  memset(new_null, false, sizeof(new_null));
  memset(new_repl, false, sizeof(new_repl));
  new_val[Anum_pg_class_relpartbound - 1] = CStringGetTextDatum(nodeToString(bound));
  new_null[Anum_pg_class_relpartbound - 1] = false;
  new_repl[Anum_pg_class_relpartbound - 1] = true;
  newtuple = heap_modify_tuple(tuple, RelationGetDescr(classRel), new_val, new_null, new_repl);
                         
  ((Form_pg_class)GETSTRUCT(newtuple))->relispartition = true;
  CatalogTupleUpdate(classRel, &newtuple->t_self, newtuple);
  heap_freetuple(newtuple);
  table_close(classRel, RowExclusiveLock);

     
                                                               
                               
     
  if (bound->is_default)
  {
    update_default_partition_oid(RelationGetRelid(parent), RelationGetRelid(rel));
  }

                                  
  CommandCounterIncrement();

     
                                                                       
                                                                          
                                                                          
              
     
  defaultPartOid = get_default_oid_from_partdesc(RelationGetPartitionDesc(parent));
  if (OidIsValid(defaultPartOid))
  {
    CacheInvalidateRelcacheByRelid(defaultPartOid);
  }

  CacheInvalidateRelcache(parent);
}
