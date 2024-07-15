                                                                            
   
               
                                            
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   

#include "postgres.h"

#include "access/amapi.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/reloptions.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/pg_am.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_tablespace.h"
#include "catalog/pg_type.h"
#include "commands/comment.h"
#include "commands/dbcommands.h"
#include "commands/defrem.h"
#include "commands/event_trigger.h"
#include "commands/progress.h"
#include "commands/tablecmds.h"
#include "commands/tablespace.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/parse_coerce.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "partitioning/partdesc.h"
#include "pgstat.h"
#include "rewrite/rewriteManip.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "storage/sinvaladt.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/partcache.h"
#include "utils/pg_rusage.h"
#include "utils/regproc.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

                                    
static void
CheckPredicate(Expr *predicate);
static void
ComputeIndexAttrs(IndexInfo *indexInfo, Oid *typeOidP, Oid *collationOidP, Oid *classOidP, int16 *colOptionP, List *attList, List *exclusionOpNames, Oid relId, const char *accessMethodName, Oid accessMethodId, bool amcanorder, bool isconstraint, Oid ddl_userid, int ddl_sec_context, int *ddl_save_nestlevel);
static char *
ChooseIndexName(const char *tabname, Oid namespaceId, List *colnames, List *exclusionOpNames, bool primary, bool isconstraint);
static char *
ChooseIndexNameAddition(List *colnames);
static List *
ChooseIndexColumnNames(List *indexElems);
static void
RangeVarCallbackForReindexIndex(const RangeVar *relation, Oid relId, Oid oldRelId, void *arg);
static bool
ReindexRelationConcurrently(Oid relationOid, int options);
static void
ReindexPartitionedIndex(Relation parentIdx);
static void
update_relispartition(Oid relationId, bool newval);

   
                                                                
   
struct ReindexIndexCallbackState
{
  bool concurrent;                               
  Oid locked_table_oid;                                     
};

   
                        
                                                                        
                                                                       
                                                                   
   
                                                          
                                              
                                                                           
                 
                                                                        
                                           
   
                                                                            
                                                                             
                                                                              
                                                                             
                                                                              
                                                       
   
                                                                            
                                                                           
                                                                             
                                                                           
                                                                      
                                                                        
            
   
                                                                             
                                                                             
                                                                            
                                                                            
                                                                        
   
                                                                        
                                                                    
   
bool
CheckIndexCompatible(Oid oldId, const char *accessMethodName, List *attributeList, List *exclusionOpNames)
{
  bool isconstraint;
  Oid *typeObjectId;
  Oid *collationObjectId;
  Oid *classObjectId;
  Oid accessMethodId;
  Oid relationId;
  HeapTuple tuple;
  Form_pg_index indexForm;
  Form_pg_am accessMethodForm;
  IndexAmRoutine *amRoutine;
  bool amcanorder;
  int16 *coloptions;
  IndexInfo *indexInfo;
  int numberOfAttributes;
  int old_natts;
  bool isnull;
  bool ret = true;
  oidvector *old_indclass;
  oidvector *old_indcollation;
  Relation irel;
  int i;
  Datum d;

                                                                   
  relationId = IndexGetRelation(oldId, false);

     
                                                                             
                                                                          
     
  isconstraint = false;

  numberOfAttributes = list_length(attributeList);
  Assert(numberOfAttributes > 0);
  Assert(numberOfAttributes <= INDEX_MAX_KEYS);

                                 
  tuple = SearchSysCache1(AMNAME, PointerGetDatum(accessMethodName));
  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("access method \"%s\" does not exist", accessMethodName)));
  }
  accessMethodForm = (Form_pg_am)GETSTRUCT(tuple);
  accessMethodId = accessMethodForm->oid;
  amRoutine = GetIndexAmRoutine(accessMethodForm->amhandler);
  ReleaseSysCache(tuple);

  amcanorder = amRoutine->amcanorder;

     
                                                                           
                                                                             
                                                                       
                                                                           
                                                             
                                          
     
  indexInfo = makeIndexInfo(numberOfAttributes, numberOfAttributes, accessMethodId, NIL, NIL, false, false, false);
  typeObjectId = (Oid *)palloc(numberOfAttributes * sizeof(Oid));
  collationObjectId = (Oid *)palloc(numberOfAttributes * sizeof(Oid));
  classObjectId = (Oid *)palloc(numberOfAttributes * sizeof(Oid));
  coloptions = (int16 *)palloc(numberOfAttributes * sizeof(int16));
  ComputeIndexAttrs(indexInfo, typeObjectId, collationObjectId, classObjectId, coloptions, attributeList, exclusionOpNames, relationId, accessMethodName, accessMethodId, amcanorder, isconstraint, InvalidOid, 0, NULL);

                                             
  tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(oldId));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for index %u", oldId);
  }
  indexForm = (Form_pg_index)GETSTRUCT(tuple);

     
                                                                        
                                                                             
     
  if (!(heap_attisnull(tuple, Anum_pg_index_indpred, NULL) && heap_attisnull(tuple, Anum_pg_index_indexprs, NULL) && indexForm->indisvalid))
  {
    ReleaseSysCache(tuple);
    return false;
  }

                                                                       
  old_natts = indexForm->indnkeyatts;
  Assert(old_natts == numberOfAttributes);

  d = SysCacheGetAttr(INDEXRELID, tuple, Anum_pg_index_indcollation, &isnull);
  Assert(!isnull);
  old_indcollation = (oidvector *)DatumGetPointer(d);

  d = SysCacheGetAttr(INDEXRELID, tuple, Anum_pg_index_indclass, &isnull);
  Assert(!isnull);
  old_indclass = (oidvector *)DatumGetPointer(d);

  ret = (memcmp(old_indclass->values, classObjectId, old_natts * sizeof(Oid)) == 0 && memcmp(old_indcollation->values, collationObjectId, old_natts * sizeof(Oid)) == 0);

  ReleaseSysCache(tuple);

  if (!ret)
  {
    return false;
  }

                                                                           
  irel = index_open(oldId, AccessShareLock);                                 
  for (i = 0; i < old_natts; i++)
  {
    if (IsPolymorphicType(get_opclass_input_type(classObjectId[i])) && TupleDescAttr(irel->rd_att, i)->atttypid != typeObjectId[i])
    {
      ret = false;
      break;
    }
  }

                                                                         
  if (ret && indexInfo->ii_ExclusionOps != NULL)
  {
    Oid *old_operators, *old_procs;
    uint16 *old_strats;

    RelationGetExclusionInfo(irel, &old_operators, &old_procs, &old_strats);
    ret = memcmp(old_operators, indexInfo->ii_ExclusionOps, old_natts * sizeof(Oid)) == 0;

                                                                      
    if (ret)
    {
      for (i = 0; i < old_natts && ret; i++)
      {
        Oid left, right;

        op_input_types(indexInfo->ii_ExclusionOps[i], &left, &right);
        if ((IsPolymorphicType(left) || IsPolymorphicType(right)) && TupleDescAttr(irel->rd_att, i)->atttypid != typeObjectId[i])
        {
          ret = false;
          break;
        }
      }
    }
  }

  index_close(irel, NoLock);
  return ret;
}

   
                         
   
                                                                               
                                                                         
                                                                              
                                                                   
   
                                                                            
                                                            
                                                                          
                                                                        
                                                                           
                                                                       
                          
   
                                                                         
                                                                      
                                                                      
                                                                          
           
   
                                                                          
                   
   
                                                                           
                                                                    
                                                                           
                                                                         
                                                                     
                                                                       
                                                                  
   
static void
WaitForOlderSnapshots(TransactionId limitXmin, bool progress)
{
  int n_old_snapshots;
  int i;
  VirtualTransactionId *old_snapshots;

  old_snapshots = GetCurrentVirtualXIDs(limitXmin, true, false, PROC_IS_AUTOVACUUM | PROC_IN_VACUUM, &n_old_snapshots);
  if (progress)
  {
    pgstat_progress_update_param(PROGRESS_WAITFOR_TOTAL, n_old_snapshots);
  }

  for (i = 0; i < n_old_snapshots; i++)
  {
    if (!VirtualTransactionIdIsValid(old_snapshots[i]))
    {
      continue;                                            
    }

    if (i > 0)
    {
                                         
      VirtualTransactionId *newer_snapshots;
      int n_newer_snapshots;
      int j;
      int k;

      newer_snapshots = GetCurrentVirtualXIDs(limitXmin, true, false, PROC_IS_AUTOVACUUM | PROC_IN_VACUUM, &n_newer_snapshots);
      for (j = i; j < n_old_snapshots; j++)
      {
        if (!VirtualTransactionIdIsValid(old_snapshots[j]))
        {
          continue;                                            
        }
        for (k = 0; k < n_newer_snapshots; k++)
        {
          if (VirtualTransactionIdEquals(old_snapshots[j], newer_snapshots[k]))
          {
            break;
          }
        }
        if (k >= n_newer_snapshots)                        
        {
          SetInvalidVirtualTransactionId(old_snapshots[j]);
        }
      }
      pfree(newer_snapshots);
    }

    if (VirtualTransactionIdIsValid(old_snapshots[i]))
    {
                                                              
      if (progress)
      {
        PGPROC *holder = BackendIdGetProc(old_snapshots[i].backendId);

        if (holder)
        {
          pgstat_progress_update_param(PROGRESS_WAITFOR_CURRENT_PID, holder->pid);
        }
      }
      VirtualXactLock(old_snapshots[i], true);
    }

    if (progress)
    {
      pgstat_progress_update_param(PROGRESS_WAITFOR_DONE, i + 1);
    }
  }
}

   
               
                         
   
                                                                               
                                                                               
                                                                               
                                                                           
                                                                               
                                                                            
                                                                            
                                                                            
                                                                             
                                                                       
                                                                          
                                                  
   
                                                                          
            
                                                                 
                                                                       
                                                        
                                                                             
                            
                                                                             
                                                         
                                                                             
                                                                               
                                                                       
                                                                              
                                                                          
                                                        
                                                                           
                                                                             
   
                                                    
   
ObjectAddress
DefineIndex(Oid relationId, IndexStmt *stmt, Oid indexRelationId, Oid parentIndexId, Oid parentConstraintId, bool is_alter_table, bool check_rights, bool check_not_in_use, bool skip_build, bool quiet)
{
  bool concurrent;
  char *indexRelationName;
  char *accessMethodName;
  Oid *typeObjectId;
  Oid *collationObjectId;
  Oid *classObjectId;
  Oid accessMethodId;
  Oid namespaceId;
  Oid tablespaceId;
  Oid createdConstraintId = InvalidOid;
  List *indexColNames;
  List *allIndexParams;
  Relation rel;
  HeapTuple tuple;
  Form_pg_am accessMethodForm;
  IndexAmRoutine *amRoutine;
  bool amcanorder;
  amoptions_function amoptions;
  bool partitioned;
  Datum reloptions;
  int16 *coloptions;
  IndexInfo *indexInfo;
  bits16 flags;
  bits16 constr_flags;
  int numberOfAttributes;
  int numberOfKeyAttributes;
  TransactionId limitXmin;
  ObjectAddress address;
  LockRelId heaprelid;
  LOCKTAG heaplocktag;
  LOCKMODE lockmode;
  Snapshot snapshot;
  Oid root_save_userid;
  int root_save_sec_context;
  int root_save_nestlevel;
  int i;

  root_save_nestlevel = NewGUCNestLevel();

     
                                                                             
                                                                          
                                                           
     
  if (stmt->reset_default_tblspc)
  {
    (void)set_config_option("default_tablespace", "", PGC_USERSET, PGC_S_SESSION, GUC_ACTION_SAVE, true, 0, false);
  }

     
                                                                             
                                                                          
                                                                            
                                                                            
           
     
  if (stmt->concurrent && get_rel_persistence(relationId) != RELPERSISTENCE_TEMP)
  {
    concurrent = true;
  }
  else
  {
    concurrent = false;
  }

     
                                                                             
           
     
  if (!OidIsValid(parentIndexId))
  {
    pgstat_progress_start_command(PROGRESS_COMMAND_CREATE_INDEX, relationId);
    pgstat_progress_update_param(PROGRESS_CREATEIDX_COMMAND, concurrent ? PROGRESS_CREATEIDX_COMMAND_CREATE_CONCURRENTLY : PROGRESS_CREATEIDX_COMMAND_CREATE);
  }

     
                                
     
  pgstat_progress_update_param(PROGRESS_CREATEIDX_INDEX_OID, InvalidOid);

     
                                   
     
  numberOfKeyAttributes = list_length(stmt->indexParams);

     
                                                                            
                                                                     
                                                                          
                                                                             
                                                                           
                          
     
  allIndexParams = list_concat(list_copy(stmt->indexParams), list_copy(stmt->indexIncludingParams));
  numberOfAttributes = list_length(allIndexParams);

  if (numberOfKeyAttributes <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("must specify at least one column")));
  }
  if (numberOfAttributes > INDEX_MAX_KEYS)
  {
    ereport(ERROR, (errcode(ERRCODE_TOO_MANY_COLUMNS), errmsg("cannot use more than %d columns in an index", INDEX_MAX_KEYS)));
  }

     
                                                                         
                                                                          
                       
     
                                                                             
                                                                            
                                                                       
                                                                            
                                        
     
                                                                     
                                                                      
                                             
     
  lockmode = concurrent ? ShareUpdateExclusiveLock : ShareLock;
  rel = table_open(relationId, lockmode);

     
                                                                             
                                                                       
                                                                          
     
  GetUserIdAndSecContext(&root_save_userid, &root_save_sec_context);
  SetUserIdAndSecContext(rel->rd_rel->relowner, root_save_sec_context | SECURITY_RESTRICTED_OPERATION);

  namespaceId = RelationGetNamespace(rel);

                                                                 
  switch (rel->rd_rel->relkind)
  {
  case RELKIND_RELATION:
  case RELKIND_MATVIEW:
  case RELKIND_PARTITIONED_TABLE:
            
    break;
  case RELKIND_FOREIGN_TABLE:

       
                                                                      
                                                    
       
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot create index on foreign table \"%s\"", RelationGetRelationName(rel))));
    break;
  default:
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table or materialized view", RelationGetRelationName(rel))));
    break;
  }

     
                                                                     
                 
     
                                                                        
                                                                          
                
     
  partitioned = rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE;
  if (partitioned)
  {
       
                                                                           
                                                                          
                                                                         
                                                 
       
    if (stmt->concurrent)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot create index on partitioned table \"%s\" concurrently", RelationGetRelationName(rel))));
    }
    if (stmt->excludeOpNames)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot create exclusion constraints on partitioned table \"%s\"", RelationGetRelationName(rel))));
    }
  }

     
                                                                 
     
  if (RELATION_IS_OTHER_TEMP(rel))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot create indexes on temporary tables of other sessions")));
  }

     
                                                                            
                                                                             
                                                                          
                                                                             
     
  if (check_not_in_use)
  {
    CheckTableNotInUse(rel, "CREATE INDEX");
  }

     
                                                                  
                                                                          
                                                               
                                                                        
     
  if (check_rights && !IsBootstrapProcessingMode())
  {
    AclResult aclresult;

    aclresult = pg_namespace_aclcheck(namespaceId, root_save_userid, ACL_CREATE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_SCHEMA, get_namespace_name(namespaceId));
    }
  }

     
                                                                         
                                                        
     
  if (stmt->tableSpace)
  {
    tablespaceId = get_tablespace_oid(stmt->tableSpace, false);
    if (partitioned && tablespaceId == MyDatabaseTableSpace)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot specify default tablespace for partitioned relations")));
    }
  }
  else
  {
    tablespaceId = GetDefaultTablespace(rel->rd_rel->relpersistence, partitioned);
                                            
  }

                                    
  if (check_rights && OidIsValid(tablespaceId) && tablespaceId != MyDatabaseTableSpace)
  {
    AclResult aclresult;

    aclresult = pg_tablespace_aclcheck(tablespaceId, root_save_userid, ACL_CREATE);
    if (aclresult != ACLCHECK_OK)
    {
      aclcheck_error(aclresult, OBJECT_TABLESPACE, get_tablespace_name(tablespaceId));
    }
  }

     
                                                                             
                                                                           
                                                                        
     
  if (rel->rd_rel->relisshared)
  {
    tablespaceId = GLOBALTABLESPACE_OID;
  }
  else if (tablespaceId == GLOBALTABLESPACE_OID)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("only shared relations can be placed in pg_global tablespace")));
  }

     
                                    
     
  indexColNames = ChooseIndexColumnNames(allIndexParams);

     
                                                    
     
  indexRelationName = stmt->idxname;
  if (indexRelationName == NULL)
  {
    indexRelationName = ChooseIndexName(RelationGetRelationName(rel), namespaceId, indexColNames, stmt->excludeOpNames, stmt->primary, stmt->isconstraint);
  }

     
                                                                            
     
  accessMethodName = stmt->accessMethod;
  tuple = SearchSysCache1(AMNAME, PointerGetDatum(accessMethodName));
  if (!HeapTupleIsValid(tuple))
  {
       
                                                                      
                                                                       
       
    if (strcmp(accessMethodName, "rtree") == 0)
    {
      ereport(NOTICE, (errmsg("substituting access method \"gist\" for obsolete method \"rtree\"")));
      accessMethodName = "gist";
      tuple = SearchSysCache1(AMNAME, PointerGetDatum(accessMethodName));
    }

    if (!HeapTupleIsValid(tuple))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("access method \"%s\" does not exist", accessMethodName)));
    }
  }
  accessMethodForm = (Form_pg_am)GETSTRUCT(tuple);
  accessMethodId = accessMethodForm->oid;
  amRoutine = GetIndexAmRoutine(accessMethodForm->amhandler);

  pgstat_progress_update_param(PROGRESS_CREATEIDX_ACCESS_METHOD_OID, accessMethodId);

  if (stmt->unique && !amRoutine->amcanunique)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("access method \"%s\" does not support unique indexes", accessMethodName)));
  }
  if (stmt->indexIncludingParams != NIL && !amRoutine->amcaninclude)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("access method \"%s\" does not support included columns", accessMethodName)));
  }
  if (numberOfKeyAttributes > 1 && !amRoutine->amcanmulticol)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("access method \"%s\" does not support multicolumn indexes", accessMethodName)));
  }
  if (stmt->excludeOpNames && amRoutine->amgettuple == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("access method \"%s\" does not support exclusion constraints", accessMethodName)));
  }

  amcanorder = amRoutine->amcanorder;
  amoptions = amRoutine->amoptions;

  pfree(amRoutine);
  ReleaseSysCache(tuple);

     
                                  
     
  if (stmt->whereClause)
  {
    CheckPredicate((Expr *)stmt->whereClause);
  }

     
                                                                      
     
  reloptions = transformRelOptions((Datum)0, stmt->options, NULL, NULL, false, false);

  (void)index_reloptions(amoptions, reloptions, true);

     
                                                                           
                                                                           
                                           
     
  indexInfo = makeIndexInfo(numberOfAttributes, numberOfKeyAttributes, accessMethodId, NIL,                               
      make_ands_implicit((Expr *)stmt->whereClause), stmt->unique, !concurrent, concurrent);

  typeObjectId = (Oid *)palloc(numberOfAttributes * sizeof(Oid));
  collationObjectId = (Oid *)palloc(numberOfAttributes * sizeof(Oid));
  classObjectId = (Oid *)palloc(numberOfAttributes * sizeof(Oid));
  coloptions = (int16 *)palloc(numberOfAttributes * sizeof(int16));
  ComputeIndexAttrs(indexInfo, typeObjectId, collationObjectId, classObjectId, coloptions, allIndexParams, stmt->excludeOpNames, relationId, accessMethodName, accessMethodId, amcanorder, stmt->isconstraint, root_save_userid, root_save_sec_context, &root_save_nestlevel);

     
                                                     
     
  if (stmt->primary)
  {
    index_check_primary_key(rel, indexInfo, is_alter_table, stmt);
  }

     
                                                                         
                                                                      
                                                                            
                                                                        
     
                                                                            
                                                                  
     
  if (partitioned && (stmt->unique || stmt->primary))
  {
    PartitionKey key = RelationGetPartitionKey(rel);
    const char *constraint_type;
    int i;

    if (stmt->primary)
    {
      constraint_type = "PRIMARY KEY";
    }
    else if (stmt->unique)
    {
      constraint_type = "UNIQUE";
    }
    else if (stmt->excludeOpNames != NIL)
    {
      constraint_type = "EXCLUDE";
    }
    else
    {
      elog(ERROR, "unknown constraint type");
      constraint_type = NULL;                          
    }

       
                                                                      
                                                                
       
    for (i = 0; i < key->partnatts; i++)
    {
      bool found = false;
      int eq_strategy;
      Oid ptkey_eqop;
      int j;

         
                                                                     
                                                                      
                                                                         
                                                         
         
      if (key->strategy == PARTITION_STRATEGY_HASH)
      {
        eq_strategy = HTEqualStrategyNumber;
      }
      else
      {
        eq_strategy = BTEqualStrategyNumber;
      }

      ptkey_eqop = get_opfamily_member(key->partopfamily[i], key->partopcintype[i], key->partopcintype[i], eq_strategy);
      if (!OidIsValid(ptkey_eqop))
      {
        elog(ERROR, "missing operator %d(%u,%u) in partition opfamily %u", eq_strategy, key->partopcintype[i], key->partopcintype[i], key->partopfamily[i]);
      }

         
                                                                  
                                                                      
                                                                       
                                                                 
         
      if (accessMethodId == BTREE_AM_OID)
      {
        eq_strategy = BTEqualStrategyNumber;
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot match partition key to an index using access method \"%s\"", accessMethodName)));
      }

         
                                                                         
                                                                     
         
      if (key->partattrs[i] == 0)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("unsupported %s constraint with partition key definition", constraint_type), errdetail("%s constraints cannot be used when partition keys include expressions.", constraint_type)));
      }

                                                  
      for (j = 0; j < indexInfo->ii_NumIndexKeyAttrs; j++)
      {
        if (key->partattrs[i] == indexInfo->ii_IndexAttrNumbers[j])
        {
                                                                   
          Oid idx_opfamily;
          Oid idx_opcintype;

          if (get_opclass_opfamily_and_input_type(classObjectId[j], &idx_opfamily, &idx_opcintype))
          {
            Oid idx_eqop;

            idx_eqop = get_opfamily_member(idx_opfamily, idx_opcintype, idx_opcintype, eq_strategy);
            if (ptkey_eqop == idx_eqop)
            {
              found = true;
              break;
            }
          }
        }
      }

      if (!found)
      {
        Form_pg_attribute att;

        att = TupleDescAttr(RelationGetDescr(rel), key->partattrs[i] - 1);
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("unique constraint on partitioned table must include all partitioning columns"), errdetail("%s constraint on table \"%s\" lacks column \"%s\" which is part of the partition key.", constraint_type, RelationGetRelationName(rel), NameStr(att->attname))));
      }
    }
  }

     
                                                                            
                                                           
     
  for (i = 0; i < indexInfo->ii_NumIndexAttrs; i++)
  {
    AttrNumber attno = indexInfo->ii_IndexAttrNumbers[i];

    if (attno < 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("index creation on system columns is not supported")));
    }
  }

     
                                                                      
     
  if (indexInfo->ii_Expressions || indexInfo->ii_Predicate)
  {
    Bitmapset *indexattrs = NULL;

    pull_varattnos((Node *)indexInfo->ii_Expressions, 1, &indexattrs);
    pull_varattnos((Node *)indexInfo->ii_Predicate, 1, &indexattrs);

    for (i = FirstLowInvalidHeapAttributeNumber + 1; i < 0; i++)
    {
      if (bms_is_member(i - FirstLowInvalidHeapAttributeNumber, indexattrs))
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("index creation on system columns is not supported")));
      }
    }
  }

     
                                                                             
                   
     
  if (stmt->isconstraint && !quiet)
  {
    const char *constraint_type;

    if (stmt->primary)
    {
      constraint_type = "PRIMARY KEY";
    }
    else if (stmt->unique)
    {
      constraint_type = "UNIQUE";
    }
    else if (stmt->excludeOpNames != NIL)
    {
      constraint_type = "EXCLUDE";
    }
    else
    {
      elog(ERROR, "unknown constraint type");
      constraint_type = NULL;                          
    }

    ereport(DEBUG1, (errmsg("%s %s will create implicit index \"%s\" for table \"%s\"", is_alter_table ? "ALTER TABLE / ADD" : "CREATE TABLE /", constraint_type, indexRelationName, RelationGetRelationName(rel))));
  }

     
                                                                            
                                                             
     
  Assert(!OidIsValid(stmt->oldNode) || (skip_build && !concurrent));

     
                                                                         
                                                                            
                                                                          
                                                           
     
  flags = constr_flags = 0;
  if (stmt->isconstraint)
  {
    flags |= INDEX_CREATE_ADD_CONSTRAINT;
  }
  if (skip_build || concurrent || partitioned)
  {
    flags |= INDEX_CREATE_SKIP_BUILD;
  }
  if (stmt->if_not_exists)
  {
    flags |= INDEX_CREATE_IF_NOT_EXISTS;
  }
  if (concurrent)
  {
    flags |= INDEX_CREATE_CONCURRENT;
  }
  if (partitioned)
  {
    flags |= INDEX_CREATE_PARTITIONED;
  }
  if (stmt->primary)
  {
    flags |= INDEX_CREATE_IS_PRIMARY;
  }

     
                                                                            
                                       
     
  if (partitioned && stmt->relation && !stmt->relation->inh)
  {
    PartitionDesc pd = RelationGetPartitionDesc(rel);

    if (pd->nparts != 0)
    {
      flags |= INDEX_CREATE_INVALID;
    }
  }

  if (stmt->deferrable)
  {
    constr_flags |= INDEX_CONSTR_CREATE_DEFERRABLE;
  }
  if (stmt->initdeferred)
  {
    constr_flags |= INDEX_CONSTR_CREATE_INIT_DEFERRED;
  }

  indexRelationId = index_create(rel, indexRelationName, indexRelationId, parentIndexId, parentConstraintId, stmt->oldNode, indexInfo, indexColNames, accessMethodId, tablespaceId, collationObjectId, classObjectId, coloptions, reloptions, flags, constr_flags, allowSystemTableMods, !check_rights, &createdConstraintId);

  ObjectAddressSet(address, RelationRelationId, indexRelationId);

  if (!OidIsValid(indexRelationId))
  {
       
                                                                           
                                                              
       
    AtEOXact_GUC(false, root_save_nestlevel);

                                             
    SetUserIdAndSecContext(root_save_userid, root_save_sec_context);

    table_close(rel, NoLock);

                                                    
    if (!OidIsValid(parentIndexId))
    {
      pgstat_progress_end_command();
    }

    return address;
  }

     
                                                                     
                                                                          
                                                                        
     
  AtEOXact_GUC(false, root_save_nestlevel);
  root_save_nestlevel = NewGUCNestLevel();

                                 
  if (stmt->idxcomment != NULL)
  {
    CreateComments(indexRelationId, RelationRelationId, 0, stmt->idxcomment);
  }

  if (partitioned)
  {
    PartitionDesc partdesc;

       
                                                                          
                                                                      
       
                                                                       
       
    partdesc = RelationGetPartitionDesc(rel);
    if ((!stmt->relation || stmt->relation->inh) && partdesc->nparts > 0)
    {
      int nparts = partdesc->nparts;
      Oid *part_oids = palloc(sizeof(Oid) * nparts);
      bool invalidate_parent = false;
      Relation parentIndex;
      TupleDesc parentDesc;

      pgstat_progress_update_param(PROGRESS_CREATEIDX_PARTITIONS_TOTAL, nparts);

                                                                  
      memcpy(part_oids, partdesc->oids, sizeof(Oid) * nparts);

         
                                                                       
                                                                        
                                                                      
                                                                    
                                                                       
                                                  
         
      parentIndex = index_open(indexRelationId, lockmode);
      indexInfo = BuildIndexInfo(parentIndex);

      parentDesc = RelationGetDescr(rel);

         
                                                                       
                                                                        
                                                             
         
                                                                 
                                                                        
         
      for (i = 0; i < nparts; i++)
      {
        Oid childRelid = part_oids[i];
        Relation childrel;
        Oid child_save_userid;
        int child_save_sec_context;
        int child_save_nestlevel;
        List *childidxs;
        ListCell *cell;
        AttrNumber *attmap;
        bool found = false;
        int maplen;

        childrel = table_open(childRelid, lockmode);

        GetUserIdAndSecContext(&child_save_userid, &child_save_sec_context);
        SetUserIdAndSecContext(childrel->rd_rel->relowner, child_save_sec_context | SECURITY_RESTRICTED_OPERATION);
        child_save_nestlevel = NewGUCNestLevel();

           
                                                                       
                                                                   
                             
           
        if (childrel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
        {
          if (stmt->unique || stmt->primary)
          {
            ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot create unique index on partitioned table \"%s\"", RelationGetRelationName(rel)), errdetail("Table \"%s\" contains partitions that are foreign tables.", RelationGetRelationName(rel))));
          }

          AtEOXact_GUC(false, child_save_nestlevel);
          SetUserIdAndSecContext(child_save_userid, child_save_sec_context);
          table_close(childrel, lockmode);
          continue;
        }

        childidxs = RelationGetIndexList(childrel);
        attmap = convert_tuples_by_name_map(RelationGetDescr(childrel), parentDesc, gettext_noop("could not convert row type"));
        maplen = parentDesc->natts;

        foreach (cell, childidxs)
        {
          Oid cldidxid = lfirst_oid(cell);
          Relation cldidx;
          IndexInfo *cldIdxInfo;

                                                              
          if (has_superclass(cldidxid))
          {
            continue;
          }

          cldidx = index_open(cldidxid, lockmode);
          cldIdxInfo = BuildIndexInfo(cldidx);
          if (CompareIndexInfo(cldIdxInfo, indexInfo, cldidx->rd_indcollation, parentIndex->rd_indcollation, cldidx->rd_opfamily, parentIndex->rd_opfamily, attmap, maplen))
          {
            Oid cldConstrOid = InvalidOid;

               
                              
               
                                                            
                                                                
                                                                  
                                                                
                             
               
            if (createdConstraintId != InvalidOid)
            {
              cldConstrOid = get_relation_idx_constraint_oid(childRelid, cldidxid);
              if (cldConstrOid == InvalidOid)
              {
                index_close(cldidx, lockmode);
                continue;
              }
            }

                                                        
            IndexSetParentIndex(cldidx, indexRelationId);
            if (createdConstraintId != InvalidOid)
            {
              ConstraintSetParentConstraint(cldConstrOid, createdConstraintId, childRelid);
            }

            if (!cldidx->rd_index->indisvalid)
            {
              invalidate_parent = true;
            }

            found = true;
                                       
            index_close(cldidx, NoLock);
            break;
          }

          index_close(cldidx, lockmode);
        }

        list_free(childidxs);
        AtEOXact_GUC(false, child_save_nestlevel);
        SetUserIdAndSecContext(child_save_userid, child_save_sec_context);
        table_close(childrel, NoLock);

           
                                                           
           
        if (!found)
        {
          IndexStmt *childStmt = copyObject(stmt);
          bool found_whole_row;
          ListCell *lc;

             
                                                                   
                                                                     
                                                                 
                                                                 
                                                          
             
          childStmt->idxname = NULL;
          childStmt->relation = NULL;
          childStmt->indexOid = InvalidOid;
          childStmt->oldNode = InvalidOid;

             
                                                                     
                                                                     
                                                       
             
          foreach (lc, childStmt->indexParams)
          {
            IndexElem *ielem = lfirst(lc);

               
                                                                
                                                   
               
            if (ielem->expr)
            {
              ielem->expr = map_variable_attnos((Node *)ielem->expr, 1, 0, attmap, maplen, InvalidOid, &found_whole_row);
              if (found_whole_row)
              {
                elog(ERROR, "cannot convert whole-row table reference");
              }
            }
          }
          childStmt->whereClause = map_variable_attnos(stmt->whereClause, 1, 0, attmap, maplen, InvalidOid, &found_whole_row);
          if (found_whole_row)
          {
            elog(ERROR, "cannot convert whole-row table reference");
          }

             
                                                                    
                                                       
             
          Assert(GetUserId() == child_save_userid);
          SetUserIdAndSecContext(root_save_userid, root_save_sec_context);
          DefineIndex(childRelid, childStmt, InvalidOid,                        
              indexRelationId,                                                  
              createdConstraintId, is_alter_table, check_rights, check_not_in_use, skip_build, quiet);
          SetUserIdAndSecContext(child_save_userid, child_save_sec_context);
        }

        pgstat_progress_update_param(PROGRESS_CREATEIDX_PARTITIONS_DONE, i + 1);
        pfree(attmap);
      }

      index_close(parentIndex, lockmode);

         
                                                                
                                                                        
                                                                       
         
      if (invalidate_parent)
      {
        Relation pg_index = table_open(IndexRelationId, RowExclusiveLock);
        HeapTuple tup, newtup;

        tup = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexRelationId));
        if (!HeapTupleIsValid(tup))
        {
          elog(ERROR, "cache lookup failed for index %u", indexRelationId);
        }
        newtup = heap_copytuple(tup);
        ((Form_pg_index)GETSTRUCT(newtup))->indisvalid = false;
        CatalogTupleUpdate(pg_index, &tup->t_self, newtup);
        ReleaseSysCache(tup);
        table_close(pg_index, RowExclusiveLock);
        heap_freetuple(newtup);
      }
    }

       
                                                                        
                  
       
    AtEOXact_GUC(false, root_save_nestlevel);
    SetUserIdAndSecContext(root_save_userid, root_save_sec_context);
    table_close(rel, NoLock);
    if (!OidIsValid(parentIndexId))
    {
      pgstat_progress_end_command();
    }
    return address;
  }

  AtEOXact_GUC(false, root_save_nestlevel);
  SetUserIdAndSecContext(root_save_userid, root_save_sec_context);

  if (!concurrent)
  {
                                                                   
    table_close(rel, NoLock);

                                                     
    if (!OidIsValid(parentIndexId))
    {
      pgstat_progress_end_command();
    }

    return address;
  }

                                                            
  heaprelid = rel->rd_lockInfo.lockRelId;
  SET_LOCKTAG_RELATION(heaplocktag, heaprelid.dbId, heaprelid.relId);
  table_close(rel, NoLock);

     
                                                                        
                                                                            
                                                                            
                                                                           
                                                           
     
                                                                      
                                                                             
                                                                            
                   
     
                                                                         
                                                                         
                                                                          
                                                        
     
                                                                       
                                                                          
                                                                      
     
  LockRelationIdForSession(&heaprelid, ShareUpdateExclusiveLock);

  PopActiveSnapshot();
  CommitTransactionCommand();
  StartTransactionCommand();

     
                                                         
     
  pgstat_progress_update_param(PROGRESS_CREATEIDX_INDEX_OID, indexRelationId);

     
                                                                          
                                        
     
                                                                             
                                                                      
                                                                             
                                                                            
                                                                
     
                                                                            
                                                                          
                                                                         
                                                                          
                         
     
  pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE, PROGRESS_CREATEIDX_PHASE_WAIT_1);
  WaitForLockers(heaplocktag, ShareLock, true);

     
                                                                        
                                                                          
                                                                            
                                                                             
                                                                      
                                                                           
                                                                        
                                      
     
                                                                           
                                                                           
                                                                            
                                                                           
                                                                     
                                                                         
     

                                                                     
  PushActiveSnapshot(GetTransactionSnapshot());

                                         
  index_concurrently_build(relationId, indexRelationId);

                                        
  PopActiveSnapshot();

     
                                                                    
     
  CommitTransactionCommand();
  StartTransactionCommand();

     
                                       
     
                                                                          
                                                
     
  pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE, PROGRESS_CREATEIDX_PHASE_WAIT_2);
  WaitForLockers(heaplocktag, ShareLock, true);

     
                                                                             
                                                                             
                                                                       
                                                                             
                                                                             
                                                                             
                                                                     
     
                                                                             
                                 
     
                                                                             
                      
     
  snapshot = RegisterSnapshot(GetTransactionSnapshot());
  PushActiveSnapshot(snapshot);

     
                                                                    
     
  validate_index(relationId, indexRelationId, snapshot);

     
                                                                            
                                                                          
                                                                          
                                                                        
                                            
     
  limitXmin = snapshot->xmin;

  PopActiveSnapshot();
  UnregisterSnapshot(snapshot);

     
                                                                          
                                                                       
                                                                          
                                                                       
                                                                           
                                                                            
     
  CommitTransactionCommand();
  StartTransactionCommand();

                                                             
  Assert(MyPgXact->xmin == InvalidTransactionId);

     
                                                                        
                                                                             
                                                                  
                                                   
     
  pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE, PROGRESS_CREATEIDX_PHASE_WAIT_3);
  WaitForOlderSnapshots(limitXmin, true);

     
                                                                
     
  index_set_state_flags(indexRelationId, INDEX_CREATE_SET_VALID);

     
                                                                            
                                                                      
                                                                             
                                                                          
                                                                             
                                                                       
     
  CacheInvalidateRelcacheByRelid(heaprelid.relId);

     
                                                                             
     
  UnlockRelationIdForSession(&heaprelid, ShareUpdateExclusiveLock);

  pgstat_progress_end_command();

  return address;
}

   
                   
                                             
   
static bool
CheckMutability(Expr *expr)
{
     
                                                                         
                                                                         
                                                                       
                                                                           
                                                                             
                                                                             
                                                                        
                                                                           
                                               
     
                                                                           
     
  expr = expression_planner(expr);

                                                     
  return contain_mutable_functions((Node *)expr);
}

   
                  
                                                            
   
                                                                       
                                                                   
                                                                       
                                                                        
                                                                    
                                                                       
   
static void
CheckPredicate(Expr *predicate)
{
     
                                                                          
                                                                    
     

     
                                                                         
                                                                 
     
  if (CheckMutability(predicate))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("functions in index predicate must be marked IMMUTABLE")));
  }
}

   
                                                                          
                                                                             
                                                                    
   
                                                                             
                                                                          
                                                        
   
static void
ComputeIndexAttrs(IndexInfo *indexInfo, Oid *typeOidP, Oid *collationOidP, Oid *classOidP, int16 *colOptionP, List *attList,                          
    List *exclusionOpNames, Oid relId, const char *accessMethodName, Oid accessMethodId, bool amcanorder, bool isconstraint, Oid ddl_userid, int ddl_sec_context, int *ddl_save_nestlevel)
{
  ListCell *nextExclOp;
  ListCell *lc;
  int attn;
  int nkeycols = indexInfo->ii_NumIndexKeyAttrs;
  Oid save_userid;
  int save_sec_context;

                                                             
  if (exclusionOpNames)
  {
    Assert(list_length(exclusionOpNames) == nkeycols);
    indexInfo->ii_ExclusionOps = (Oid *)palloc(sizeof(Oid) * nkeycols);
    indexInfo->ii_ExclusionProcs = (Oid *)palloc(sizeof(Oid) * nkeycols);
    indexInfo->ii_ExclusionStrats = (uint16 *)palloc(sizeof(uint16) * nkeycols);
    nextExclOp = list_head(exclusionOpNames);
  }
  else
  {
    nextExclOp = NULL;
  }

  if (OidIsValid(ddl_userid))
  {
    GetUserIdAndSecContext(&save_userid, &save_sec_context);
  }

     
                           
     
  attn = 0;
  foreach (lc, attList)
  {
    IndexElem *attribute = (IndexElem *)lfirst(lc);
    Oid atttype;
    Oid attcollation;

       
                                                       
       
    if (attribute->name != NULL)
    {
                                  
      HeapTuple atttuple;
      Form_pg_attribute attform;

      Assert(attribute->expr == NULL);
      atttuple = SearchSysCacheAttName(relId, attribute->name);
      if (!HeapTupleIsValid(atttuple))
      {
                                                                 
        if (isconstraint)
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" named in key does not exist", attribute->name)));
        }
        else
        {
          ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" does not exist", attribute->name)));
        }
      }
      attform = (Form_pg_attribute)GETSTRUCT(atttuple);
      indexInfo->ii_IndexAttrNumbers[attn] = attform->attnum;
      atttype = attform->atttypid;
      attcollation = attform->attcollation;
      ReleaseSysCache(atttuple);
    }
    else
    {
                            
      Node *expr = attribute->expr;

      Assert(expr != NULL);

      if (attn >= nkeycols)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("expressions are not supported in included columns")));
      }
      atttype = exprType(expr);
      attcollation = exprCollation(expr);

         
                                                                         
                                                  
         
      while (IsA(expr, CollateExpr))
      {
        expr = (Node *)((CollateExpr *)expr)->arg;
      }

      if (IsA(expr, Var) && ((Var *)expr)->varattno != InvalidAttrNumber)
      {
           
                                                                  
                                                  
           
        indexInfo->ii_IndexAttrNumbers[attn] = ((Var *)expr)->varattno;
      }
      else
      {
        indexInfo->ii_IndexAttrNumbers[attn] = 0;                       
        indexInfo->ii_Expressions = lappend(indexInfo->ii_Expressions, expr);

           
                                                                    
                                                                     
                                    
           

           
                                                                    
                                                                    
                                                                       
                        
           
        if (CheckMutability((Expr *)expr))
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("functions in index expression must be marked IMMUTABLE")));
        }
      }
    }

    typeOidP[attn] = atttype;

       
                                                                      
                
       
    if (attn >= nkeycols)
    {
      if (attribute->collation)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("including column does not support a collation")));
      }
      if (attribute->opclass)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("including column does not support an operator class")));
      }
      if (attribute->ordering != SORTBY_DEFAULT)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("including column does not support ASC/DESC options")));
      }
      if (attribute->nulls_ordering != SORTBY_NULLS_DEFAULT)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("including column does not support NULLS FIRST/LAST options")));
      }

      classOidP[attn] = InvalidOid;
      colOptionP[attn] = 0;
      collationOidP[attn] = InvalidOid;
      attn++;

      continue;
    }

       
                                                                        
                                                                         
                                                               
       
    if (attribute->collation)
    {
      if (OidIsValid(ddl_userid))
      {
        AtEOXact_GUC(false, *ddl_save_nestlevel);
        SetUserIdAndSecContext(ddl_userid, ddl_sec_context);
      }
      attcollation = get_collation_oid(attribute->collation, false);
      if (OidIsValid(ddl_userid))
      {
        SetUserIdAndSecContext(save_userid, save_sec_context);
        *ddl_save_nestlevel = NewGUCNestLevel();
      }
    }

       
                                                                       
                                                                         
                                                                          
                                                                   
       
    if (type_is_collatable(atttype))
    {
      if (!OidIsValid(attcollation))
      {
        ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("could not determine which collation to use for index expression"), errhint("Use the COLLATE clause to set the collation explicitly.")));
      }
    }
    else
    {
      if (OidIsValid(attcollation))
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("collations are not supported by type %s", format_type_be(atttype))));
      }
    }

    collationOidP[attn] = attcollation;

       
                                                                           
                                                                      
                                                                  
                                        
       
    if (OidIsValid(ddl_userid))
    {
      AtEOXact_GUC(false, *ddl_save_nestlevel);
      SetUserIdAndSecContext(ddl_userid, ddl_sec_context);
    }
    classOidP[attn] = ResolveOpClass(attribute->opclass, atttype, accessMethodName, accessMethodId);
    if (OidIsValid(ddl_userid))
    {
      SetUserIdAndSecContext(save_userid, save_sec_context);
      *ddl_save_nestlevel = NewGUCNestLevel();
    }

       
                                                
       
    if (nextExclOp)
    {
      List *opname = (List *)lfirst(nextExclOp);
      Oid opid;
      Oid opfamily;
      int strat;

         
                                                                  
                                                                    
                                                                         
                                                         
                                                                       
                                                                 
         
      if (OidIsValid(ddl_userid))
      {
        AtEOXact_GUC(false, *ddl_save_nestlevel);
        SetUserIdAndSecContext(ddl_userid, ddl_sec_context);
      }
      opid = compatible_oper_opid(opname, atttype, atttype, false);
      if (OidIsValid(ddl_userid))
      {
        SetUserIdAndSecContext(save_userid, save_sec_context);
        *ddl_save_nestlevel = NewGUCNestLevel();
      }

         
                                                                  
                                                                     
                                         
         
      if (get_commutator(opid) != opid)
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("operator %s is not commutative", format_operator(opid)), errdetail("Only commutative operators can be used in exclusion constraints.")));
      }

         
                                                              
         
      opfamily = get_opclass_family(classOidP[attn]);
      strat = get_op_opfamily_strategy(opid, opfamily);
      if (strat == 0)
      {
        HeapTuple opftuple;
        Form_pg_opfamily opfform;

           
                                                                      
                                                                     
                          
           
        opftuple = SearchSysCache1(OPFAMILYOID, ObjectIdGetDatum(opfamily));
        if (!HeapTupleIsValid(opftuple))
        {
          elog(ERROR, "cache lookup failed for opfamily %u", opfamily);
        }
        opfform = (Form_pg_opfamily)GETSTRUCT(opftuple);

        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("operator %s is not a member of operator family \"%s\"", format_operator(opid), NameStr(opfform->opfname)), errdetail("The exclusion operator must be related to the index operator class for the constraint.")));
      }

      indexInfo->ii_ExclusionOps[attn] = opid;
      indexInfo->ii_ExclusionProcs[attn] = get_opcode(opid);
      indexInfo->ii_ExclusionStrats[attn] = strat;
      nextExclOp = lnext(nextExclOp);
    }

       
                                                                          
                                                                          
                                 
       
    colOptionP[attn] = 0;
    if (amcanorder)
    {
                                   
      if (attribute->ordering == SORTBY_DESC)
      {
        colOptionP[attn] |= INDOPTION_DESC;
      }
                                                                 
      if (attribute->nulls_ordering == SORTBY_NULLS_DEFAULT)
      {
        if (attribute->ordering == SORTBY_DESC)
        {
          colOptionP[attn] |= INDOPTION_NULLS_FIRST;
        }
      }
      else if (attribute->nulls_ordering == SORTBY_NULLS_FIRST)
      {
        colOptionP[attn] |= INDOPTION_NULLS_FIRST;
      }
    }
    else
    {
                                              
      if (attribute->ordering != SORTBY_DEFAULT)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("access method \"%s\" does not support ASC/DESC options", accessMethodName)));
      }
      if (attribute->nulls_ordering != SORTBY_NULLS_DEFAULT)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("access method \"%s\" does not support NULLS FIRST/LAST options", accessMethodName)));
      }
    }

    attn++;
  }
}

   
                                                           
   
                                                                           
                              
   
Oid
ResolveOpClass(List *opclass, Oid attrType, const char *accessMethodName, Oid accessMethodId)
{
  char *schemaname;
  char *opcname;
  HeapTuple tuple;
  Form_pg_opclass opform;
  Oid opClassId, opInputType;

     
                                                                            
                                                                           
                                                    
     
                                                                            
                
     
                                                                            
                                                                           
                
     
                                                                          
                              
     
  if (list_length(opclass) == 1)
  {
    char *claname = strVal(linitial(opclass));

    if (strcmp(claname, "network_ops") == 0 || strcmp(claname, "timespan_ops") == 0 || strcmp(claname, "datetime_ops") == 0 || strcmp(claname, "lztext_ops") == 0 || strcmp(claname, "timestamp_ops") == 0 || strcmp(claname, "bigbox_ops") == 0)
    {
      opclass = NIL;
    }
  }

  if (opclass == NIL)
  {
                                                          
    opClassId = GetDefaultOpClass(attrType, accessMethodId);
    if (!OidIsValid(opClassId))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("data type %s has no default operator class for access method \"%s\"", format_type_be(attrType), accessMethodName), errhint("You must specify an operator class for the index or define a default operator class for the data type.")));
    }
    return opClassId;
  }

     
                                                          
     

                                 
  DeconstructQualifiedName(opclass, &schemaname, &opcname);

  if (schemaname)
  {
                                      
    Oid namespaceId;

    namespaceId = LookupExplicitNamespace(schemaname, false);
    tuple = SearchSysCache3(CLAAMNAMENSP, ObjectIdGetDatum(accessMethodId), PointerGetDatum(opcname), ObjectIdGetDatum(namespaceId));
  }
  else
  {
                                                             
    opClassId = OpclassnameGetOpcid(accessMethodId, opcname);
    if (!OidIsValid(opClassId))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("operator class \"%s\" does not exist for access method \"%s\"", opcname, accessMethodName)));
    }
    tuple = SearchSysCache1(CLAOID, ObjectIdGetDatum(opClassId));
  }

  if (!HeapTupleIsValid(tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("operator class \"%s\" does not exist for access method \"%s\"", NameListToString(opclass), accessMethodName)));
  }

     
                                                                          
                                       
     
  opform = (Form_pg_opclass)GETSTRUCT(tuple);
  opClassId = opform->oid;
  opInputType = opform->opcintype;

  if (!IsBinaryCoercible(attrType, opInputType))
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("operator class \"%s\" does not accept data type %s", NameListToString(opclass), format_type_be(attrType))));
  }

  ReleaseSysCache(tuple);

  return opClassId;
}

   
                     
   
                                                                       
                                                                 
   
Oid
GetDefaultOpClass(Oid type_id, Oid am_id)
{
  Oid result = InvalidOid;
  int nexact = 0;
  int ncompatible = 0;
  int ncompatiblepreferred = 0;
  Relation rel;
  ScanKeyData skey[1];
  SysScanDesc scan;
  HeapTuple tup;
  TYPCATEGORY tcategory;

                                                       
  type_id = getBaseType(type_id);

  tcategory = TypeCategory(type_id);

     
                                                                        
                                                                        
                                                                       
     
                                                                          
                                                                             
                                                                           
                                                                           
                                                                        
                                              
     
  rel = table_open(OperatorClassRelationId, AccessShareLock);

  ScanKeyInit(&skey[0], Anum_pg_opclass_opcmethod, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(am_id));

  scan = systable_beginscan(rel, OpclassAmNameNspIndexId, true, NULL, 1, skey);

  while (HeapTupleIsValid(tup = systable_getnext(scan)))
  {
    Form_pg_opclass opclass = (Form_pg_opclass)GETSTRUCT(tup);

                                                    
    if (!opclass->opcdefault)
    {
      continue;
    }
    if (opclass->opcintype == type_id)
    {
      nexact++;
      result = opclass->oid;
    }
    else if (nexact == 0 && IsBinaryCoercible(type_id, opclass->opcintype))
    {
      if (IsPreferredType(tcategory, opclass->opcintype))
      {
        ncompatiblepreferred++;
        result = opclass->oid;
      }
      else if (ncompatiblepreferred == 0)
      {
        ncompatible++;
        result = opclass->oid;
      }
    }
  }

  systable_endscan(scan);

  table_close(rel, AccessShareLock);

                                                            
  if (nexact > 1)
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("there are multiple default operator classes for data type %s", format_type_be(type_id))));
  }

  if (nexact == 1 || ncompatiblepreferred == 1 || (ncompatiblepreferred == 0 && ncompatible == 1))
  {
    return result;
  }

  return InvalidOid;
}

   
                    
   
                                                                        
                             
   
                                                                             
                                                                          
                                            
   
                                    
   
                                                                         
                                                                       
                                                                         
                                                                          
                                                              
   
                                                                      
                                                                      
                                                              
   
char *
makeObjectName(const char *name1, const char *name2, const char *label)
{
  char *name;
  int overhead = 0;                                             
  int availchars;                                    
  int name1chars;                                 
  int name2chars;                                 
  int ndx;

  name1chars = strlen(name1);
  if (name2)
  {
    name2chars = strlen(name2);
    overhead++;                                      
  }
  else
  {
    name2chars = 0;
  }
  if (label)
  {
    overhead += strlen(label) + 1;
  }

  availchars = NAMEDATALEN - 1 - overhead;
  Assert(availchars > 0);                                    

     
                                                                         
                                                                             
             
     
  while (name1chars + name2chars > availchars)
  {
    if (name1chars > name2chars)
    {
      name1chars--;
    }
    else
    {
      name2chars--;
    }
  }

  name1chars = pg_mbcliplen(name1, name1chars, name1chars);
  if (name2)
  {
    name2chars = pg_mbcliplen(name2, name2chars, name2chars);
  }

                                                         
  name = palloc(name1chars + name2chars + overhead + 1);
  memcpy(name, name1, name1chars);
  ndx = name1chars;
  if (name2)
  {
    name[ndx++] = '_';
    memcpy(name + ndx, name2, name2chars);
    ndx += name2chars;
  }
  if (label)
  {
    name[ndx++] = '_';
    strcpy(name + ndx, label);
  }
  else
  {
    name[ndx] = '\0';
  }

  return name;
}

   
                                                                        
                                                                       
                                                              
   
                                                                          
                                                                             
                                                                             
   
                                                                       
                                                                           
                                                                             
                                                                        
                      
   
                                                                            
                                                                           
                                                                            
                                                                          
                                                                      
                                                         
   
                              
   
char *
ChooseRelationName(const char *name1, const char *name2, const char *label, Oid namespaceid, bool isconstraint)
{
  int pass = 0;
  char *relname = NULL;
  char modlabel[NAMEDATALEN];

                                      
  StrNCpy(modlabel, label, sizeof(modlabel));

  for (;;)
  {
    relname = makeObjectName(name1, name2, modlabel);

    if (!OidIsValid(get_relname_relid(relname, namespaceid)))
    {
      if (!isconstraint || !ConstraintNameExists(relname, namespaceid))
      {
        break;
      }
    }

                                                       
    pfree(relname);
    snprintf(modlabel, sizeof(modlabel), "%s%d", label, ++pass);
  }

  return relname;
}

   
                                            
   
                                          
   
static char *
ChooseIndexName(const char *tabname, Oid namespaceId, List *colnames, List *exclusionOpNames, bool primary, bool isconstraint)
{
  char *indexname;

  if (primary)
  {
                                                                          
    indexname = ChooseRelationName(tabname, NULL, "pkey", namespaceId, true);
  }
  else if (exclusionOpNames != NIL)
  {
    indexname = ChooseRelationName(tabname, ChooseIndexNameAddition(colnames), "excl", namespaceId, true);
  }
  else if (isconstraint)
  {
    indexname = ChooseRelationName(tabname, ChooseIndexNameAddition(colnames), "key", namespaceId, true);
  }
  else
  {
    indexname = ChooseRelationName(tabname, ChooseIndexNameAddition(colnames), "idx", namespaceId, false);
  }

  return indexname;
}

   
                                                                          
                                                                    
                                                                             
   
                                                                        
                                                                 
   
                                                           
                                        
   
static char *
ChooseIndexNameAddition(List *colnames)
{
  char buf[NAMEDATALEN * 2];
  int buflen = 0;
  ListCell *lc;

  buf[0] = '\0';
  foreach (lc, colnames)
  {
    const char *name = (const char *)lfirst(lc);

    if (buflen > 0)
    {
      buf[buflen++] = '_';                             
    }

       
                                                                         
                                                               
       
    strlcpy(buf + buflen, name, NAMEDATALEN);
    buflen += strlen(buf + buflen);
    if (buflen >= NAMEDATALEN)
    {
      break;
    }
  }
  return pstrdup(buf);
}

   
                                                                             
                                                                          
                                                                         
   
                                                               
   
static List *
ChooseIndexColumnNames(List *indexElems)
{
  List *result = NIL;
  ListCell *lc;

  foreach (lc, indexElems)
  {
    IndexElem *ielem = (IndexElem *)lfirst(lc);
    const char *origname;
    const char *curname;
    int i;
    char buf[NAMEDATALEN];

                                                     
    if (ielem->indexcolname)
    {
      origname = ielem->indexcolname;                            
    }
    else if (ielem->name)
    {
      origname = ielem->name;                              
    }
    else
    {
      origname = "expr";                                  
    }

                                                            
    curname = origname;
    for (i = 1;; i++)
    {
      ListCell *lc2;
      char nbuf[32];
      int nlen;

      foreach (lc2, result)
      {
        if (strcmp(curname, (char *)lfirst(lc2)) == 0)
        {
          break;
        }
      }
      if (lc2 == NULL)
      {
        break;                                
      }

      sprintf(nbuf, "%d", i);

                                                               
      nlen = pg_mbcliplen(origname, strlen(origname), NAMEDATALEN - 1 - strlen(nbuf));
      memcpy(buf, origname, nlen);
      strcpy(buf + nlen, nbuf);
      curname = buf;
    }

                                       
    result = lappend(result, pstrdup(curname));
  }
  return result;
}

   
                
                               
   
void
ReindexIndex(RangeVar *indexRelation, int options, bool concurrent)
{
  struct ReindexIndexCallbackState state;
  Oid indOid;
  Relation irel;
  char persistence;

     
                                                                          
                                                                           
                                                                      
     
                                                                          
                                                                             
                                                                        
                                   
     
  state.concurrent = concurrent;
  state.locked_table_oid = InvalidOid;
  indOid = RangeVarGetRelidExtended(indexRelation, concurrent ? ShareUpdateExclusiveLock : AccessExclusiveLock, 0, RangeVarCallbackForReindexIndex, &state);

     
                                                                            
                        
     
  irel = index_open(indOid, NoLock);

  if (irel->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
  {
    ReindexPartitionedIndex(irel);
    return;
  }

  persistence = irel->rd_rel->relpersistence;
  index_close(irel, NoLock);

  if (concurrent && persistence != RELPERSISTENCE_TEMP)
  {
    ReindexRelationConcurrently(indOid, options);
  }
  else
  {
    reindex_index(indOid, false, persistence, options | REINDEXOPT_REPORT_PROGRESS);
  }
}

   
                                                                        
                                                                               
              
   
static void
RangeVarCallbackForReindexIndex(const RangeVar *relation, Oid relId, Oid oldRelId, void *arg)
{
  char relkind;
  struct ReindexIndexCallbackState *state = arg;
  LOCKMODE table_lockmode;

     
                                                                    
                                                                            
                      
     
  table_lockmode = state->concurrent ? ShareUpdateExclusiveLock : ShareLock;

     
                                                                         
                                                                           
           
     
  if (relId != oldRelId && OidIsValid(oldRelId))
  {
    UnlockRelationOid(state->locked_table_oid, table_lockmode);
    state->locked_table_oid = InvalidOid;
  }

                                                                   
  if (!OidIsValid(relId))
  {
    return;
  }

     
                                                                             
                                                                           
                                                           
     
  relkind = get_rel_relkind(relId);
  if (!relkind)
  {
    return;
  }
  if (relkind != RELKIND_INDEX && relkind != RELKIND_PARTITIONED_INDEX)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not an index", relation->relname)));
  }

                         
  if (!pg_class_ownercheck(relId, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_INDEX, relation->relname);
  }

                                                 
  if (relId != oldRelId)
  {
    Oid table_oid = IndexGetRelation(relId, true);

       
                                                                   
                                                                     
       
    if (OidIsValid(table_oid))
    {
      LockRelationOid(table_oid, table_lockmode);
      state->locked_table_oid = table_oid;
    }
  }
}

   
                
                                                                     
   
Oid
ReindexTable(RangeVar *relation, int options, bool concurrent)
{
  Oid heapOid;
  bool result;

     
                                                               
     
                                                                          
                                                                           
                                                                             
                                   
     
  heapOid = RangeVarGetRelidExtended(relation, concurrent ? ShareUpdateExclusiveLock : ShareLock, 0, RangeVarCallbackOwnsTable, NULL);

  if (concurrent && get_rel_persistence(heapOid) != RELPERSISTENCE_TEMP)
  {
    result = ReindexRelationConcurrently(heapOid, options);

    if (!result)
    {
      ereport(NOTICE, (errmsg("table \"%s\" has no indexes that can be reindexed concurrently", relation->relname)));
    }
  }
  else
  {
    result = reindex_relation(heapOid, REINDEX_REL_PROCESS_TOAST | REINDEX_REL_CHECK_CONSTRAINTS, options | REINDEXOPT_REPORT_PROGRESS);
    if (!result)
    {
      ereport(NOTICE, (errmsg("table \"%s\" has no indexes to reindex", relation->relname)));
    }
  }

  return heapOid;
}

   
                         
                                                                  
   
                                                                        
                                                                      
                                                                       
   
void
ReindexMultipleTables(const char *objectName, ReindexObjectType objectKind, int options, bool concurrent)
{
  Oid objectOid;
  Relation relationRelation;
  TableScanDesc scan;
  ScanKeyData scan_keys[1];
  HeapTuple tuple;
  MemoryContext private_context;
  MemoryContext old;
  List *relids = NIL;
  ListCell *l;
  int num_keys;
  bool concurrent_warning = false;

  AssertArg(objectName);
  Assert(objectKind == REINDEX_OBJECT_SCHEMA || objectKind == REINDEX_OBJECT_SYSTEM || objectKind == REINDEX_OBJECT_DATABASE);

  if (objectKind == REINDEX_OBJECT_SYSTEM && concurrent)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot reindex system catalogs concurrently")));
  }

     
                                                                           
                                                                             
                                                                          
                                              
     
  if (objectKind == REINDEX_OBJECT_SCHEMA)
  {
    objectOid = get_namespace_oid(objectName, false);

    if (!pg_namespace_ownercheck(objectOid, GetUserId()))
    {
      aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_SCHEMA, objectName);
    }
  }
  else
  {
    objectOid = MyDatabaseId;

    if (strcmp(objectName, get_database_name(objectOid)) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("can only reindex the currently open database")));
    }
    if (!pg_database_ownercheck(objectOid, GetUserId()))
    {
      aclcheck_error(ACLCHECK_NOT_OWNER, OBJECT_DATABASE, objectName);
    }
  }

     
                                                                             
                                                                      
                                                                        
                          
     
  private_context = AllocSetContextCreate(PortalContext, "ReindexMultipleTables", ALLOCSET_SMALL_SIZES);

     
                                                                             
                                                                             
                                
     
  if (objectKind == REINDEX_OBJECT_SCHEMA)
  {
    num_keys = 1;
    ScanKeyInit(&scan_keys[0], Anum_pg_class_relnamespace, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(objectOid));
  }
  else
  {
    num_keys = 0;
  }

     
                                                                        
     
                                                                         
                                                             
     
  relationRelation = table_open(RelationRelationId, AccessShareLock);
  scan = table_beginscan_catalog(relationRelation, num_keys, scan_keys);
  while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    Form_pg_class classtuple = (Form_pg_class)GETSTRUCT(tuple);
    Oid relid = classtuple->oid;

       
                                                                        
                               
       
                                                                         
                                                                         
                                                                         
                                                                   
                                
       
    if (classtuple->relkind != RELKIND_RELATION && classtuple->relkind != RELKIND_MATVIEW)
    {
      continue;
    }

                                                                          
    if (classtuple->relpersistence == RELPERSISTENCE_TEMP && !isTempNamespace(classtuple->relnamespace))
    {
      continue;
    }

                                                               
    if (objectKind == REINDEX_OBJECT_SYSTEM && !IsSystemClass(relid, classtuple))
    {
      continue;
    }

       
                                                                      
                                                                         
                                                                         
                                                                        
                                                                         
                                                                   
       
    if (classtuple->relisshared && !pg_class_ownercheck(relid, GetUserId()))
    {
      continue;
    }

       
                                                                           
                                                            
       
    if (concurrent && IsCatalogRelationOid(relid))
    {
      if (!concurrent_warning)
      {
        ereport(WARNING, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot reindex system catalogs concurrently, skipping all")));
      }
      concurrent_warning = true;
      continue;
    }

                                                           
    old = MemoryContextSwitchTo(private_context);

       
                                                                       
                                                                   
                                                                         
                                                                       
                        
       
    if (relid == RelationRelationId)
    {
      relids = lcons_oid(relid, relids);
    }
    else
    {
      relids = lappend_oid(relids, relid);
    }

    MemoryContextSwitchTo(old);
  }
  table_endscan(scan);
  table_close(relationRelation, AccessShareLock);

                                                      
  PopActiveSnapshot();
  CommitTransactionCommand();
  foreach (l, relids)
  {
    Oid relid = lfirst_oid(l);

    StartTransactionCommand();
                                                      
    PushActiveSnapshot(GetTransactionSnapshot());

    if (concurrent && get_rel_persistence(relid) != RELPERSISTENCE_TEMP)
    {
      (void)ReindexRelationConcurrently(relid, options);
                                                                 
    }
    else
    {
      bool result;

      result = reindex_relation(relid, REINDEX_REL_PROCESS_TOAST | REINDEX_REL_CHECK_CONSTRAINTS, options | REINDEXOPT_REPORT_PROGRESS);

      if (result && (options & REINDEXOPT_VERBOSE))
      {
        ereport(INFO, (errmsg("table \"%s.%s\" was reindexed", get_namespace_name(get_rel_namespace(relid)), get_rel_name(relid))));
      }

      PopActiveSnapshot();
    }

    CommitTransactionCommand();
  }
  StartTransactionCommand();

  MemoryContextDelete(private_context);
}

   
                                                                        
                
   
                                                                          
                                                                              
                                                                            
                                                                             
                                                                            
                                                                   
   
                                                                            
                                                                            
                                                                       
   
                                                                          
                                                     
   
                                                                               
                                                                            
                                                                        
                                                                           
                                                           
   
static bool
ReindexRelationConcurrently(Oid relationOid, int options)
{
  List *heapRelationIds = NIL;
  List *indexIds = NIL;
  List *newIndexIds = NIL;
  List *relationLocks = NIL;
  List *lockTags = NIL;
  ListCell *lc, *lc2;
  MemoryContext private_context;
  MemoryContext oldcontext;
  char relkind;
  char *relationName = NULL;
  char *relationNamespace = NULL;
  PGRUsage ru0;
  const int progress_index[] = {PROGRESS_CREATEIDX_COMMAND, PROGRESS_CREATEIDX_PHASE, PROGRESS_CREATEIDX_INDEX_OID, PROGRESS_CREATEIDX_ACCESS_METHOD_OID};
  int64 progress_vals[4];

     
                                                                             
                                                                      
                                                                        
                          
     
  private_context = AllocSetContextCreate(PortalContext, "ReindexConcurrent", ALLOCSET_SMALL_SIZES);

  if (options & REINDEXOPT_VERBOSE)
  {
                                                                
    oldcontext = MemoryContextSwitchTo(private_context);

    relationName = get_rel_name(relationOid);
    relationNamespace = get_namespace_name(get_rel_namespace(relationOid));

    pg_rusage_init(&ru0);

    MemoryContextSwitchTo(oldcontext);
  }

  relkind = get_rel_relkind(relationOid);

     
                                                                           
                                   
     
  switch (relkind)
  {
  case RELKIND_RELATION:
  case RELKIND_MATVIEW:
  case RELKIND_TOASTVALUE:
  {
       
                                                                 
                      
       
    Relation heapRelation;

                                                           
    oldcontext = MemoryContextSwitchTo(private_context);

                                               
    heapRelationIds = lappend_oid(heapRelationIds, relationOid);

    MemoryContextSwitchTo(oldcontext);

    if (IsCatalogRelationOid(relationOid))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot reindex system catalogs concurrently")));
    }

                                          
    heapRelation = table_open(relationOid, ShareUpdateExclusiveLock);

                                                       
    foreach (lc, RelationGetIndexList(heapRelation))
    {
      Oid cellOid = lfirst_oid(lc);
      Relation indexRelation = index_open(cellOid, ShareUpdateExclusiveLock);

      if (!indexRelation->rd_index->indisvalid)
      {
        ereport(WARNING, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot reindex invalid index \"%s.%s\" concurrently, skipping", get_namespace_name(get_rel_namespace(cellOid)), get_rel_name(cellOid))));
      }
      else if (indexRelation->rd_index->indisexclusion)
      {
        ereport(WARNING, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot reindex exclusion constraint index \"%s.%s\" concurrently, skipping", get_namespace_name(get_rel_namespace(cellOid)), get_rel_name(cellOid))));
      }
      else
      {
                                                               
        oldcontext = MemoryContextSwitchTo(private_context);

        indexIds = lappend_oid(indexIds, cellOid);

        MemoryContextSwitchTo(oldcontext);
      }

      index_close(indexRelation, NoLock);
    }

                                    
    if (OidIsValid(heapRelation->rd_rel->reltoastrelid))
    {
      Oid toastOid = heapRelation->rd_rel->reltoastrelid;
      Relation toastRelation = table_open(toastOid, ShareUpdateExclusiveLock);

                                                             
      oldcontext = MemoryContextSwitchTo(private_context);

                                                 
      heapRelationIds = lappend_oid(heapRelationIds, toastOid);

      MemoryContextSwitchTo(oldcontext);

      foreach (lc2, RelationGetIndexList(toastRelation))
      {
        Oid cellOid = lfirst_oid(lc2);
        Relation indexRelation = index_open(cellOid, ShareUpdateExclusiveLock);

        if (!indexRelation->rd_index->indisvalid)
        {
          ereport(WARNING, (errcode(ERRCODE_INDEX_CORRUPTED), errmsg("cannot reindex invalid index \"%s.%s\" concurrently, skipping", get_namespace_name(get_rel_namespace(cellOid)), get_rel_name(cellOid))));
        }
        else
        {
             
                                                       
                     
             
          oldcontext = MemoryContextSwitchTo(private_context);

          indexIds = lappend_oid(indexIds, cellOid);

          MemoryContextSwitchTo(oldcontext);
        }

        index_close(indexRelation, NoLock);
      }

      table_close(toastRelation, NoLock);
    }

    table_close(heapRelation, NoLock);
    break;
  }
  case RELKIND_INDEX:
  {
    Oid heapId = IndexGetRelation(relationOid, false);

    if (IsCatalogRelationOid(heapId))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot reindex system catalogs concurrently")));
    }

       
                                                                   
                                                       
       
    if (IsToastNamespace(get_rel_namespace(relationOid)) && !get_index_isvalid(relationOid))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot reindex invalid index on TOAST table concurrently")));
    }

                                                           
    oldcontext = MemoryContextSwitchTo(private_context);

                                                                 
    heapRelationIds = list_make1_oid(heapId);

       
                                                                
                                              
       
    indexIds = lappend_oid(indexIds, relationOid);

    MemoryContextSwitchTo(oldcontext);
    break;
  }
  case RELKIND_PARTITIONED_TABLE:
                                
    ereport(WARNING, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("REINDEX of partitioned tables is not yet implemented, skipping \"%s\"", get_rel_name(relationOid))));
    return false;
  default:
                                                           
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot reindex this type of relation concurrently")));
    break;
  }

                                       
  if (indexIds == NIL)
  {
    PopActiveSnapshot();
    return false;
  }

  Assert(heapRelationIds != NIL);

          
                                                                 
     
                         
     
                                          
                          
                                                                      
                         
                                 
                         
     
                                                                            
                     
     

     
                                     
     
                                                                           
                                                                            
                                                                            
                             
     

  foreach (lc, indexIds)
  {
    char *concurrentName;
    Oid indexId = lfirst_oid(lc);
    Oid newIndexId;
    Relation indexRel;
    Relation heapRel;
    Oid save_userid;
    int save_sec_context;
    int save_nestlevel;
    Relation newIndexRel;
    LockRelId *lockrelid;

    indexRel = index_open(indexId, ShareUpdateExclusiveLock);
    heapRel = table_open(indexRel->rd_index->indrelid, ShareUpdateExclusiveLock);

       
                                                                           
                                                                        
                                                                       
       
    GetUserIdAndSecContext(&save_userid, &save_sec_context);
    SetUserIdAndSecContext(heapRel->rd_rel->relowner, save_sec_context | SECURITY_RESTRICTED_OPERATION);
    save_nestlevel = NewGUCNestLevel();

                                                                    
    if (indexRel->rd_rel->relpersistence == RELPERSISTENCE_TEMP)
    {
      elog(ERROR, "cannot reindex a temporary table concurrently");
    }

    pgstat_progress_start_command(PROGRESS_COMMAND_CREATE_INDEX, RelationGetRelid(heapRel));
    progress_vals[0] = PROGRESS_CREATEIDX_COMMAND_REINDEX_CONCURRENTLY;
    progress_vals[1] = 0;                   
    progress_vals[2] = indexId;
    progress_vals[3] = indexRel->rd_rel->relam;
    pgstat_progress_update_multi_param(4, progress_index, progress_vals);

                                                            
    concurrentName = ChooseRelationName(get_rel_name(indexId), NULL, "ccnew", get_rel_namespace(indexRel->rd_index->indrelid), false);

                                                          
    newIndexId = index_concurrently_create_copy(heapRel, indexId, concurrentName);

       
                                                                       
                          
       
    newIndexRel = index_open(newIndexId, ShareUpdateExclusiveLock);

       
                                                          
       
    oldcontext = MemoryContextSwitchTo(private_context);

    newIndexIds = lappend_oid(newIndexIds, newIndexId);

       
                                                                    
                                                                        
                                                                           
                                        
       
    lockrelid = palloc(sizeof(*lockrelid));
    *lockrelid = indexRel->rd_lockInfo.lockRelId;
    relationLocks = lappend(relationLocks, lockrelid);
    lockrelid = palloc(sizeof(*lockrelid));
    *lockrelid = newIndexRel->rd_lockInfo.lockRelId;
    relationLocks = lappend(relationLocks, lockrelid);

    MemoryContextSwitchTo(oldcontext);

    index_close(indexRel, NoLock);
    index_close(newIndexRel, NoLock);

                                                               
    AtEOXact_GUC(false, save_nestlevel);

                                             
    SetUserIdAndSecContext(save_userid, save_sec_context);

    table_close(heapRel, NoLock);
  }

     
                                                                            
                                       
     
  foreach (lc, heapRelationIds)
  {
    Relation heapRelation = table_open(lfirst_oid(lc), ShareUpdateExclusiveLock);
    LockRelId *lockrelid;
    LOCKTAG *heaplocktag;

                                                   
    oldcontext = MemoryContextSwitchTo(private_context);

                                                                        
    lockrelid = palloc(sizeof(*lockrelid));
    *lockrelid = heapRelation->rd_lockInfo.lockRelId;
    relationLocks = lappend(relationLocks, lockrelid);

    heaplocktag = (LOCKTAG *)palloc(sizeof(LOCKTAG));

                                                                      
    SET_LOCKTAG_RELATION(*heaplocktag, lockrelid->dbId, lockrelid->relId);
    lockTags = lappend(lockTags, heaplocktag);

    MemoryContextSwitchTo(oldcontext);

                             
    table_close(heapRelation, NoLock);
  }

                                               
  foreach (lc, relationLocks)
  {
    LockRelId *lockrelid = (LockRelId *)lfirst(lc);

    LockRelationIdForSession(lockrelid, ShareUpdateExclusiveLock);
  }

  PopActiveSnapshot();
  CommitTransactionCommand();
  StartTransactionCommand();

     
                                     
     
                                                                             
                                                                        
                                                                            
                                                                    
                                     
     

  pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE, PROGRESS_CREATEIDX_PHASE_WAIT_1);
  WaitForLockersMultiple(lockTags, ShareLock, true);
  CommitTransactionCommand();

  foreach (lc, newIndexIds)
  {
    Relation newIndexRel;
    Oid newIndexId = lfirst_oid(lc);
    Oid heapId;
    Oid indexam;

                                                                 
    StartTransactionCommand();

       
                                                                           
                                                                 
                                                    
       
    CHECK_FOR_INTERRUPTS();

                                                                       
    PushActiveSnapshot(GetTransactionSnapshot());

       
                                                                          
                            
       
    newIndexRel = index_open(newIndexId, ShareUpdateExclusiveLock);
    heapId = newIndexRel->rd_index->indrelid;
    indexam = newIndexRel->rd_rel->relam;
    index_close(newIndexRel, NoLock);

       
                                                                       
                       
       
    pgstat_progress_start_command(PROGRESS_COMMAND_CREATE_INDEX, heapId);
    progress_vals[0] = PROGRESS_CREATEIDX_COMMAND_REINDEX_CONCURRENTLY;
    progress_vals[1] = PROGRESS_CREATEIDX_PHASE_BUILD;
    progress_vals[2] = newIndexId;
    progress_vals[3] = indexam;
    pgstat_progress_update_multi_param(4, progress_index, progress_vals);

                                               
    index_concurrently_build(heapId, newIndexId);

    PopActiveSnapshot();
    CommitTransactionCommand();
  }
  StartTransactionCommand();

     
                                     
     
                                                                         
                                                                             
                       
     

  pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE, PROGRESS_CREATEIDX_PHASE_WAIT_2);
  WaitForLockersMultiple(lockTags, ShareLock, true);
  CommitTransactionCommand();

  foreach (lc, newIndexIds)
  {
    Oid newIndexId = lfirst_oid(lc);
    Oid heapId;
    TransactionId limitXmin;
    Snapshot snapshot;
    Relation newIndexRel;
    Oid indexam;

    StartTransactionCommand();

       
                                                                           
                                                                 
                                                    
       
    CHECK_FOR_INTERRUPTS();

       
                                                                           
                                   
       
    snapshot = RegisterSnapshot(GetTransactionSnapshot());
    PushActiveSnapshot(snapshot);

       
                                                                          
                            
       
    newIndexRel = index_open(newIndexId, ShareUpdateExclusiveLock);
    heapId = newIndexRel->rd_index->indrelid;
    indexam = newIndexRel->rd_rel->relam;
    index_close(newIndexRel, NoLock);

       
                                                                       
                       
       
    pgstat_progress_start_command(PROGRESS_COMMAND_CREATE_INDEX, heapId);
    progress_vals[0] = PROGRESS_CREATEIDX_COMMAND_REINDEX_CONCURRENTLY;
    progress_vals[1] = PROGRESS_CREATEIDX_PHASE_VALIDATE_IDXSCAN;
    progress_vals[2] = newIndexId;
    progress_vals[3] = indexam;
    pgstat_progress_update_multi_param(4, progress_index, progress_vals);

    validate_index(heapId, newIndexId, snapshot);

       
                                                                          
                                                   
       
    limitXmin = snapshot->xmin;

    PopActiveSnapshot();
    UnregisterSnapshot(snapshot);

       
                                                                    
                                                                          
           
       
    CommitTransactionCommand();
    StartTransactionCommand();

       
                                                                          
                                                                          
                                                                         
                                                     
       
    pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE, PROGRESS_CREATEIDX_PHASE_WAIT_3);
    WaitForOlderSnapshots(limitXmin, true);

    CommitTransactionCommand();
  }

     
                                     
     
                                                                            
                                  
     
                                                                          
                                                                           
                                     
     

  StartTransactionCommand();

  forboth(lc, indexIds, lc2, newIndexIds)
  {
    char *oldName;
    Oid oldIndexId = lfirst_oid(lc);
    Oid newIndexId = lfirst_oid(lc2);
    Oid heapId;

       
                                                                           
                                                                 
                                                    
       
    CHECK_FOR_INTERRUPTS();

    heapId = IndexGetRelation(oldIndexId, false);

                                              
    oldName = ChooseRelationName(get_rel_name(oldIndexId), NULL, "ccold", get_rel_namespace(heapId), false);

       
                                                                        
                                           
       
    index_concurrently_swap(newIndexId, oldIndexId, oldName);

       
                                                                        
                                                                           
              
       
    CacheInvalidateRelcacheByRelid(heapId);

       
                                                                     
                                                                       
                                                                          
                                                                   
                   
       
    CommandCounterIncrement();
  }

                                                            
  CommitTransactionCommand();
  StartTransactionCommand();

     
                                     
     
                                                                        
                                                                 
                                    
     

  pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE, PROGRESS_CREATEIDX_PHASE_WAIT_4);
  WaitForLockersMultiple(lockTags, AccessExclusiveLock, true);

  foreach (lc, indexIds)
  {
    Oid oldIndexId = lfirst_oid(lc);
    Oid heapId;

       
                                                                           
                                                                 
                                                    
       
    CHECK_FOR_INTERRUPTS();

    heapId = IndexGetRelation(oldIndexId, false);
    index_concurrently_set_dead(heapId, oldIndexId);
  }

                                                            
  CommitTransactionCommand();
  StartTransactionCommand();

     
                                     
     
                           
     

  pgstat_progress_update_param(PROGRESS_CREATEIDX_PHASE, PROGRESS_CREATEIDX_PHASE_WAIT_5);
  WaitForLockersMultiple(lockTags, AccessExclusiveLock, true);

  PushActiveSnapshot(GetTransactionSnapshot());

  {
    ObjectAddresses *objects = new_object_addresses();

    foreach (lc, indexIds)
    {
      Oid oldIndexId = lfirst_oid(lc);
      ObjectAddress object;

      object.classId = RelationRelationId;
      object.objectId = oldIndexId;
      object.objectSubId = 0;

      add_exact_object_address(&object, objects);
    }

       
                                                                          
                         
       
    performMultipleDeletions(objects, DROP_RESTRICT, PERFORM_DELETION_CONCURRENT_LOCK | PERFORM_DELETION_INTERNAL);
  }

  PopActiveSnapshot();
  CommitTransactionCommand();

     
                                                           
     
  foreach (lc, relationLocks)
  {
    LockRelId *lockrelid = (LockRelId *)lfirst(lc);

    UnlockRelationIdForSession(lockrelid, ShareUpdateExclusiveLock);
  }

                                                          
  StartTransactionCommand();

                       
  if (options & REINDEXOPT_VERBOSE)
  {
    if (relkind == RELKIND_INDEX)
    {
      ereport(INFO, (errmsg("index \"%s.%s\" was reindexed", relationNamespace, relationName), errdetail("%s.", pg_rusage_show(&ru0))));
    }
    else
    {
      foreach (lc, newIndexIds)
      {
        Oid indOid = lfirst_oid(lc);

        ereport(INFO, (errmsg("index \"%s.%s\" was reindexed", get_namespace_name(get_rel_namespace(indOid)), get_rel_name(indOid))));
                                                               
      }

      ereport(INFO, (errmsg("table \"%s.%s\" was reindexed", relationNamespace, relationName), errdetail("%s.", pg_rusage_show(&ru0))));
    }
  }

  MemoryContextDelete(private_context);

  pgstat_progress_end_command();

  return true;
}

   
                           
                                                       
   
                        
   
static void
ReindexPartitionedIndex(Relation parentIdx)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("REINDEX is not yet implemented for partitioned indexes")));
}

   
                                                                             
                                                 
   
                                                                        
   
void
IndexSetParentIndex(Relation partitionIdx, Oid parentOid)
{
  Relation pg_inherits;
  ScanKeyData key[2];
  SysScanDesc scan;
  Oid partRelid = RelationGetRelid(partitionIdx);
  HeapTuple tuple;
  bool fix_dependencies;

                                  
  Assert(partitionIdx->rd_rel->relkind == RELKIND_INDEX || partitionIdx->rd_rel->relkind == RELKIND_PARTITIONED_INDEX);

     
                                                                 
     
  pg_inherits = relation_open(InheritsRelationId, RowExclusiveLock);
  ScanKeyInit(&key[0], Anum_pg_inherits_inhrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(partRelid));
  ScanKeyInit(&key[1], Anum_pg_inherits_inhseqno, BTEqualStrategyNumber, F_INT4EQ, Int32GetDatum(1));
  scan = systable_beginscan(pg_inherits, InheritsRelidSeqnoIndexId, true, NULL, 2, key);
  tuple = systable_getnext(scan);

  if (!HeapTupleIsValid(tuple))
  {
    if (parentOid == InvalidOid)
    {
         
                                                                         
               
         
      fix_dependencies = false;
    }
    else
    {
      StoreSingleInheritance(partRelid, parentOid, 1);
      fix_dependencies = true;
    }
  }
  else
  {
    Form_pg_inherits inhForm = (Form_pg_inherits)GETSTRUCT(tuple);

    if (parentOid == InvalidOid)
    {
         
                                                                        
         
      CatalogTupleDelete(pg_inherits, &tuple->t_self);
      fix_dependencies = true;
    }
    else
    {
         
                                                                         
                                                                    
                            
         
      if (inhForm->inhparent != parentOid)
      {
                                                               
        elog(ERROR, "bogus pg_inherit row: inhrelid %u inhparent %u", inhForm->inhrelid, inhForm->inhparent);
      }

                                      
      fix_dependencies = false;
    }
  }

                             
  systable_endscan(scan);
  relation_close(pg_inherits, RowExclusiveLock);

                                                                             
  if (OidIsValid(parentOid))
  {
    SetRelationHasSubclass(parentOid, true);
  }

                                                     
  update_relispartition(partRelid, OidIsValid(parentOid));

  if (fix_dependencies)
  {
       
                                                                         
                                                                     
                                              
       
    if (OidIsValid(parentOid))
    {
      ObjectAddress partIdx;
      ObjectAddress parentIdx;
      ObjectAddress partitionTbl;

      ObjectAddressSet(partIdx, RelationRelationId, partRelid);
      ObjectAddressSet(parentIdx, RelationRelationId, parentOid);
      ObjectAddressSet(partitionTbl, RelationRelationId, partitionIdx->rd_index->indrelid);
      recordDependencyOn(&partIdx, &parentIdx, DEPENDENCY_PARTITION_PRI);
      recordDependencyOn(&partIdx, &partitionTbl, DEPENDENCY_PARTITION_SEC);
    }
    else
    {
      deleteDependencyRecordsForClass(RelationRelationId, partRelid, RelationRelationId, DEPENDENCY_PARTITION_PRI);
      deleteDependencyRecordsForClass(RelationRelationId, partRelid, RelationRelationId, DEPENDENCY_PARTITION_SEC);
    }

                                  
    CommandCounterIncrement();
  }
}

   
                                                                              
                                   
   
static void
update_relispartition(Oid relationId, bool newval)
{
  HeapTuple tup;
  Relation classRel;

  classRel = table_open(RelationRelationId, RowExclusiveLock);
  tup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relationId));
  if (!HeapTupleIsValid(tup))
  {
    elog(ERROR, "cache lookup failed for relation %u", relationId);
  }
  Assert(((Form_pg_class)GETSTRUCT(tup))->relispartition != newval);
  ((Form_pg_class)GETSTRUCT(tup))->relispartition = newval;
  CatalogTupleUpdate(classRel, &tup->t_self, tup);
  heap_freetuple(tup);
  table_close(classRel, RowExclusiveLock);
}
