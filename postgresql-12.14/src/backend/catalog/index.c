                                                                            
   
           
                                                         
   
                                                                         
                                                                        
   
   
                  
                                 
   
   
                      
                                                         
                                                          
                                                       
                                                                   
   
                                                                            
   
#include "postgres.h"

#include <unistd.h>

#include "access/amapi.h"
#include "access/heapam.h"
#include "access/multixact.h"
#include "access/relscan.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/visibilitymap.h"
#include "access/xact.h"
#include "bootstrap/bootstrap.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/objectaccess.h"
#include "catalog/partition.h"
#include "catalog/pg_am.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_description.h"
#include "catalog/pg_depend.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_trigger.h"
#include "catalog/pg_type.h"
#include "catalog/storage.h"
#include "commands/event_trigger.h"
#include "commands/progress.h"
#include "commands/tablecmds.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/parser.h"
#include "pgstat.h"
#include "rewrite/rewriteManip.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "storage/procarray.h"
#include "storage/smgr.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_rusage.h"
#include "utils/syscache.h"
#include "utils/tuplesort.h"
#include "utils/snapmgr.h"

                                                     
Oid binary_upgrade_next_index_pg_class_oid = InvalidOid;

   
                                                                        
                                                                        
   
typedef struct
{
  Oid currentlyReindexedHeap;
  Oid currentlyReindexedIndex;
  int numPendingReindexedIndexes;
  Oid pendingReindexedIndexes[FLEXIBLE_ARRAY_MEMBER];
} SerializedReindexState;

                                    
static bool
relationHasPrimaryKey(Relation rel);
static TupleDesc
ConstructTupleDescriptor(Relation heapRelation, IndexInfo *indexInfo, List *indexColNames, Oid accessMethodObjectId, Oid *collationObjectId, Oid *classObjectId);
static void
InitializeAttributeOids(Relation indexRelation, int numatts, Oid indexoid);
static void
AppendAttributeTuples(Relation indexRelation, int numatts);
static void
UpdateIndexRelation(Oid indexoid, Oid heapoid, Oid parentIndexId, IndexInfo *indexInfo, Oid *collationOids, Oid *classOids, int16 *coloptions, bool primary, bool isexclusion, bool immediate, bool isvalid, bool isready);
static void
index_update_stats(Relation rel, bool hasindex, double reltuples);
static void
IndexCheckExclusion(Relation heapRelation, Relation indexRelation, IndexInfo *indexInfo);
static bool
validate_index_callback(ItemPointer itemptr, void *opaque);
static bool
ReindexIsCurrentlyProcessingIndex(Oid indexOid);
static void
SetReindexProcessing(Oid heapOid, Oid indexOid);
static void
ResetReindexProcessing(void);
static void
SetReindexPending(List *indexes);
static void
RemoveReindexPending(Oid indexOid);

   
                         
                                                        
   
                                                   
   
                                                                            
                                                                              
                                                              
   
static bool
relationHasPrimaryKey(Relation rel)
{
  bool result = false;
  List *indexoidlist;
  ListCell *indexoidscan;

     
                                                                             
                                                                            
                                                 
     
  indexoidlist = RelationGetIndexList(rel);

  foreach (indexoidscan, indexoidlist)
  {
    Oid indexoid = lfirst_oid(indexoidscan);
    HeapTuple indexTuple;

    indexTuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexoid));
    if (!HeapTupleIsValid(indexTuple))                        
    {
      elog(ERROR, "cache lookup failed for index %u", indexoid);
    }
    result = ((Form_pg_index)GETSTRUCT(indexTuple))->indisprimary;
    ReleaseSysCache(indexTuple);
    if (result)
    {
      break;
    }
  }

  list_free(indexoidlist);

  return result;
}

   
                           
                                                                    
   
                                                                       
                                                                             
   
                                                                              
                                                                      
                                               
   
                                                                              
                                                                           
                                                                          
                                                                        
                                                                           
                                  
   
                                                                             
                               
   
void
index_check_primary_key(Relation heapRel, IndexInfo *indexInfo, bool is_alter_table, IndexStmt *stmt)
{
  int i;

     
                                                                            
                                                                          
                                                                           
                                                                             
     
  if ((is_alter_table || heapRel->rd_rel->relispartition) && relationHasPrimaryKey(heapRel))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("multiple primary keys for table \"%s\" are not allowed", RelationGetRelationName(heapRel))));
  }

     
                                                                         
                                                                             
                                              
     
  for (i = 0; i < indexInfo->ii_NumIndexKeyAttrs; i++)
  {
    AttrNumber attnum = indexInfo->ii_IndexAttrNumbers[i];
    HeapTuple atttuple;
    Form_pg_attribute attform;

    if (attnum == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("primary keys cannot be expressions")));
    }

                                                               
    if (attnum < 0)
    {
      continue;
    }

    atttuple = SearchSysCache2(ATTNUM, ObjectIdGetDatum(RelationGetRelid(heapRel)), Int16GetDatum(attnum));
    if (!HeapTupleIsValid(atttuple))
    {
      elog(ERROR, "cache lookup failed for attribute %d of relation %u", attnum, RelationGetRelid(heapRel));
    }
    attform = (Form_pg_attribute)GETSTRUCT(atttuple);

    if (!attform->attnotnull)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TABLE_DEFINITION), errmsg("primary key column \"%s\" is not marked NOT NULL", NameStr(attform->attname))));
    }

    ReleaseSysCache(atttuple);
  }
}

   
                             
   
                                                   
   
static TupleDesc
ConstructTupleDescriptor(Relation heapRelation, IndexInfo *indexInfo, List *indexColNames, Oid accessMethodObjectId, Oid *collationObjectId, Oid *classObjectId)
{
  int numatts = indexInfo->ii_NumIndexAttrs;
  int numkeyatts = indexInfo->ii_NumIndexKeyAttrs;
  ListCell *colnames_item = list_head(indexColNames);
  ListCell *indexpr_item = list_head(indexInfo->ii_Expressions);
  IndexAmRoutine *amroutine;
  TupleDesc heapTupDesc;
  TupleDesc indexTupDesc;
  int natts;                                             
  int i;

                                                   
  amroutine = GetIndexAmRoutineByAmId(accessMethodObjectId, false);

                                               
  heapTupDesc = RelationGetDescr(heapRelation);
  natts = RelationGetForm(heapRelation)->relnatts;

     
                                       
     
  indexTupDesc = CreateTemplateTupleDesc(numatts);

     
                                   
     
  for (i = 0; i < numatts; i++)
  {
    AttrNumber atnum = indexInfo->ii_IndexAttrNumbers[i];
    Form_pg_attribute to = TupleDescAttr(indexTupDesc, i);
    HeapTuple tuple;
    Form_pg_type typeTup;
    Form_pg_opclass opclassTup;
    Oid keyType;

    MemSet(to, 0, ATTRIBUTE_FIXED_PART_SIZE);
    to->attnum = i + 1;
    to->attstattarget = -1;
    to->attcacheoff = -1;
    to->attislocal = true;
    to->attcollation = (i < numkeyatts) ? collationObjectId[i] : InvalidOid;

       
                                                      
       
    if (colnames_item == NULL)                       
    {
      elog(ERROR, "too few entries in colnames list");
    }
    namestrcpy(&to->attname, (const char *)lfirst(colnames_item));
    colnames_item = lnext(colnames_item);

       
                                                                           
                                                                           
               
       
    if (atnum != 0)
    {
                               
      const FormData_pg_attribute *from;

      Assert(atnum > 0);                                  

      if (atnum > natts)                   
      {
        elog(ERROR, "invalid column number %d", atnum);
      }
      from = TupleDescAttr(heapTupDesc, AttrNumberGetAttrOffset(atnum));

      to->atttypid = from->atttypid;
      to->attlen = from->attlen;
      to->attndims = from->attndims;
      to->atttypmod = from->atttypmod;
      to->attbyval = from->attbyval;
      to->attstorage = from->attstorage;
      to->attalign = from->attalign;
    }
    else
    {
                              
      Node *indexkey;

      if (indexpr_item == NULL)                       
      {
        elog(ERROR, "too few entries in indexprs list");
      }
      indexkey = (Node *)lfirst(indexpr_item);
      indexpr_item = lnext(indexpr_item);

         
                                                                        
         
      keyType = exprType(indexkey);
      tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(keyType));
      if (!HeapTupleIsValid(tuple))
      {
        elog(ERROR, "cache lookup failed for type %u", keyType);
      }
      typeTup = (Form_pg_type)GETSTRUCT(tuple);

         
                                                               
         
      to->atttypid = keyType;
      to->attlen = typeTup->typlen;
      to->attbyval = typeTup->typbyval;
      to->attstorage = typeTup->typstorage;
      to->attalign = typeTup->typalign;
      to->atttypmod = exprTypmod(indexkey);

      ReleaseSysCache(tuple);

         
                                                                        
                                                                         
                                                                         
                                                                    
                                                                    
                                                                   
                                                     
         
      CheckAttributeType(NameStr(to->attname), to->atttypid, to->attcollation, NIL, 0);
    }

       
                                                                          
                                                                      
              
       
    to->attrelid = InvalidOid;

       
                                                                          
                                                                   
                   
       
    keyType = amroutine->amkeytype;

       
                                                                        
                             
       
    if (i < indexInfo->ii_NumIndexKeyAttrs)
    {
      tuple = SearchSysCache1(CLAOID, ObjectIdGetDatum(classObjectId[i]));
      if (!HeapTupleIsValid(tuple))
      {
        elog(ERROR, "cache lookup failed for opclass %u", classObjectId[i]);
      }
      opclassTup = (Form_pg_opclass)GETSTRUCT(tuple);
      if (OidIsValid(opclassTup->opckeytype))
      {
        keyType = opclassTup->opckeytype;
      }

         
                                                                 
                                                                       
                                                               
         
      if (keyType == ANYELEMENTOID && opclassTup->opcintype == ANYARRAYOID)
      {
        keyType = get_base_element_type(to->atttypid);
        if (!OidIsValid(keyType))
        {
          elog(ERROR, "could not get element type of array type %u", to->atttypid);
        }
      }

      ReleaseSysCache(tuple);
    }

       
                                                                        
                                                     
       
    if (OidIsValid(keyType) && keyType != to->atttypid)
    {
      tuple = SearchSysCache1(TYPEOID, ObjectIdGetDatum(keyType));
      if (!HeapTupleIsValid(tuple))
      {
        elog(ERROR, "cache lookup failed for type %u", keyType);
      }
      typeTup = (Form_pg_type)GETSTRUCT(tuple);

      to->atttypid = keyType;
      to->atttypmod = -1;
      to->attlen = typeTup->typlen;
      to->attbyval = typeTup->typbyval;
      to->attalign = typeTup->typalign;
      to->attstorage = typeTup->typstorage;

      ReleaseSysCache(tuple);
    }
  }

  pfree(amroutine);

  return indexTupDesc;
}

                                                                    
                            
                                                                    
   
static void
InitializeAttributeOids(Relation indexRelation, int numatts, Oid indexoid)
{
  TupleDesc tupleDescriptor;
  int i;

  tupleDescriptor = RelationGetDescr(indexRelation);

  for (i = 0; i < numatts; i += 1)
  {
    TupleDescAttr(tupleDescriptor, i)->attrelid = indexoid;
  }
}

                                                                    
                          
                                                                    
   
static void
AppendAttributeTuples(Relation indexRelation, int numatts)
{
  Relation pg_attribute;
  CatalogIndexState indstate;
  TupleDesc indexTupDesc;
  int i;

     
                                                 
     
  pg_attribute = table_open(AttributeRelationId, RowExclusiveLock);

  indstate = CatalogOpenIndexes(pg_attribute);

     
                                                            
     
  indexTupDesc = RelationGetDescr(indexRelation);

  for (i = 0; i < numatts; i++)
  {
    Form_pg_attribute attr = TupleDescAttr(indexTupDesc, i);

    Assert(attr->attnum == i + 1);

    InsertPgAttributeTuple(pg_attribute, attr, indstate);
  }

  CatalogCloseIndexes(indstate);

  table_close(pg_attribute, RowExclusiveLock);
}

                                                                    
                        
   
                                                            
                                                                    
   
static void
UpdateIndexRelation(Oid indexoid, Oid heapoid, Oid parentIndexId, IndexInfo *indexInfo, Oid *collationOids, Oid *classOids, int16 *coloptions, bool primary, bool isexclusion, bool immediate, bool isvalid, bool isready)
{
  int2vector *indkey;
  oidvector *indcollation;
  oidvector *indclass;
  int2vector *indoption;
  Datum exprsDatum;
  Datum predDatum;
  Datum values[Natts_pg_index];
  bool nulls[Natts_pg_index];
  Relation pg_index;
  HeapTuple tuple;
  int i;

     
                                                                            
                                                         
     
  indkey = buildint2vector(NULL, indexInfo->ii_NumIndexAttrs);
  for (i = 0; i < indexInfo->ii_NumIndexAttrs; i++)
  {
    indkey->values[i] = indexInfo->ii_IndexAttrNumbers[i];
  }
  indcollation = buildoidvector(collationOids, indexInfo->ii_NumIndexKeyAttrs);
  indclass = buildoidvector(classOids, indexInfo->ii_NumIndexKeyAttrs);
  indoption = buildint2vector(coloptions, indexInfo->ii_NumIndexKeyAttrs);

     
                                                            
     
  if (indexInfo->ii_Expressions != NIL)
  {
    char *exprsString;

    exprsString = nodeToString(indexInfo->ii_Expressions);
    exprsDatum = CStringGetTextDatum(exprsString);
    pfree(exprsString);
  }
  else
  {
    exprsDatum = (Datum)0;
  }

     
                                                                            
                                                             
     
  if (indexInfo->ii_Predicate != NIL)
  {
    char *predString;

    predString = nodeToString(make_ands_explicit(indexInfo->ii_Predicate));
    predDatum = CStringGetTextDatum(predString);
    pfree(predString);
  }
  else
  {
    predDatum = (Datum)0;
  }

     
                                            
     
  pg_index = table_open(IndexRelationId, RowExclusiveLock);

     
                            
     
  MemSet(nulls, false, sizeof(nulls));

  values[Anum_pg_index_indexrelid - 1] = ObjectIdGetDatum(indexoid);
  values[Anum_pg_index_indrelid - 1] = ObjectIdGetDatum(heapoid);
  values[Anum_pg_index_indnatts - 1] = Int16GetDatum(indexInfo->ii_NumIndexAttrs);
  values[Anum_pg_index_indnkeyatts - 1] = Int16GetDatum(indexInfo->ii_NumIndexKeyAttrs);
  values[Anum_pg_index_indisunique - 1] = BoolGetDatum(indexInfo->ii_Unique);
  values[Anum_pg_index_indisprimary - 1] = BoolGetDatum(primary);
  values[Anum_pg_index_indisexclusion - 1] = BoolGetDatum(isexclusion);
  values[Anum_pg_index_indimmediate - 1] = BoolGetDatum(immediate);
  values[Anum_pg_index_indisclustered - 1] = BoolGetDatum(false);
  values[Anum_pg_index_indisvalid - 1] = BoolGetDatum(isvalid);
  values[Anum_pg_index_indcheckxmin - 1] = BoolGetDatum(false);
  values[Anum_pg_index_indisready - 1] = BoolGetDatum(isready);
  values[Anum_pg_index_indislive - 1] = BoolGetDatum(true);
  values[Anum_pg_index_indisreplident - 1] = BoolGetDatum(false);
  values[Anum_pg_index_indkey - 1] = PointerGetDatum(indkey);
  values[Anum_pg_index_indcollation - 1] = PointerGetDatum(indcollation);
  values[Anum_pg_index_indclass - 1] = PointerGetDatum(indclass);
  values[Anum_pg_index_indoption - 1] = PointerGetDatum(indoption);
  values[Anum_pg_index_indexprs - 1] = exprsDatum;
  if (exprsDatum == (Datum)0)
  {
    nulls[Anum_pg_index_indexprs - 1] = true;
  }
  values[Anum_pg_index_indpred - 1] = predDatum;
  if (predDatum == (Datum)0)
  {
    nulls[Anum_pg_index_indpred - 1] = true;
  }

  tuple = heap_form_tuple(RelationGetDescr(pg_index), values, nulls);

     
                                                
     
  CatalogTupleInsert(pg_index, tuple);

     
                                           
     
  table_close(pg_index, RowExclusiveLock);
  heap_freetuple(tuple);
}

   
                
   
                                                                     
                                  
                                                                  
                                                                 
                                          
                                                                    
                                        
                                                                        
                                                           
                                                                      
                                               
                                                               
                                                                 
                                                
                                          
                                                                    
                                                                    
                                                            
                                   
                                                                  
                            
                                
                                 
                                         
                             
                                                                   
                                           
                             
                                                               
                                                                
                   
                                
                                                            
                     
                              
                                                            
                                                         
                                                 
                                                               
                                                          
                                                                 
   
                                         
   
Oid
index_create(Relation heapRelation, const char *indexRelationName, Oid indexRelationId, Oid parentIndexRelid, Oid parentConstraintId, Oid relFileNode, IndexInfo *indexInfo, List *indexColNames, Oid accessMethodObjectId, Oid tableSpaceId, Oid *collationObjectId, Oid *classObjectId, int16 *coloptions, Datum reloptions, bits16 flags, bits16 constr_flags, bool allow_system_table_mods, bool is_internal, Oid *constraintId)
{
  Oid heapRelationId = RelationGetRelid(heapRelation);
  Relation pg_class;
  Relation indexRelation;
  TupleDesc indexTupDesc;
  bool shared_relation;
  bool mapped_relation;
  bool is_exclusion;
  Oid namespaceId;
  int i;
  char relpersistence;
  bool isprimary = (flags & INDEX_CREATE_IS_PRIMARY) != 0;
  bool invalid = (flags & INDEX_CREATE_INVALID) != 0;
  bool concurrent = (flags & INDEX_CREATE_CONCURRENT) != 0;
  bool partitioned = (flags & INDEX_CREATE_PARTITIONED) != 0;
  char relkind;
  TransactionId relfrozenxid;
  MultiXactId relminmxid;

                                                                       
  Assert((constr_flags == 0) || ((flags & INDEX_CREATE_ADD_CONSTRAINT) != 0));
                                                               
  Assert(!partitioned || (flags & INDEX_CREATE_SKIP_BUILD));

  relkind = partitioned ? RELKIND_PARTITIONED_INDEX : RELKIND_INDEX;
  is_exclusion = (indexInfo->ii_ExclusionOps != NULL);

  pg_class = table_open(RelationRelationId, RowExclusiveLock);

     
                                                                         
                                                                         
                                                                         
                                           
     
  namespaceId = RelationGetNamespace(heapRelation);
  shared_relation = heapRelation->rd_rel->relisshared;
  mapped_relation = RelationIsMapped(heapRelation);
  relpersistence = heapRelation->rd_rel->relpersistence;

     
                      
     
  if (indexInfo->ii_NumIndexAttrs < 1)
  {
    elog(ERROR, "must index at least one column");
  }

  if (!allow_system_table_mods && IsSystemRelation(heapRelation) && IsNormalProcessingMode())
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("user-defined indexes on system catalog tables are not supported")));
  }

     
                                                                            
                                                                             
                                                                          
                                                                           
                                                                           
                                                                        
                                                                             
                                                                  
     
                                                                            
                                                                             
                                                                            
                    
     
                                                                        
                                                                       
                                                               
     
  for (i = 0; i < indexInfo->ii_NumIndexKeyAttrs; i++)
  {
    Oid collation = collationObjectId[i];
    Oid opclass = classObjectId[i];

    if (collation)
    {
      if ((opclass == TEXT_BTREE_PATTERN_OPS_OID || opclass == VARCHAR_BTREE_PATTERN_OPS_OID || opclass == BPCHAR_BTREE_PATTERN_OPS_OID) && !get_collation_isdeterministic(collation))
      {
        HeapTuple classtup;

        classtup = SearchSysCache1(CLAOID, ObjectIdGetDatum(opclass));
        if (!HeapTupleIsValid(classtup))
        {
          elog(ERROR, "cache lookup failed for operator class %u", opclass);
        }
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("nondeterministic collations are not supported for operator class \"%s\"", NameStr(((Form_pg_opclass)GETSTRUCT(classtup))->opcname))));
        ReleaseSysCache(classtup);
      }
    }
  }

     
                                                                             
                                                  
     
  if (concurrent && IsCatalogRelation(heapRelation))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("concurrent index creation on system catalog tables is not supported")));
  }

     
                                                                            
                                                                    
     
  if (concurrent && is_exclusion)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("concurrent index creation for exclusion constraints is not supported")));
  }

     
                                                                      
                                                                     
     
  if (shared_relation && !IsBootstrapProcessingMode())
  {
    ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("shared indexes cannot be created after initdb")));
  }

     
                                                                   
     
  if (shared_relation && tableSpaceId != GLOBALTABLESPACE_OID)
  {
    elog(ERROR, "shared relations must be placed in pg_global tablespace");
  }

     
                                                                   
                                                                           
                                                                         
                    
     
  if (get_relname_relid(indexRelationName, namespaceId))
  {
    if ((flags & INDEX_CREATE_IF_NOT_EXISTS) != 0)
    {
      ereport(NOTICE, (errcode(ERRCODE_DUPLICATE_TABLE), errmsg("relation \"%s\" already exists, skipping", indexRelationName)));
      table_close(pg_class, RowExclusiveLock);
      return InvalidOid;
    }

    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_TABLE), errmsg("relation \"%s\" already exists", indexRelationName)));
  }

  if ((flags & INDEX_CREATE_ADD_CONSTRAINT) != 0 && ConstraintNameIsUsed(CONSTRAINT_RELATION, heapRelationId, indexRelationName))
  {
       
                                                                 
                                               
       
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("constraint \"%s\" for relation \"%s\" already exists", indexRelationName, RelationGetRelationName(heapRelation))));
  }

     
                                                 
     
  indexTupDesc = ConstructTupleDescriptor(heapRelation, indexInfo, indexColNames, accessMethodObjectId, collationObjectId, classObjectId);

     
                                                                     
     
                                                                      
                                                                   
     
  if (!OidIsValid(indexRelationId))
  {
                                                                   
    if (IsBinaryUpgrade)
    {
      if (!OidIsValid(binary_upgrade_next_index_pg_class_oid))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("pg_class index OID value not set when in binary upgrade mode")));
      }

      indexRelationId = binary_upgrade_next_index_pg_class_oid;
      binary_upgrade_next_index_pg_class_oid = InvalidOid;
    }
    else
    {
      indexRelationId = GetNewRelFileNode(tableSpaceId, pg_class, relpersistence);
    }
  }

     
                                                                       
                                                                   
                                                            
     
  indexRelation = heap_create(indexRelationName, namespaceId, tableSpaceId, indexRelationId, relFileNode, accessMethodObjectId, indexTupDesc, relkind, relpersistence, shared_relation, mapped_relation, allow_system_table_mods, &relfrozenxid, &relminmxid);

  Assert(relfrozenxid == InvalidTransactionId);
  Assert(relminmxid == InvalidMultiXactId);
  Assert(indexRelationId == RelationGetRelid(indexRelation));

     
                                                                             
                                                                       
                                       
     
  LockRelation(indexRelation, AccessExclusiveLock);

     
                                                                             
                     
     
                                                               
     
  indexRelation->rd_rel->relowner = heapRelation->rd_rel->relowner;
  indexRelation->rd_rel->relam = accessMethodObjectId;
  indexRelation->rd_rel->relispartition = OidIsValid(parentIndexRelid);

     
                                  
     
  InsertPgClassTuple(pg_class, indexRelation, RelationGetRelid(indexRelation), (Datum)0, reloptions);

                          
  table_close(pg_class, RowExclusiveLock);

     
                                                                        
                                       
     
  InitializeAttributeOids(indexRelation, indexInfo->ii_NumIndexAttrs, indexRelationId);

     
                                           
     
  AppendAttributeTuples(indexRelation, indexInfo->ii_NumIndexAttrs);

                      
                       
                            
     
                                                                  
                                                                           
                      
     
  UpdateIndexRelation(indexRelationId, heapRelationId, parentIndexRelid, indexInfo, collationObjectId, classObjectId, coloptions, isprimary, is_exclusion, (constr_flags & INDEX_CONSTR_CREATE_DEFERRABLE) == 0, !concurrent && !invalid, !concurrent);

     
                                                                      
                                            
     
  CacheInvalidateRelcache(heapRelation);

                                                                     
  if (OidIsValid(parentIndexRelid))
  {
    StoreSingleInheritance(indexRelationId, parentIndexRelid, 1);
    SetRelationHasSubclass(parentIndexRelid, true);
  }

     
                                                         
     
                                                                         
                                                                          
                                                                       
                                       
     
                                                                         
                                               
     
                                                                           
                                  
     
  if (!IsBootstrapProcessingMode())
  {
    ObjectAddress myself, referenced;

    myself.classId = RelationRelationId;
    myself.objectId = indexRelationId;
    myself.objectSubId = 0;

    if ((flags & INDEX_CREATE_ADD_CONSTRAINT) != 0)
    {
      char constraintType;
      ObjectAddress localaddr;

      if (isprimary)
      {
        constraintType = CONSTRAINT_PRIMARY;
      }
      else if (indexInfo->ii_Unique)
      {
        constraintType = CONSTRAINT_UNIQUE;
      }
      else if (is_exclusion)
      {
        constraintType = CONSTRAINT_EXCLUSION;
      }
      else
      {
        elog(ERROR, "constraint must be PRIMARY, UNIQUE or EXCLUDE");
        constraintType = 0;                          
      }

      localaddr = index_constraint_create(heapRelation, indexRelationId, parentConstraintId, indexInfo, indexRelationName, constraintType, constr_flags, allow_system_table_mods, is_internal);
      if (constraintId)
      {
        *constraintId = localaddr.objectId;
      }
    }
    else
    {
      bool have_simple_col = false;

                                                                 
      for (i = 0; i < indexInfo->ii_NumIndexAttrs; i++)
      {
        if (indexInfo->ii_IndexAttrNumbers[i] != 0)
        {
          referenced.classId = RelationRelationId;
          referenced.objectId = heapRelationId;
          referenced.objectSubId = indexInfo->ii_IndexAttrNumbers[i];

          recordDependencyOn(&myself, &referenced, DEPENDENCY_AUTO);

          have_simple_col = true;
        }
      }

         
                                                                      
                                                                       
                                                                        
                                                           
         
      if (!have_simple_col)
      {
        referenced.classId = RelationRelationId;
        referenced.objectId = heapRelationId;
        referenced.objectSubId = 0;

        recordDependencyOn(&myself, &referenced, DEPENDENCY_AUTO);
      }
    }

       
                                                                       
                                                                      
                                                                        
                                                                 
       
    if (OidIsValid(parentIndexRelid))
    {
      referenced.classId = RelationRelationId;
      referenced.objectId = parentIndexRelid;
      referenced.objectSubId = 0;

      recordDependencyOn(&myself, &referenced, DEPENDENCY_PARTITION_PRI);

      referenced.classId = RelationRelationId;
      referenced.objectId = heapRelationId;
      referenced.objectSubId = 0;

      recordDependencyOn(&myself, &referenced, DEPENDENCY_PARTITION_SEC);
    }

                                        
                                                                       
    for (i = 0; i < indexInfo->ii_NumIndexKeyAttrs; i++)
    {
      if (OidIsValid(collationObjectId[i]) && collationObjectId[i] != DEFAULT_COLLATION_OID)
      {
        referenced.classId = CollationRelationId;
        referenced.objectId = collationObjectId[i];
        referenced.objectSubId = 0;

        recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
      }
    }

                                              
    for (i = 0; i < indexInfo->ii_NumIndexKeyAttrs; i++)
    {
      referenced.classId = OperatorClassRelationId;
      referenced.objectId = classObjectId[i];
      referenced.objectSubId = 0;

      recordDependencyOn(&myself, &referenced, DEPENDENCY_NORMAL);
    }

                                                                       
    if (indexInfo->ii_Expressions)
    {
      recordDependencyOnSingleRelExpr(&myself, (Node *)indexInfo->ii_Expressions, heapRelationId, DEPENDENCY_NORMAL, DEPENDENCY_AUTO, false);
    }

                                                               
    if (indexInfo->ii_Predicate)
    {
      recordDependencyOnSingleRelExpr(&myself, (Node *)indexInfo->ii_Predicate, heapRelationId, DEPENDENCY_NORMAL, DEPENDENCY_AUTO, false);
    }
  }
  else
  {
                                                                         
    Assert((flags & INDEX_CREATE_ADD_CONSTRAINT) == 0);
  }

                                        
  InvokeObjectPostCreateHookArg(RelationRelationId, indexRelationId, 0, is_internal);

     
                                                                      
                                   
     
  CommandCounterIncrement();

     
                                                                             
                                                                          
                                                                            
                              
     
  if (IsBootstrapProcessingMode())
  {
    RelationInitIndexAccessInfo(indexRelation);
  }
  else
  {
    Assert(indexRelation->rd_indexcxt != NULL);
  }

  indexRelation->rd_index->indnkeyatts = indexInfo->ii_NumIndexKeyAttrs;

     
                                                                            
                                                                         
                                                                            
                                                                           
                                                                          
                                                                        
            
     
  if (IsBootstrapProcessingMode())
  {
    index_register(heapRelationId, indexRelationId, indexInfo);
  }
  else if ((flags & INDEX_CREATE_SKIP_BUILD) != 0)
  {
       
                                                                       
                                                                           
                        
       
    index_update_stats(heapRelation, true, -1.0);
                                       
    CommandCounterIncrement();
  }
  else
  {
    index_build(heapRelation, indexRelation, indexInfo, false, true);
  }

     
                                                                            
                                                                   
     
  index_close(indexRelation, NoLock);

  return indexRelationId;
}

   
                                  
   
                                                                               
                                                                            
                                                             
   
Oid
index_concurrently_create_copy(Relation heapRelation, Oid oldIndexId, const char *newName)
{
  Relation indexRelation;
  IndexInfo *oldInfo, *newInfo;
  Oid newIndexId = InvalidOid;
  HeapTuple indexTuple, classTuple;
  Datum indclassDatum, colOptionDatum, optionDatum;
  oidvector *indclass;
  int2vector *indcoloptions;
  bool isnull;
  List *indexColNames = NIL;
  List *indexExprs = NIL;
  List *indexPreds = NIL;

  indexRelation = index_open(oldIndexId, RowExclusiveLock);

                                                               
  oldInfo = BuildIndexInfo(indexRelation);

     
                                                                    
                
     
  if (oldInfo->ii_ExclusionOps != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("concurrent index creation for exclusion constraints is not supported")));
  }

                                                                     
  indexTuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(oldIndexId));
  if (!HeapTupleIsValid(indexTuple))
  {
    elog(ERROR, "cache lookup failed for index %u", oldIndexId);
  }
  indclassDatum = SysCacheGetAttr(INDEXRELID, indexTuple, Anum_pg_index_indclass, &isnull);
  Assert(!isnull);
  indclass = (oidvector *)DatumGetPointer(indclassDatum);

  colOptionDatum = SysCacheGetAttr(INDEXRELID, indexTuple, Anum_pg_index_indoption, &isnull);
  Assert(!isnull);
  indcoloptions = (int2vector *)DatumGetPointer(colOptionDatum);

                                     
  classTuple = SearchSysCache1(RELOID, oldIndexId);
  if (!HeapTupleIsValid(classTuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", oldIndexId);
  }
  optionDatum = SysCacheGetAttr(RELOID, classTuple, Anum_pg_class_reloptions, &isnull);

     
                                                                    
                                                                          
                                                             
     
  if (oldInfo->ii_Expressions != NIL)
  {
    Datum exprDatum;
    char *exprString;

    exprDatum = SysCacheGetAttr(INDEXRELID, indexTuple, Anum_pg_index_indexprs, &isnull);
    Assert(!isnull);
    exprString = TextDatumGetCString(exprDatum);
    indexExprs = (List *)stringToNode(exprString);
    pfree(exprString);
  }
  if (oldInfo->ii_Predicate != NIL)
  {
    Datum predDatum;
    char *predString;

    predDatum = SysCacheGetAttr(INDEXRELID, indexTuple, Anum_pg_index_indpred, &isnull);
    Assert(!isnull);
    predString = TextDatumGetCString(predDatum);
    indexPreds = (List *)stringToNode(predString);

                                             
    indexPreds = make_ands_implicit((Expr *)indexPreds);
    pfree(predString);
  }

     
                                                                          
                                                                            
                                                
     
  newInfo = makeIndexInfo(oldInfo->ii_NumIndexAttrs, oldInfo->ii_NumIndexKeyAttrs, oldInfo->ii_Am, indexExprs, indexPreds, oldInfo->ii_Unique, false,                            
      true);

     
                                                                         
                                                                         
               
     
  for (int i = 0; i < oldInfo->ii_NumIndexAttrs; i++)
  {
    TupleDesc indexTupDesc = RelationGetDescr(indexRelation);
    Form_pg_attribute att = TupleDescAttr(indexTupDesc, i);

    indexColNames = lappend(indexColNames, NameStr(att->attname));
    newInfo->ii_IndexAttrNumbers[i] = oldInfo->ii_IndexAttrNumbers[i];
  }

     
                               
     
                                                                         
                                                                           
                      
     
  newIndexId = index_create(heapRelation, newName, InvalidOid,                                                                                                                                                                                                           
      InvalidOid,                                                                                                                                                                                                                                                         
      InvalidOid,                                                                                                                                                                                                                                                           
      InvalidOid,                                                                                                                                                                                                                                                    
      newInfo, indexColNames, indexRelation->rd_rel->relam, indexRelation->rd_rel->reltablespace, indexRelation->rd_indcollation, indclass->values, indcoloptions->values, optionDatum, INDEX_CREATE_SKIP_BUILD | INDEX_CREATE_CONCURRENT, 0, true,                                          
      false,                                                                                                                                                                                                                                                          
      NULL);

                                             
  index_close(indexRelation, NoLock);
  ReleaseSysCache(indexTuple);
  ReleaseSysCache(classTuple);

  return newIndexId;
}

   
                            
   
                                                                           
                                                                             
                                                                          
                                                                         
                                                                          
   
void
index_concurrently_build(Oid heapRelationId, Oid indexRelationId)
{
  Relation heapRel;
  Oid save_userid;
  int save_sec_context;
  int save_nestlevel;
  Relation indexRelation;
  IndexInfo *indexInfo;

                                                           
  Assert(ActiveSnapshotSet());

                                              
  heapRel = table_open(heapRelationId, ShareUpdateExclusiveLock);

     
                                                                             
                                                                      
                                                                 
     
  GetUserIdAndSecContext(&save_userid, &save_sec_context);
  SetUserIdAndSecContext(heapRel->rd_rel->relowner, save_sec_context | SECURITY_RESTRICTED_OPERATION);
  save_nestlevel = NewGUCNestLevel();

  indexRelation = index_open(indexRelationId, RowExclusiveLock);

     
                                                                        
                                                                          
                        
     
  indexInfo = BuildIndexInfo(indexRelation);
  Assert(!indexInfo->ii_ReadyForInserts);
  indexInfo->ii_Concurrent = true;
  indexInfo->ii_BrokenHotChain = false;

                           
  index_build(heapRel, indexRelation, indexInfo, false, true);

                                                             
  AtEOXact_GUC(false, save_nestlevel);

                                           
  SetUserIdAndSecContext(save_userid, save_sec_context);

                                                    
  table_close(heapRel, NoLock);
  index_close(indexRelation, NoLock);

     
                                                                             
                                                                            
                                                                           
     
  index_set_state_flags(indexRelationId, INDEX_CREATE_SET_READY);
}

   
                           
   
                                                                             
                                                                       
   
void
index_concurrently_swap(Oid newIndexId, Oid oldIndexId, const char *oldName)
{
  Relation pg_class, pg_index, pg_constraint, pg_trigger;
  Relation oldClassRel, newClassRel;
  HeapTuple oldClassTuple, newClassTuple;
  Form_pg_class oldClassForm, newClassForm;
  HeapTuple oldIndexTuple, newIndexTuple;
  Form_pg_index oldIndexForm, newIndexForm;
  bool isPartition;
  Oid indexConstraintOid;
  List *constraintOids = NIL;
  ListCell *lc;

     
                                                                          
     
  oldClassRel = relation_open(oldIndexId, ShareUpdateExclusiveLock);
  newClassRel = relation_open(newIndexId, ShareUpdateExclusiveLock);

                                                        
  pg_class = table_open(RelationRelationId, RowExclusiveLock);

  oldClassTuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(oldIndexId));
  if (!HeapTupleIsValid(oldClassTuple))
  {
    elog(ERROR, "could not find tuple for relation %u", oldIndexId);
  }
  newClassTuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(newIndexId));
  if (!HeapTupleIsValid(newClassTuple))
  {
    elog(ERROR, "could not find tuple for relation %u", newIndexId);
  }

  oldClassForm = (Form_pg_class)GETSTRUCT(oldClassTuple);
  newClassForm = (Form_pg_class)GETSTRUCT(newClassTuple);

                      
  namestrcpy(&newClassForm->relname, NameStr(oldClassForm->relname));
  namestrcpy(&oldClassForm->relname, oldName);

                                                              
  isPartition = newClassForm->relispartition;
  newClassForm->relispartition = oldClassForm->relispartition;
  oldClassForm->relispartition = isPartition;

  CatalogTupleUpdate(pg_class, &oldClassTuple->t_self, oldClassTuple);
  CatalogTupleUpdate(pg_class, &newClassTuple->t_self, newClassTuple);

  heap_freetuple(oldClassTuple);
  heap_freetuple(newClassTuple);

                           
  pg_index = table_open(IndexRelationId, RowExclusiveLock);

  oldIndexTuple = SearchSysCacheCopy1(INDEXRELID, ObjectIdGetDatum(oldIndexId));
  if (!HeapTupleIsValid(oldIndexTuple))
  {
    elog(ERROR, "could not find tuple for relation %u", oldIndexId);
  }
  newIndexTuple = SearchSysCacheCopy1(INDEXRELID, ObjectIdGetDatum(newIndexId));
  if (!HeapTupleIsValid(newIndexTuple))
  {
    elog(ERROR, "could not find tuple for relation %u", newIndexId);
  }

  oldIndexForm = (Form_pg_index)GETSTRUCT(oldIndexTuple);
  newIndexForm = (Form_pg_index)GETSTRUCT(newIndexTuple);

     
                                                                            
                                  
     
  newIndexForm->indisprimary = oldIndexForm->indisprimary;
  oldIndexForm->indisprimary = false;
  newIndexForm->indisexclusion = oldIndexForm->indisexclusion;
  oldIndexForm->indisexclusion = false;
  newIndexForm->indimmediate = oldIndexForm->indimmediate;
  oldIndexForm->indimmediate = true;

                                                
  newIndexForm->indisreplident = oldIndexForm->indisreplident;

                                                
  newIndexForm->indisclustered = oldIndexForm->indisclustered;

     
                                                                            
                                        
     
  newIndexForm->indisvalid = true;
  oldIndexForm->indisvalid = false;
  oldIndexForm->indisclustered = false;
  oldIndexForm->indisreplident = false;

  CatalogTupleUpdate(pg_index, &oldIndexTuple->t_self, oldIndexTuple);
  CatalogTupleUpdate(pg_index, &newIndexTuple->t_self, newIndexTuple);

  heap_freetuple(oldIndexTuple);
  heap_freetuple(newIndexTuple);

     
                                                         
     

  constraintOids = get_index_ref_constraints(oldIndexId);

  indexConstraintOid = get_index_constraint(oldIndexId);

  if (OidIsValid(indexConstraintOid))
  {
    constraintOids = lappend_oid(constraintOids, indexConstraintOid);
  }

  pg_constraint = table_open(ConstraintRelationId, RowExclusiveLock);
  pg_trigger = table_open(TriggerRelationId, RowExclusiveLock);

  foreach (lc, constraintOids)
  {
    HeapTuple constraintTuple, triggerTuple;
    Form_pg_constraint conForm;
    ScanKeyData key[1];
    SysScanDesc scan;
    Oid constraintOid = lfirst_oid(lc);

                                                           
    constraintTuple = SearchSysCacheCopy1(CONSTROID, ObjectIdGetDatum(constraintOid));
    if (!HeapTupleIsValid(constraintTuple))
    {
      elog(ERROR, "could not find tuple for constraint %u", constraintOid);
    }

    conForm = ((Form_pg_constraint)GETSTRUCT(constraintTuple));

    if (conForm->conindid == oldIndexId)
    {
      conForm->conindid = newIndexId;

      CatalogTupleUpdate(pg_constraint, &constraintTuple->t_self, constraintTuple);
    }

    heap_freetuple(constraintTuple);

                                    
    ScanKeyInit(&key[0], Anum_pg_trigger_tgconstraint, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(constraintOid));

    scan = systable_beginscan(pg_trigger, TriggerConstraintIndexId, true, NULL, 1, key);

    while (HeapTupleIsValid((triggerTuple = systable_getnext(scan))))
    {
      Form_pg_trigger tgForm = (Form_pg_trigger)GETSTRUCT(triggerTuple);

      if (tgForm->tgconstrindid != oldIndexId)
      {
        continue;
      }

                                  
      triggerTuple = heap_copytuple(triggerTuple);
      tgForm = (Form_pg_trigger)GETSTRUCT(triggerTuple);

      tgForm->tgconstrindid = newIndexId;

      CatalogTupleUpdate(pg_trigger, &triggerTuple->t_self, triggerTuple);

      heap_freetuple(triggerTuple);
    }

    systable_endscan(scan);
  }

     
                         
     
  {
    Relation description;
    ScanKeyData skey[3];
    SysScanDesc sd;
    HeapTuple tuple;
    Datum values[Natts_pg_description] = {0};
    bool nulls[Natts_pg_description] = {0};
    bool replaces[Natts_pg_description] = {0};

    values[Anum_pg_description_objoid - 1] = ObjectIdGetDatum(newIndexId);
    replaces[Anum_pg_description_objoid - 1] = true;

    ScanKeyInit(&skey[0], Anum_pg_description_objoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(oldIndexId));
    ScanKeyInit(&skey[1], Anum_pg_description_classoid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationRelationId));
    ScanKeyInit(&skey[2], Anum_pg_description_objsubid, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(0));

    description = table_open(DescriptionRelationId, RowExclusiveLock);

    sd = systable_beginscan(description, DescriptionObjIndexId, true, NULL, 3, skey);

    while ((tuple = systable_getnext(sd)) != NULL)
    {
      tuple = heap_modify_tuple(tuple, RelationGetDescr(description), values, nulls, replaces);
      CatalogTupleUpdate(description, &tuple->t_self, tuple);

      break;                                         
    }

    systable_endscan(sd);
    table_close(description, NoLock);
  }

     
                                                     
     
  if (get_rel_relispartition(oldIndexId))
  {
    List *ancestors = get_partition_ancestors(oldIndexId);
    Oid parentIndexRelid = linitial_oid(ancestors);

    DeleteInheritsTuple(oldIndexId, parentIndexRelid);
    StoreSingleInheritance(newIndexId, parentIndexRelid, 1);

    list_free(ancestors);
  }

     
                                                                       
                                                                            
                                                                 
     
  changeDependenciesOf(RelationRelationId, newIndexId, oldIndexId);
  changeDependenciesOn(RelationRelationId, newIndexId, oldIndexId);

  changeDependenciesOf(RelationRelationId, oldIndexId, newIndexId);
  changeDependenciesOn(RelationRelationId, oldIndexId, newIndexId);

     
                                                
     
  {
    PgStat_StatTabEntry *tabentry;

    tabentry = pgstat_fetch_stat_tabentry(oldIndexId);
    if (tabentry)
    {
      if (newClassRel->pgstat_info)
      {
        newClassRel->pgstat_info->t_counts.t_numscans = tabentry->numscans;
        newClassRel->pgstat_info->t_counts.t_tuples_returned = tabentry->tuples_returned;
        newClassRel->pgstat_info->t_counts.t_tuples_fetched = tabentry->tuples_fetched;
        newClassRel->pgstat_info->t_counts.t_blocks_fetched = tabentry->blocks_fetched;
        newClassRel->pgstat_info->t_counts.t_blocks_hit = tabentry->blocks_hit;

           
                                                                  
                 
           
      }
    }
  }

                                                                   
  CopyStatistics(oldIndexId, newIndexId);

                                                                
  {
    HeapTuple attrTuple;
    Relation pg_attribute;
    SysScanDesc scan;
    ScanKeyData key[1];

    pg_attribute = table_open(AttributeRelationId, RowExclusiveLock);
    ScanKeyInit(&key[0], Anum_pg_attribute_attrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(newIndexId));
    scan = systable_beginscan(pg_attribute, AttributeRelidNumIndexId, true, NULL, 1, key);

    while (HeapTupleIsValid((attrTuple = systable_getnext(scan))))
    {
      Form_pg_attribute att = (Form_pg_attribute)GETSTRUCT(attrTuple);
      Datum repl_val[Natts_pg_attribute];
      bool repl_null[Natts_pg_attribute];
      bool repl_repl[Natts_pg_attribute];
      int attstattarget;
      HeapTuple newTuple;

                                  
      if (att->attisdropped)
      {
        continue;
      }

         
                                                                         
         
      attstattarget = get_attstattarget(oldIndexId, att->attnum);

                                               
      if (attstattarget == att->attstattarget)
      {
        continue;
      }

      memset(repl_val, 0, sizeof(repl_val));
      memset(repl_null, false, sizeof(repl_null));
      memset(repl_repl, false, sizeof(repl_repl));

      repl_repl[Anum_pg_attribute_attstattarget - 1] = true;
      repl_val[Anum_pg_attribute_attstattarget - 1] = Int32GetDatum(attstattarget);

      newTuple = heap_modify_tuple(attrTuple, RelationGetDescr(pg_attribute), repl_val, repl_null, repl_repl);
      CatalogTupleUpdate(pg_attribute, &newTuple->t_self, newTuple);

      heap_freetuple(newTuple);
    }

    systable_endscan(scan);
    table_close(pg_attribute, RowExclusiveLock);
  }

                       
  table_close(pg_class, RowExclusiveLock);
  table_close(pg_index, RowExclusiveLock);
  table_close(pg_constraint, RowExclusiveLock);
  table_close(pg_trigger, RowExclusiveLock);

                                                                              
  relation_close(oldClassRel, NoLock);
  relation_close(newClassRel, NoLock);
}

   
                               
   
                                                                             
                                                                        
                                                                             
                                                                               
   
void
index_concurrently_set_dead(Oid heapId, Oid indexId)
{
  Relation userHeapRelation;
  Relation userIndexRelation;

     
                                                                             
                                                                          
                                                                           
               
     
  userHeapRelation = table_open(heapId, ShareUpdateExclusiveLock);
  userIndexRelation = index_open(indexId, ShareUpdateExclusiveLock);
  TransferPredicateLocksToHeapRelation(userIndexRelation);

     
                                                                             
                                                                       
                                                                        
     
  index_set_state_flags(indexId, INDEX_DROP_SET_DEAD);

     
                                                                          
                                                                        
                                           
     
  CacheInvalidateRelcache(userHeapRelation);

     
                                                                   
     
  table_close(userHeapRelation, NoLock);
  index_close(userIndexRelation, NoLock);
}

   
                           
   
                                                                              
            
   
                                                                            
                                     
                                                                       
                              
                                                               
                                                                       
                                                                    
                         
                                                                  
                                                                
                                                             
                                                                        
                                                              
                                                                      
                                 
                                                               
                                                             
   
ObjectAddress
index_constraint_create(Relation heapRelation, Oid indexRelationId, Oid parentConstraintId, IndexInfo *indexInfo, const char *constraintName, char constraintType, bits16 constr_flags, bool allow_system_table_mods, bool is_internal)
{
  Oid namespaceId = RelationGetNamespace(heapRelation);
  ObjectAddress myself, idxaddr;
  Oid conOid;
  bool deferrable;
  bool initdeferred;
  bool mark_as_primary;
  bool islocal;
  bool noinherit;
  int inhcount;

  deferrable = (constr_flags & INDEX_CONSTR_CREATE_DEFERRABLE) != 0;
  initdeferred = (constr_flags & INDEX_CONSTR_CREATE_INIT_DEFERRED) != 0;
  mark_as_primary = (constr_flags & INDEX_CONSTR_CREATE_MARK_AS_PRIMARY) != 0;

                                                                    
  Assert(!IsBootstrapProcessingMode());

                                        
  if (!allow_system_table_mods && IsSystemRelation(heapRelation) && IsNormalProcessingMode())
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("user-defined indexes on system catalog tables are not supported")));
  }

                                                                 
  if (indexInfo->ii_Expressions && constraintType != CONSTRAINT_EXCLUSION)
  {
    elog(ERROR, "constraints cannot have index expressions");
  }

     
                                                                           
                                                                          
                                                                            
     
                                                                           
                                                                         
                                                    
     
  if (constr_flags & INDEX_CONSTR_CREATE_REMOVE_OLD_DEPS)
  {
    deleteDependencyRecordsForClass(RelationRelationId, indexRelationId, RelationRelationId, DEPENDENCY_AUTO);
  }

  if (OidIsValid(parentConstraintId))
  {
    islocal = false;
    inhcount = 1;
    noinherit = false;
  }
  else
  {
    islocal = true;
    inhcount = 0;
    noinherit = true;
  }

     
                                      
     
  conOid = CreateConstraintEntry(constraintName, namespaceId, constraintType, deferrable, initdeferred, true, parentConstraintId, RelationGetRelid(heapRelation), indexInfo->ii_IndexAttrNumbers, indexInfo->ii_NumIndexKeyAttrs, indexInfo->ii_NumIndexAttrs, InvalidOid,                
      indexRelationId,                                                                                                                                                                                                                                                                    
      InvalidOid,                                                                                                                                                                                                                                                                              
      NULL, NULL, NULL, NULL, 0, ' ', ' ', ' ', indexInfo->ii_ExclusionOps, NULL,                                                                                                                                                                                                                   
      NULL, islocal, inhcount, noinherit, is_internal);

     
                                                                   
     
                                                                         
                                                                       
     
  ObjectAddressSet(myself, ConstraintRelationId, conOid);
  ObjectAddressSet(idxaddr, RelationRelationId, indexRelationId);
  recordDependencyOn(&idxaddr, &myself, DEPENDENCY_INTERNAL);

     
                                                                          
                                                                 
     
  if (OidIsValid(parentConstraintId))
  {
    ObjectAddress referenced;

    ObjectAddressSet(referenced, ConstraintRelationId, parentConstraintId);
    recordDependencyOn(&myself, &referenced, DEPENDENCY_PARTITION_PRI);
    ObjectAddressSet(referenced, RelationRelationId, RelationGetRelid(heapRelation));
    recordDependencyOn(&myself, &referenced, DEPENDENCY_PARTITION_SEC);
  }

     
                                                                     
                                                                             
                                       
     
  if (deferrable)
  {
    CreateTrigStmt *trigger;

    trigger = makeNode(CreateTrigStmt);
    trigger->trigname = (constraintType == CONSTRAINT_PRIMARY) ? "PK_ConstraintTrigger" : "Unique_ConstraintTrigger";
    trigger->relation = NULL;
    trigger->funcname = SystemFuncName("unique_key_recheck");
    trigger->args = NIL;
    trigger->row = true;
    trigger->timing = TRIGGER_TYPE_AFTER;
    trigger->events = TRIGGER_TYPE_INSERT | TRIGGER_TYPE_UPDATE;
    trigger->columns = NIL;
    trigger->whenClause = NULL;
    trigger->isconstraint = true;
    trigger->deferrable = true;
    trigger->initdeferred = initdeferred;
    trigger->constrrel = NULL;

    (void)CreateTrigger(trigger, NULL, RelationGetRelid(heapRelation), InvalidOid, conOid, indexRelationId, InvalidOid, InvalidOid, NULL, true, false);
  }

     
                                                                       
     
                                                                             
                                                                           
                                                                          
                   
     
  if ((constr_flags & INDEX_CONSTR_CREATE_UPDATE_INDEX) && (mark_as_primary || deferrable))
  {
    Relation pg_index;
    HeapTuple indexTuple;
    Form_pg_index indexForm;
    bool dirty = false;
    bool marked_as_primary = false;

    pg_index = table_open(IndexRelationId, RowExclusiveLock);

    indexTuple = SearchSysCacheCopy1(INDEXRELID, ObjectIdGetDatum(indexRelationId));
    if (!HeapTupleIsValid(indexTuple))
    {
      elog(ERROR, "cache lookup failed for index %u", indexRelationId);
    }
    indexForm = (Form_pg_index)GETSTRUCT(indexTuple);

    if (mark_as_primary && !indexForm->indisprimary)
    {
      indexForm->indisprimary = true;
      dirty = true;
      marked_as_primary = true;
    }

    if (deferrable && indexForm->indimmediate)
    {
      indexForm->indimmediate = false;
      dirty = true;
    }

    if (dirty)
    {
      CatalogTupleUpdate(pg_index, &indexTuple->t_self, indexTuple);

         
                                                                     
                                                                     
                                                                        
                                                        
         
      if (marked_as_primary)
      {
        CacheInvalidateRelcache(heapRelation);
      }

      InvokeObjectPostAlterHookArg(IndexRelationId, indexRelationId, 0, InvalidOid, is_internal);
    }

    heap_freetuple(indexTuple);
    table_close(pg_index, RowExclusiveLock);
  }

  return myself;
}

   
               
   
                                                                           
                                                     
   
                                                                          
                                                                           
                                                                             
                 
   
void
index_drop(Oid indexId, bool concurrent, bool concurrent_lock_mode)
{
  Oid heapId;
  Relation userHeapRelation;
  Relation userIndexRelation;
  Relation indexRelation;
  HeapTuple tuple;
  bool hasexprs;
  LockRelId heaprelid, indexrelid;
  LOCKTAG heaplocktag;
  LOCKMODE lockmode;

     
                                                                            
                                                                            
                                                                          
                     
     
  Assert(get_rel_persistence(indexId) != RELPERSISTENCE_TEMP || (!concurrent && !concurrent_lock_mode));

     
                                                                        
                                                                       
                                                                            
                                                                           
                                                                         
                                                                       
                                                                         
                                                   
     
                                                                             
                                                                            
                                                                             
                                                                          
                                                                           
                                                                            
                
     
  heapId = IndexGetRelation(indexId, false);
  lockmode = (concurrent || concurrent_lock_mode) ? ShareUpdateExclusiveLock : AccessExclusiveLock;
  userHeapRelation = table_open(heapId, lockmode);
  userIndexRelation = index_open(indexId, lockmode);

     
                                                                             
                                                      
     
  CheckTableNotInUse(userIndexRelation, "DROP INDEX");

     
                                                                           
                         
     
                                                                            
                                                                            
                                                                        
                                                                           
                                                                           
                                        
     
                                                                        
                                                                        
                                                                            
                                                                            
                                                                            
                                                                             
                                                                            
                         
     
                                                                             
                                                               
                                                                          
                                                                            
                                                                          
                                                                            
                                                                             
                                                                           
                                                                           
                                     
     
  if (concurrent)
  {
       
                                                                          
                                                                          
                                                                      
                                                                    
                                                                          
                                                                         
                                                                         
                                                                        
                                                                        
                                                                          
                                      
       
    if (GetTopTransactionIdIfAny() != InvalidTransactionId)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("DROP INDEX CONCURRENTLY must be first action in transaction")));
    }

       
                                                         
       
    index_set_state_flags(indexId, INDEX_DROP_CLEAR_VALID);

       
                                                                        
                                                                           
              
       
    CacheInvalidateRelcache(userHeapRelation);

                                                                         
    heaprelid = userHeapRelation->rd_lockInfo.lockRelId;
    SET_LOCKTAG_RELATION(heaplocktag, heaprelid.dbId, heaprelid.relId);
    indexrelid = userIndexRelation->rd_lockInfo.lockRelId;

    table_close(userHeapRelation, NoLock);
    index_close(userIndexRelation, NoLock);

       
                                                                     
                                                                         
                                                                      
                                                                      
       
                                                                           
                                                                           
                                                                         
                                                             
       
    LockRelationIdForSession(&heaprelid, ShareUpdateExclusiveLock);
    LockRelationIdForSession(&indexrelid, ShareUpdateExclusiveLock);

    PopActiveSnapshot();
    CommitTransactionCommand();
    StartTransactionCommand();

       
                                                                        
                                                                     
                                                                           
                                                                           
                                                                           
                     
       
                                                                         
                                                                     
                                                                         
                                                                      
                                               
       
                                                                         
                                                                           
                                                                    
       
    WaitForLockers(heaplocktag, AccessExclusiveLock, true);

                                                          
    index_concurrently_set_dead(heapId, indexId);

       
                                                                         
                          
       
    CommitTransactionCommand();
    StartTransactionCommand();

       
                                                                    
                                                      
       
    WaitForLockers(heaplocktag, AccessExclusiveLock, true);

       
                                                              
       
                                                                      
                                                                         
                                     
       
    userHeapRelation = table_open(heapId, ShareUpdateExclusiveLock);
    userIndexRelation = index_open(indexId, AccessExclusiveLock);
  }
  else
  {
                                                                         
    TransferPredicateLocksToHeapRelation(userIndexRelation);
  }

     
                                                     
     
  if (userIndexRelation->rd_rel->relkind != RELKIND_PARTITIONED_INDEX)
  {
    RelationDropStorage(userIndexRelation);
  }

     
                                                                            
                                                                         
                  
     
  index_close(userIndexRelation, NoLock);

  RelationForgetRelation(indexId);

     
                                                          
     
  indexRelation = table_open(IndexRelationId, RowExclusiveLock);

  tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexId));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for index %u", indexId);
  }

  hasexprs = !heap_attisnull(tuple, Anum_pg_index_indexprs, RelationGetDescr(indexRelation));

  CatalogTupleDelete(indexRelation, &tuple->t_self);

  ReleaseSysCache(tuple);
  table_close(indexRelation, RowExclusiveLock);

     
                                                                             
           
     
  if (hasexprs)
  {
    RemoveStatistics(indexId, 0);
  }

     
                            
     
  DeleteAttributeTuples(indexId);

     
                           
     
  DeleteRelationTuple(indexId);

     
                           
     
  DeleteInheritsTuple(indexId, InvalidOid);

     
                                                                           
                                                                            
                                                                          
                                                                         
                                                                            
                                                       
     
  CacheInvalidateRelcache(userHeapRelation);

     
                                     
     
  table_close(userHeapRelation, NoLock);

     
                                             
     
  if (concurrent)
  {
    UnlockRelationIdForSession(&heaprelid, ShareUpdateExclusiveLock);
    UnlockRelationIdForSession(&indexrelid, ShareUpdateExclusiveLock);
  }
}

                                                                    
                            
                                                                    
   

                    
                   
                                                     
   
                                                                     
                                                                            
                                                                            
                                                                         
                    
   
IndexInfo *
BuildIndexInfo(Relation index)
{
  IndexInfo *ii = makeNode(IndexInfo);
  Form_pg_index indexStruct = index->rd_index;
  int i;
  int numAtts;

                                                                          
  numAtts = indexStruct->indnatts;
  if (numAtts < 1 || numAtts > INDEX_MAX_KEYS)
  {
    elog(ERROR, "invalid indnatts %d for index %u", numAtts, RelationGetRelid(index));
  }
  ii->ii_NumIndexAttrs = numAtts;
  ii->ii_NumIndexKeyAttrs = indexStruct->indnkeyatts;
  Assert(ii->ii_NumIndexKeyAttrs != 0);
  Assert(ii->ii_NumIndexKeyAttrs <= ii->ii_NumIndexAttrs);

  for (i = 0; i < numAtts; i++)
  {
    ii->ii_IndexAttrNumbers[i] = indexStruct->indkey.values[i];
  }

                                                             
  ii->ii_Expressions = RelationGetIndexExpressions(index);
  ii->ii_ExpressionsState = NIL;

                                    
  ii->ii_Predicate = RelationGetIndexPredicate(index);
  ii->ii_PredicateState = NULL;

                                              
  if (indexStruct->indisexclusion)
  {
    RelationGetExclusionInfo(index, &ii->ii_ExclusionOps, &ii->ii_ExclusionProcs, &ii->ii_ExclusionStrats);
  }
  else
  {
    ii->ii_ExclusionOps = NULL;
    ii->ii_ExclusionProcs = NULL;
    ii->ii_ExclusionStrats = NULL;
  }

                  
  ii->ii_Unique = indexStruct->indisunique;
  ii->ii_ReadyForInserts = indexStruct->indisready;
                                                      
  ii->ii_UniqueOps = NULL;
  ii->ii_UniqueProcs = NULL;
  ii->ii_UniqueStrats = NULL;

                                               
  ii->ii_Concurrent = false;
  ii->ii_BrokenHotChain = false;
  ii->ii_ParallelWorkers = 0;

                                           
  ii->ii_Am = index->rd_rel->relam;
  ii->ii_AmCache = NULL;
  ii->ii_Context = CurrentMemoryContext;

  return ii;
}

                    
                        
                                                          
   
                                                                           
                                                                          
                                                                             
                                                                             
                                                                             
                                                                              
                                                                            
                                             
                    
   
IndexInfo *
BuildDummyIndexInfo(Relation index)
{
  IndexInfo *ii;
  Form_pg_index indexStruct = index->rd_index;
  int i;
  int numAtts;

                                                                          
  numAtts = indexStruct->indnatts;
  if (numAtts < 1 || numAtts > INDEX_MAX_KEYS)
  {
    elog(ERROR, "invalid indnatts %d for index %u", numAtts, RelationGetRelid(index));
  }

     
                                                                             
                   
     
  ii = makeIndexInfo(indexStruct->indnatts, indexStruct->indnkeyatts, index->rd_rel->relam, RelationGetDummyIndexExpressions(index), NIL, indexStruct->indisunique, indexStruct->indisready, false);

                                 
  for (i = 0; i < numAtts; i++)
  {
    ii->ii_IndexAttrNumbers[i] = indexStruct->indkey.values[i];
  }

                                                 

  return ii;
}

   
                    
                                                                       
                                                    
   
                                                                           
                                                                     
   
                                                                       
   
bool
CompareIndexInfo(IndexInfo *info1, IndexInfo *info2, Oid *collations1, Oid *collations2, Oid *opfamilies1, Oid *opfamilies2, AttrNumber *attmap, int maplen)
{
  int i;

  if (info1->ii_Unique != info2->ii_Unique)
  {
    return false;
  }

                                                                       
  if (info1->ii_Am != info2->ii_Am)
  {
    return false;
  }

                                     
  if (info1->ii_NumIndexAttrs != info2->ii_NumIndexAttrs)
  {
    return false;
  }

                                         
  if (info1->ii_NumIndexKeyAttrs != info2->ii_NumIndexKeyAttrs)
  {
    return false;
  }

     
                                                                           
                                                                        
                                                                         
                             
     
  for (i = 0; i < info1->ii_NumIndexAttrs; i++)
  {
    if (maplen < info2->ii_IndexAttrNumbers[i])
    {
      elog(ERROR, "incorrect attribute map");
    }

                                          
    if ((info1->ii_IndexAttrNumbers[i] != InvalidAttrNumber) && (attmap[info2->ii_IndexAttrNumbers[i] - 1] != info1->ii_IndexAttrNumbers[i]))
    {
      return false;
    }

                                                                   
    if (i >= info1->ii_NumIndexKeyAttrs)
    {
      continue;
    }

    if (collations1[i] != collations2[i])
    {
      return false;
    }
    if (opfamilies1[i] != opfamilies2[i])
    {
      return false;
    }
  }

     
                                                                            
                                                       
     
  if ((info1->ii_Expressions != NIL) != (info2->ii_Expressions != NIL))
  {
    return false;
  }
  if (info1->ii_Expressions != NIL)
  {
    bool found_whole_row;
    Node *mapped;

    mapped = map_variable_attnos((Node *)info2->ii_Expressions, 1, 0, attmap, maplen, InvalidOid, &found_whole_row);
    if (found_whole_row)
    {
         
                                                                       
                  
         
      return false;
    }

    if (!equal(info1->ii_Expressions, mapped))
    {
      return false;
    }
  }

                                                                 
  if ((info1->ii_Predicate == NULL) != (info2->ii_Predicate == NULL))
  {
    return false;
  }
  if (info1->ii_Predicate != NULL)
  {
    bool found_whole_row;
    Node *mapped;

    mapped = map_variable_attnos((Node *)info2->ii_Predicate, 1, 0, attmap, maplen, InvalidOid, &found_whole_row);
    if (found_whole_row)
    {
         
                                                                       
                  
         
      return false;
    }
    if (!equal(info1->ii_Predicate, mapped))
    {
      return false;
    }
  }

                                                             
  if (info1->ii_ExclusionOps != NULL || info2->ii_ExclusionOps != NULL)
  {
    return false;
  }

  return true;
}

                    
                              
                                         
   
                                                                              
                                                                             
                                                                         
   
                                                                            
                                                 
                    
   
void
BuildSpeculativeIndexInfo(Relation index, IndexInfo *ii)
{
  int indnkeyatts;
  int i;

  indnkeyatts = IndexRelationGetNumberOfKeyAttributes(index);

     
                                            
     
  Assert(ii->ii_Unique);

  if (index->rd_rel->relam != BTREE_AM_OID)
  {
    elog(ERROR, "unexpected non-btree speculative unique index");
  }

  ii->ii_UniqueOps = (Oid *)palloc(sizeof(Oid) * indnkeyatts);
  ii->ii_UniqueProcs = (Oid *)palloc(sizeof(Oid) * indnkeyatts);
  ii->ii_UniqueStrats = (uint16 *)palloc(sizeof(uint16) * indnkeyatts);

     
                                                                         
                                                         
     
                                                      
  for (i = 0; i < indnkeyatts; i++)
  {
    ii->ii_UniqueStrats[i] = BTEqualStrategyNumber;
    ii->ii_UniqueOps[i] = get_opfamily_member(index->rd_opfamily[i], index->rd_opcintype[i], index->rd_opcintype[i], ii->ii_UniqueStrats[i]);
    if (!OidIsValid(ii->ii_UniqueOps[i]))
    {
      elog(ERROR, "missing operator %d(%u,%u) in opfamily %u", ii->ii_UniqueStrats[i], index->rd_opcintype[i], index->rd_opcintype[i], index->rd_opfamily[i]);
    }
    ii->ii_UniqueProcs[i] = get_opcode(ii->ii_UniqueOps[i]);
  }
}

                    
                   
                                                                   
   
                                   
                                                              
                                                                
                                                
                                                      
   
                                                                          
                                                                         
                                                   
   
                                                                          
                                                                         
                                              
                    
   
void
FormIndexDatum(IndexInfo *indexInfo, TupleTableSlot *slot, EState *estate, Datum *values, bool *isnull)
{
  ListCell *indexpr_item;
  int i;

  if (indexInfo->ii_Expressions != NIL && indexInfo->ii_ExpressionsState == NIL)
  {
                                                                
    indexInfo->ii_ExpressionsState = ExecPrepareExprList(indexInfo->ii_Expressions, estate);
                                                   
    Assert(GetPerTupleExprContext(estate)->ecxt_scantuple == slot);
  }
  indexpr_item = list_head(indexInfo->ii_ExpressionsState);

  for (i = 0; i < indexInfo->ii_NumIndexAttrs; i++)
  {
    int keycol = indexInfo->ii_IndexAttrNumbers[i];
    Datum iDatum;
    bool isNull;

    if (keycol < 0)
    {
      iDatum = slot_getsysattr(slot, keycol, &isNull);
    }
    else if (keycol != 0)
    {
         
                                                                     
                     
         
      iDatum = slot_getattr(slot, keycol, &isNull);
    }
    else
    {
         
                                                   
         
      if (indexpr_item == NULL)
      {
        elog(ERROR, "wrong number of index expressions");
      }
      iDatum = ExecEvalExprSwitchContext((ExprState *)lfirst(indexpr_item), GetPerTupleExprContext(estate), &isNull);
      indexpr_item = lnext(indexpr_item);
    }
    values[i] = iDatum;
    isnull[i] = isNull;
  }

  if (indexpr_item != NULL)
  {
    elog(ERROR, "wrong number of index expressions");
  }
}

   
                                                                              
   
                                                                          
                                                                               
                                                                  
   
                                           
                                                                   
   
                                                                         
                                                           
   
                                                                               
                                                                             
                                                                              
                                                                           
                                                                         
                                                                         
                                                   
   
static void
index_update_stats(Relation rel, bool hasindex, double reltuples)
{
  Oid relid = RelationGetRelid(rel);
  Relation pg_class;
  HeapTuple tuple;
  Form_pg_class rd_rel;
  bool dirty;

     
                                                                  
                                                                     
     
                                                                       
     
                                                                            
                                                                          
                                                                
     
                                                                           
                                                                         
                                                                        
                                                                           
                                                                        
                                                                        
                               
     
                                                                  
                                                                          
                                                                           
                                                                           
                                                                
                                                                       
                                                                            
                              
     

  pg_class = table_open(RelationRelationId, RowExclusiveLock);

     
                                                                            
                                                                         
             
     
  if (IsBootstrapProcessingMode() || ReindexIsProcessingHeap(RelationRelationId))
  {
                                         
    TableScanDesc pg_class_scan;
    ScanKeyData key[1];

    ScanKeyInit(&key[0], Anum_pg_class_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));

    pg_class_scan = table_beginscan_catalog(pg_class, 1, key);
    tuple = heap_getnext(pg_class_scan, ForwardScanDirection);
    tuple = heap_copytuple(tuple);
    table_endscan(pg_class_scan);
  }
  else
  {
                                   
    tuple = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relid));
  }

  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "could not find tuple for relation %u", relid);
  }
  rd_rel = (Form_pg_class)GETSTRUCT(tuple);

                                                 
  Assert(rd_rel->relkind != RELKIND_PARTITIONED_INDEX);

                                                       

  dirty = false;
  if (rd_rel->relhasindex != hasindex)
  {
    rd_rel->relhasindex = hasindex;
    dirty = true;
  }

  if (reltuples >= 0)
  {
    BlockNumber relpages = RelationGetNumberOfBlocks(rel);
    BlockNumber relallvisible;

    if (rd_rel->relkind != RELKIND_INDEX)
    {
      visibilitymap_count(rel, &relallvisible, NULL);
    }
    else                               
    {
      relallvisible = 0;
    }

    if (rd_rel->relpages != (int32)relpages)
    {
      rd_rel->relpages = (int32)relpages;
      dirty = true;
    }
    if (rd_rel->reltuples != (float4)reltuples)
    {
      rd_rel->reltuples = (float4)reltuples;
      dirty = true;
    }
    if (rd_rel->relallvisible != (int32)relallvisible)
    {
      rd_rel->relallvisible = (int32)relallvisible;
      dirty = true;
    }
  }

     
                                              
     
  if (dirty)
  {
    heap_inplace_update(pg_class, tuple);
                                               
  }
  else
  {
                                                                  
    CacheInvalidateRelcacheByTuple(tuple);
  }

  heap_freetuple(tuple);

  table_close(pg_class, RowExclusiveLock);
}

   
                                                                     
   
                                                                          
                                                                      
                                                                         
                                                                      
                                                             
   
                                                                      
                                                    
   
                                                                     
                                                                           
                                                           
   
void
index_build(Relation heapRelation, Relation indexRelation, IndexInfo *indexInfo, bool isreindex, bool parallel)
{
  IndexBuildResult *stats;
  Oid save_userid;
  int save_sec_context;
  int save_nestlevel;

     
                   
     
  Assert(RelationIsValid(indexRelation));
  Assert(PointerIsValid(indexRelation->rd_indam));
  Assert(PointerIsValid(indexRelation->rd_indam->ambuild));
  Assert(PointerIsValid(indexRelation->rd_indam->ambuildempty));

     
                                                                             
                                                 
     
                                                         
     
  if (parallel && IsNormalProcessingMode() && indexRelation->rd_rel->relam == BTREE_AM_OID)
  {
    indexInfo->ii_ParallelWorkers = plan_create_index_workers(RelationGetRelid(heapRelation), RelationGetRelid(indexRelation));
  }

  if (indexInfo->ii_ParallelWorkers == 0)
  {
    ereport(DEBUG1, (errmsg("building index \"%s\" on table \"%s\" serially", RelationGetRelationName(indexRelation), RelationGetRelationName(heapRelation))));
  }
  else
  {
    ereport(DEBUG1, (errmsg_plural("building index \"%s\" on table \"%s\" with request for %d parallel worker", "building index \"%s\" on table \"%s\" with request for %d parallel workers", indexInfo->ii_ParallelWorkers, RelationGetRelationName(indexRelation), RelationGetRelationName(heapRelation), indexInfo->ii_ParallelWorkers)));
  }

     
                                                                             
                                                                      
                                                                 
     
  GetUserIdAndSecContext(&save_userid, &save_sec_context);
  SetUserIdAndSecContext(heapRelation->rd_rel->relowner, save_sec_context | SECURITY_RESTRICTED_OPERATION);
  save_nestlevel = NewGUCNestLevel();

                                             
  {
    const int index[] = {PROGRESS_CREATEIDX_PHASE, PROGRESS_CREATEIDX_SUBPHASE, PROGRESS_CREATEIDX_TUPLES_DONE, PROGRESS_CREATEIDX_TUPLES_TOTAL, PROGRESS_SCAN_BLOCKS_DONE, PROGRESS_SCAN_BLOCKS_TOTAL};
    const int64 val[] = {PROGRESS_CREATEIDX_PHASE_BUILD, PROGRESS_CREATEIDX_SUBPHASE_INITIALIZE, 0, 0, 0, 0};

    pgstat_progress_update_multi_param(6, index, val);
  }

     
                                              
     
  stats = indexRelation->rd_indam->ambuild(heapRelation, indexRelation, indexInfo);
  Assert(PointerIsValid(stats));

     
                                                                             
                                                                        
                                                                        
                                                                     
                                                                  
     
  if (indexRelation->rd_rel->relpersistence == RELPERSISTENCE_UNLOGGED && !smgrexists(RelationGetSmgr(indexRelation), INIT_FORKNUM))
  {
    smgrcreate(RelationGetSmgr(indexRelation), INIT_FORKNUM, false);
    indexRelation->rd_indam->ambuildempty(indexRelation);
  }

     
                                                                          
                                                                            
                                                                           
                                                                            
                                                                        
                                                                             
                                                                             
                                                     
     
                                                                            
                                                                           
                                                                      
                                                                           
                                                                       
                                                                             
                                                                 
     
                                                                        
                                                                           
                                                                      
     
                                                                       
                                                                         
                                                                        
                                                                         
                                                                             
             
     
  if ((indexInfo->ii_BrokenHotChain || EarlyPruningEnabled(heapRelation)) && !isreindex && !indexInfo->ii_Concurrent)
  {
    Oid indexId = RelationGetRelid(indexRelation);
    Relation pg_index;
    HeapTuple indexTuple;
    Form_pg_index indexForm;

    pg_index = table_open(IndexRelationId, RowExclusiveLock);

    indexTuple = SearchSysCacheCopy1(INDEXRELID, ObjectIdGetDatum(indexId));
    if (!HeapTupleIsValid(indexTuple))
    {
      elog(ERROR, "cache lookup failed for index %u", indexId);
    }
    indexForm = (Form_pg_index)GETSTRUCT(indexTuple);

                                                                
    Assert(!indexForm->indcheckxmin);

    indexForm->indcheckxmin = true;
    CatalogTupleUpdate(pg_index, &indexTuple->t_self, indexTuple);

    heap_freetuple(indexTuple);
    table_close(pg_index, RowExclusiveLock);
  }

     
                                         
     
  index_update_stats(heapRelation, true, stats->heap_tuples);

  index_update_stats(indexRelation, false, stats->index_tuples);

                                                     
  CommandCounterIncrement();

     
                                                                           
                                                                            
                                                                             
                                            
     
  if (indexInfo->ii_ExclusionOps != NULL)
  {
    IndexCheckExclusion(heapRelation, indexRelation, indexInfo);
  }

                                                             
  AtEOXact_GUC(false, save_nestlevel);

                                           
  SetUserIdAndSecContext(save_userid, save_sec_context);
}

   
                                                                             
   
                                                                            
                                                                            
                                                                              
                                                                            
                                                                             
                                                                               
                                                                         
                                                                
   
static void
IndexCheckExclusion(Relation heapRelation, Relation indexRelation, IndexInfo *indexInfo)
{
  TableScanDesc scan;
  Datum values[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];
  ExprState *predicate;
  TupleTableSlot *slot;
  EState *estate;
  ExprContext *econtext;
  Snapshot snapshot;

     
                                                                       
                                                                             
                                                                             
     
  if (ReindexIsCurrentlyProcessingIndex(RelationGetRelid(indexRelation)))
  {
    ResetReindexProcessing();
  }

     
                                                                          
                                                         
     
  estate = CreateExecutorState();
  econtext = GetPerTupleExprContext(estate);
  slot = table_slot_create(heapRelation, NULL);

                                                                    
  econtext->ecxt_scantuple = slot;

                                                     
  predicate = ExecPrepareQual(indexInfo->ii_Predicate, estate);

     
                                                
     
  snapshot = RegisterSnapshot(GetLatestSnapshot());
  scan = table_beginscan_strat(heapRelation,               
      snapshot,                                            
      0,                                                         
      NULL,                                                
      true,                                                                 
      true);                                                  

  while (table_scan_getnextslot(scan, ForwardScanDirection, slot))
  {
    CHECK_FOR_INTERRUPTS();

       
                                                                           
       
    if (predicate != NULL)
    {
      if (!ExecQual(predicate, econtext))
      {
        continue;
      }
    }

       
                                                                     
       
    FormIndexDatum(indexInfo, slot, estate, values, isnull);

       
                                               
       
    check_exclusion_constraint(heapRelation, indexRelation, indexInfo, &(slot->tts_tid), values, isnull, estate, true);

    MemoryContextReset(econtext->ecxt_per_tuple_memory);
  }

  table_endscan(scan);
  UnregisterSnapshot(snapshot);

  ExecDropSingleTupleTableSlot(slot);

  FreeExecutorState(estate);

                                                           
  indexInfo->ii_ExpressionsState = NIL;
  indexInfo->ii_PredicateState = NULL;
}

   
                                                             
   
                                                                               
                                                                           
                                                                            
                                                                            
                                                                             
                                                                            
                                                                             
                                                                            
                                                                   
                                                                    
                                                                              
                                                                         
                                                                           
                                                                        
                                                                       
                                                                             
                                                                            
                                                                            
   
                                                                         
                                                                           
                                                                            
                                                                             
                                                                           
                                                                             
                                                                       
                                                                       
                                                                             
                                                                           
                                                               
   
                                                                           
                                                                            
                                                                         
                                                                            
                                                                              
                                                                             
                                                                          
   
                                                                        
                                                                        
                                                                            
                                                                              
                                                                           
                                                                         
                                                                          
                                          
   
                                                                          
                                                                      
                                                                        
                                                                          
                                                                            
                                                    
   
                                                                             
                                                                           
                                                                         
                                
   
void
validate_index(Oid heapId, Oid indexId, Snapshot snapshot)
{
  Relation heapRelation, indexRelation;
  IndexInfo *indexInfo;
  IndexVacuumInfo ivinfo;
  ValidateIndexState state;
  Oid save_userid;
  int save_sec_context;
  int save_nestlevel;

  {
    const int index[] = {PROGRESS_CREATEIDX_PHASE, PROGRESS_CREATEIDX_TUPLES_DONE, PROGRESS_CREATEIDX_TUPLES_TOTAL, PROGRESS_SCAN_BLOCKS_DONE, PROGRESS_SCAN_BLOCKS_TOTAL};
    const int64 val[] = {PROGRESS_CREATEIDX_PHASE_VALIDATE_IDXSCAN, 0, 0, 0, 0};

    pgstat_progress_update_multi_param(5, index, val);
  }

                                              
  heapRelation = table_open(heapId, ShareUpdateExclusiveLock);

     
                                                                             
                                                                      
                                                                 
     
  GetUserIdAndSecContext(&save_userid, &save_sec_context);
  SetUserIdAndSecContext(heapRelation->rd_rel->relowner, save_sec_context | SECURITY_RESTRICTED_OPERATION);
  save_nestlevel = NewGUCNestLevel();

  indexRelation = index_open(indexId, RowExclusiveLock);

     
                                                                          
                                                                         
                                            
     
  indexInfo = BuildIndexInfo(indexRelation);

                                                     
  indexInfo->ii_Concurrent = true;

     
                                                                        
     
  ivinfo.index = indexRelation;
  ivinfo.analyze_only = false;
  ivinfo.report_progress = true;
  ivinfo.estimated_count = true;
  ivinfo.message_level = DEBUG2;
  ivinfo.num_heap_tuples = heapRelation->rd_rel->reltuples;
  ivinfo.strategy = NULL;

     
                                                                           
                                                                             
                                                                   
                                      
     
  state.tuplesort = tuplesort_begin_datum(INT8OID, Int8LessOperator, InvalidOid, false, maintenance_work_mem, NULL, false);
  state.htups = state.itups = state.tups_inserted = 0;

                                             
  (void)index_bulk_delete(&ivinfo, NULL, validate_index_callback, (void *)&state);

                        
  {
    const int index[] = {PROGRESS_CREATEIDX_PHASE, PROGRESS_SCAN_BLOCKS_DONE, PROGRESS_SCAN_BLOCKS_TOTAL};
    const int64 val[] = {PROGRESS_CREATEIDX_PHASE_VALIDATE_SORT, 0, 0};

    pgstat_progress_update_multi_param(3, index, val);
  }
  tuplesort_performsort(state.tuplesort);

     
                                                     
     
  pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE, PROGRESS_CREATEIDX_PHASE_VALIDATE_TABLESCAN);
  table_index_validate_scan(heapRelation, indexRelation, indexInfo, snapshot, &state);

                                  
  tuplesort_end(state.tuplesort);

  elog(DEBUG2, "validate_index found %.0f heap tuples, %.0f index tuples; inserted %.0f missing tuples", state.htups, state.itups, state.tups_inserted);

                                                             
  AtEOXact_GUC(false, save_nestlevel);

                                           
  SetUserIdAndSecContext(save_userid, save_sec_context);

                                  
  index_close(indexRelation, NoLock);
  table_close(heapRelation, NoLock);
}

   
                                                                           
   
static bool
validate_index_callback(ItemPointer itemptr, void *opaque)
{
  ValidateIndexState *state = (ValidateIndexState *)opaque;
  int64 encoded = itemptr_encode(itemptr);

  tuplesort_putdatum(state->tuplesort, Int64GetDatum(encoded), false);
  state->itups += 1;
  return false;                                     
}

   
                                                       
   
                                                                             
                                        
   
                                                                             
                                                                             
   
void
index_set_state_flags(Oid indexId, IndexStateFlagsAction action)
{
  Relation pg_index;
  HeapTuple indexTuple;
  Form_pg_index indexForm;

                                                                    
  pg_index = table_open(IndexRelationId, RowExclusiveLock);

  indexTuple = SearchSysCacheCopy1(INDEXRELID, ObjectIdGetDatum(indexId));
  if (!HeapTupleIsValid(indexTuple))
  {
    elog(ERROR, "cache lookup failed for index %u", indexId);
  }
  indexForm = (Form_pg_index)GETSTRUCT(indexTuple);

                                                      
  switch (action)
  {
  case INDEX_CREATE_SET_READY:
                                                                    
    Assert(indexForm->indislive);
    Assert(!indexForm->indisready);
    Assert(!indexForm->indisvalid);
    indexForm->indisready = true;
    break;
  case INDEX_CREATE_SET_VALID:
                                                                    
    Assert(indexForm->indislive);
    Assert(indexForm->indisready);
    Assert(!indexForm->indisvalid);
    indexForm->indisvalid = true;
    break;
  case INDEX_DROP_CLEAR_VALID:

       
                                                                  
       
                                                                     
                                                                       
                                                                       
                                                                    
                                                  
       
                                                                     
                                                              
                                               
       
    indexForm->indisvalid = false;
    indexForm->indisclustered = false;
    indexForm->indisreplident = false;
    break;
  case INDEX_DROP_SET_DEAD:

       
                                                                 
       
                                                                   
                                                                       
                         
       
    Assert(!indexForm->indisvalid);
    Assert(!indexForm->indisclustered);
    Assert(!indexForm->indisreplident);
    indexForm->indisready = false;
    indexForm->indislive = false;
    break;
  }

                         
  CatalogTupleUpdate(pg_index, &indexTuple->t_self, indexTuple);

  table_close(pg_index, RowExclusiveLock);
}

   
                                                                       
                                                       
   
Oid
IndexGetRelation(Oid indexId, bool missing_ok)
{
  HeapTuple tuple;
  Form_pg_index index;
  Oid result;

  tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexId));
  if (!HeapTupleIsValid(tuple))
  {
    if (missing_ok)
    {
      return InvalidOid;
    }
    elog(ERROR, "cache lookup failed for index %u", indexId);
  }
  index = (Form_pg_index)GETSTRUCT(tuple);
  Assert(index->indexrelid == indexId);

  result = index->indrelid;
  ReleaseSysCache(tuple);
  return result;
}

   
                                                                   
   
void
reindex_index(Oid indexId, bool skip_constraint_checks, char persistence, int options)
{
  Relation iRel, heapRelation;
  Oid heapId;
  Oid save_userid;
  int save_sec_context;
  int save_nestlevel;
  IndexInfo *indexInfo;
  volatile bool skipped_constraint = false;
  PGRUsage ru0;
  bool progress = (options & REINDEXOPT_REPORT_PROGRESS) != 0;

  pg_rusage_init(&ru0);

     
                                                                            
                                                                     
     
  heapId = IndexGetRelation(indexId, false);
  heapRelation = table_open(heapId, ShareLock);

     
                                                                             
                                                                      
                                                                 
     
  GetUserIdAndSecContext(&save_userid, &save_sec_context);
  SetUserIdAndSecContext(heapRelation->rd_rel->relowner, save_sec_context | SECURITY_RESTRICTED_OPERATION);
  save_nestlevel = NewGUCNestLevel();

  if (progress)
  {
    pgstat_progress_start_command(PROGRESS_COMMAND_CREATE_INDEX, heapId);
    pgstat_progress_update_param(PROGRESS_CREATEIDX_COMMAND, PROGRESS_CREATEIDX_COMMAND_REINDEX);
    pgstat_progress_update_param(PROGRESS_CREATEIDX_INDEX_OID, indexId);
  }

     
                                                                        
                                                                
     
  iRel = index_open(indexId, AccessExclusiveLock);

  if (progress)
  {
    pgstat_progress_update_param(PROGRESS_CREATEIDX_ACCESS_METHOD_OID, iRel->rd_rel->relam);
  }

     
                                                                      
                                                                
     
  if (iRel->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
  {
    elog(ERROR, "unsupported relation kind for index \"%s\"", RelationGetRelationName(iRel));
  }

     
                                                                          
                                          
     
  if (RELATION_IS_OTHER_TEMP(iRel))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot reindex temporary tables of other sessions")));
  }

     
                                                                        
                                                                          
                                         
     
  if (IsToastNamespace(RelationGetNamespace(iRel)) && !get_index_isvalid(indexId))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot reindex invalid index on TOAST table")));
  }

     
                                                                            
                                                         
     
  CheckTableNotInUse(iRel, "REINDEX INDEX");

     
                                                                            
                                         
     
  TransferPredicateLocksToHeapRelation(iRel);

                                         
  indexInfo = BuildIndexInfo(iRel);

                                                                    
  if (skip_constraint_checks)
  {
    if (indexInfo->ii_Unique || indexInfo->ii_ExclusionOps != NULL)
    {
      skipped_constraint = true;
    }
    indexInfo->ii_Unique = false;
    indexInfo->ii_ExclusionOps = NULL;
    indexInfo->ii_ExclusionProcs = NULL;
    indexInfo->ii_ExclusionStrats = NULL;
  }

                                                            
  SetReindexProcessing(heapId, indexId);

                                                    
  RelationSetNewRelfilenode(iRel, persistence);

                                        
                                                         
  index_build(heapRelation, iRel, indexInfo, true, true);

                                    
  ResetReindexProcessing();

     
                                                                           
                                                                             
                                                                            
                                                          
     
                                                                
                                                                        
                                                                         
                                                                           
                                                                         
                                                                          
                                                                             
                
     
                                                                            
                                                                       
                                                                      
                                                                          
                                                                            
                                                                        
                  
     
                                                                        
                                                                         
                                                                             
                           
     
                                                                      
                                                                            
                                                                             
                                       
     
  if (!skipped_constraint)
  {
    Relation pg_index;
    HeapTuple indexTuple;
    Form_pg_index indexForm;
    bool index_bad;
    bool early_pruning_enabled = EarlyPruningEnabled(heapRelation);

    pg_index = table_open(IndexRelationId, RowExclusiveLock);

    indexTuple = SearchSysCacheCopy1(INDEXRELID, ObjectIdGetDatum(indexId));
    if (!HeapTupleIsValid(indexTuple))
    {
      elog(ERROR, "cache lookup failed for index %u", indexId);
    }
    indexForm = (Form_pg_index)GETSTRUCT(indexTuple);

    index_bad = (!indexForm->indisvalid || !indexForm->indisready || !indexForm->indislive);
    if (index_bad || (indexForm->indcheckxmin && !indexInfo->ii_BrokenHotChain) || early_pruning_enabled)
    {
      if (!indexInfo->ii_BrokenHotChain && !early_pruning_enabled)
      {
        indexForm->indcheckxmin = false;
      }
      else if (index_bad || early_pruning_enabled)
      {
        indexForm->indcheckxmin = true;
      }
      indexForm->indisvalid = true;
      indexForm->indisready = true;
      indexForm->indislive = true;
      CatalogTupleUpdate(pg_index, &indexTuple->t_self, indexTuple);

         
                                                                        
                                                                         
                                                                   
                                                                         
                       
         
      CacheInvalidateRelcache(heapRelation);
    }

    table_close(pg_index, RowExclusiveLock);
  }

                       
  if (options & REINDEXOPT_VERBOSE)
  {
    ereport(INFO, (errmsg("index \"%s\" was reindexed", get_rel_name(indexId)), errdetail_internal("%s", pg_rusage_show(&ru0))));
  }

                                                             
  AtEOXact_GUC(false, save_nestlevel);

                                           
  SetUserIdAndSecContext(save_userid, save_sec_context);

                                  
  index_close(iRel, NoLock);
  table_close(heapRelation, NoLock);

  if (progress)
  {
    pgstat_progress_end_command();
  }
}

   
                                                                   
                                                                  
   
                                                                        
   
                                                                             
   
                                                                             
                                                                             
                                                                               
                                                                             
                                                                              
                                                                                
                                                                           
                                                                               
                                                   
   
                                                                        
                                                                          
                                                                               
                                                                           
                                                                              
                                                                    
   
                                                                           
                                
   
                                                                            
                                 
   
                                                                           
                                                                              
                  
   
bool
reindex_relation(Oid relid, int flags, int options)
{
  Relation rel;
  Oid toast_relid;
  List *indexIds;
  char persistence;
  bool result;
  ListCell *indexId;
  int i;

     
                                                                             
                                                                         
                                  
     
  rel = table_open(relid, ShareLock);

     
                                                                             
                                                                      
                                                                          
                                                      
     
  if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    ereport(WARNING, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("REINDEX of partitioned tables is not yet implemented, skipping \"%s\"", RelationGetRelationName(rel))));
    table_close(rel, ShareLock);
    return false;
  }

  toast_relid = rel->rd_rel->reltoastrelid;

     
                                                                     
                                                                    
               
     
  indexIds = RelationGetIndexList(rel);

  if (flags & REINDEX_REL_SUPPRESS_INDEX_USE)
  {
                                                                
    SetReindexPending(indexIds);

       
                                                                  
                     
       
    CommandCounterIncrement();
  }

     
                                                                        
                                 
     
  if (flags & REINDEX_REL_FORCE_INDEXES_UNLOGGED)
  {
    persistence = RELPERSISTENCE_UNLOGGED;
  }
  else if (flags & REINDEX_REL_FORCE_INDEXES_PERMANENT)
  {
    persistence = RELPERSISTENCE_PERMANENT;
  }
  else
  {
    persistence = rel->rd_rel->relpersistence;
  }

                                
  i = 1;
  foreach (indexId, indexIds)
  {
    Oid indexOid = lfirst_oid(indexId);
    Oid indexNamespaceId = get_rel_namespace(indexOid);

       
                                                                     
                                                                      
                                                              
       
    if (IsToastNamespace(indexNamespaceId) && !get_index_isvalid(indexOid))
    {
      ereport(WARNING, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot reindex invalid index \"%s.%s\" on TOAST table, skipping", get_namespace_name(indexNamespaceId), get_rel_name(indexOid))));
      continue;
    }

    reindex_index(indexOid, !(flags & REINDEX_REL_CHECK_CONSTRAINTS), persistence, options);

    CommandCounterIncrement();

                                                       
    Assert(!ReindexIsProcessingIndex(indexOid));

                                 
    pgstat_progress_update_param(PROGRESS_CLUSTER_INDEX_REBUILD_COUNT, i);
    i++;
  }

     
                                               
     
  table_close(rel, NoLock);

  result = (indexIds != NIL);

     
                                                                          
                                              
     
  if ((flags & REINDEX_REL_PROCESS_TOAST) && OidIsValid(toast_relid))
  {
    result |= reindex_relation(toast_relid, flags, options);
  }

  return result;
}

                                                                    
                                    
   
                                                                          
                                                                           
                                                                        
                                                                       
                                                                         
                                                                    
   

static Oid currentlyReindexedHeap = InvalidOid;
static Oid currentlyReindexedIndex = InvalidOid;
static List *pendingReindexedIndexes = NIL;
static int reindexingNestLevel = 0;

   
                           
                                                                
   
bool
ReindexIsProcessingHeap(Oid heapOid)
{
  return heapOid == currentlyReindexedHeap;
}

   
                                     
                                                                 
   
static bool
ReindexIsCurrentlyProcessingIndex(Oid indexOid)
{
  return indexOid == currentlyReindexedIndex;
}

   
                            
                                                                 
                                                                    
   
bool
ReindexIsProcessingIndex(Oid indexOid)
{
  return indexOid == currentlyReindexedIndex || list_member_oid(pendingReindexedIndexes, indexOid);
}

   
                        
                                                            
   
static void
SetReindexProcessing(Oid heapOid, Oid indexOid)
{
  Assert(OidIsValid(heapOid) && OidIsValid(indexOid));
                                     
  if (OidIsValid(currentlyReindexedHeap))
  {
    elog(ERROR, "cannot reindex while reindexing");
  }
  currentlyReindexedHeap = heapOid;
  currentlyReindexedIndex = indexOid;
                                             
  RemoveReindexPending(indexOid);
                                                                        
  reindexingNestLevel = GetCurrentTransactionNestLevel();
}

   
                          
                             
   
static void
ResetReindexProcessing(void)
{
  currentlyReindexedHeap = InvalidOid;
  currentlyReindexedIndex = InvalidOid;
                                                                    
}

   
                     
                                               
   
                                                                         
   
static void
SetReindexPending(List *indexes)
{
                                     
  if (pendingReindexedIndexes)
  {
    elog(ERROR, "cannot reindex while reindexing");
  }
  if (IsInParallelMode())
  {
    elog(ERROR, "cannot modify reindex state during a parallel operation");
  }
  pendingReindexedIndexes = list_copy(indexes);
  reindexingNestLevel = GetCurrentTransactionNestLevel();
}

   
                        
                                                  
   
static void
RemoveReindexPending(Oid indexOid)
{
  if (IsInParallelMode())
  {
    elog(ERROR, "cannot modify reindex state during a parallel operation");
  }
  pendingReindexedIndexes = list_delete_oid(pendingReindexedIndexes, indexOid);
}

   
                     
                                                              
   
void
ResetReindexState(int nestLevel)
{
     
                                                                             
                                                                          
                                                                             
                                                                             
     
  if (reindexingNestLevel >= nestLevel)
  {
    currentlyReindexedHeap = InvalidOid;
    currentlyReindexedIndex = InvalidOid;

       
                                                                          
                                                                         
                              
       
    pendingReindexedIndexes = NIL;

    reindexingNestLevel = 0;
  }
}

   
                             
                                                                     
   
Size
EstimateReindexStateSpace(void)
{
  return offsetof(SerializedReindexState, pendingReindexedIndexes) + mul_size(sizeof(Oid), list_length(pendingReindexedIndexes));
}

   
                         
                                                  
   
void
SerializeReindexState(Size maxsize, char *start_address)
{
  SerializedReindexState *sistate = (SerializedReindexState *)start_address;
  int c = 0;
  ListCell *lc;

  sistate->currentlyReindexedHeap = currentlyReindexedHeap;
  sistate->currentlyReindexedIndex = currentlyReindexedIndex;
  sistate->numPendingReindexedIndexes = list_length(pendingReindexedIndexes);
  foreach (lc, pendingReindexedIndexes)
  {
    sistate->pendingReindexedIndexes[c++] = lfirst_oid(lc);
  }
}

   
                       
                                                
   
void
RestoreReindexState(void *reindexstate)
{
  SerializedReindexState *sistate = (SerializedReindexState *)reindexstate;
  int c = 0;
  MemoryContext oldcontext;

  currentlyReindexedHeap = sistate->currentlyReindexedHeap;
  currentlyReindexedIndex = sistate->currentlyReindexedIndex;

  Assert(pendingReindexedIndexes == NIL);
  oldcontext = MemoryContextSwitchTo(TopMemoryContext);
  for (c = 0; c < sistate->numPendingReindexedIndexes; ++c)
  {
    pendingReindexedIndexes = lappend_oid(pendingReindexedIndexes, sistate->pendingReindexedIndexes[c]);
  }
  MemoryContextSwitchTo(oldcontext);

                                                             
  reindexingNestLevel = GetCurrentTransactionNestLevel();
}
