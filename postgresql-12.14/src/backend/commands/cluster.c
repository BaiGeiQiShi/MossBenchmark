                                                                            
   
             
                                                                          
   
                                                                            
   
   
                                                                         
                                                                          
   
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "access/amapi.h"
#include "access/heapam.h"
#include "access/multixact.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/tuptoaster.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/pg_am.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/index.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/toasting.h"
#include "commands/cluster.h"
#include "commands/progress.h"
#include "commands/tablecmds.h"
#include "commands/vacuum.h"
#include "miscadmin.h"
#include "optimizer/optimizer.h"
#include "pgstat.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/predicate.h"
#include "utils/acl.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_rusage.h"
#include "utils/relmapper.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/tuplesort.h"

   
                                                                      
                                                                              
                                
   
typedef struct
{
  Oid tableOid;
  Oid indexOid;
} RelToCluster;

static void
rebuild_relation(Relation OldHeap, Oid indexOid, bool verbose);
static void
copy_table_data(Oid OIDNewHeap, Oid OIDOldHeap, Oid OIDOldIndex, bool verbose, bool *pSwapToastByContent, TransactionId *pFreezeXid, MultiXactId *pCutoffMulti);
static List *
get_tables_to_cluster(MemoryContext cluster_context);

                                                                              
                                                                            
                                                                         
                                                                      
                                                                  
   
                                                              
                                                                         
               
                                                                          
                                             
                                                                    
                                                                         
                                                                  
                                        
                         
   
                                                             
   
                                                                          
                                                                         
                                          
                                                                              
   
void
cluster(ClusterStmt *stmt, bool isTopLevel)
{
  if (stmt->relation != NULL)
  {
                                           
    Oid tableOid, indexOid = InvalidOid;
    Relation rel;

                                                        
    tableOid = RangeVarGetRelidExtended(stmt->relation, AccessExclusiveLock, 0, RangeVarCallbackOwnsTable, NULL);
    rel = table_open(tableOid, NoLock);

       
                                                                    
                                     
       
    if (RELATION_IS_OTHER_TEMP(rel))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot cluster temporary tables of other sessions")));
    }

       
                                              
       
    if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot cluster a partitioned table")));
    }

    if (stmt->indexname == NULL)
    {
      ListCell *index;

                                                                  
      foreach (index, RelationGetIndexList(rel))
      {
        HeapTuple idxtuple;
        Form_pg_index indexForm;

        indexOid = lfirst_oid(index);
        idxtuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexOid));
        if (!HeapTupleIsValid(idxtuple))
        {
          elog(ERROR, "cache lookup failed for index %u", indexOid);
        }
        indexForm = (Form_pg_index)GETSTRUCT(idxtuple);
        if (indexForm->indisclustered)
        {
          ReleaseSysCache(idxtuple);
          break;
        }
        ReleaseSysCache(idxtuple);
        indexOid = InvalidOid;
      }

      if (!OidIsValid(indexOid))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("there is no previously clustered index for table \"%s\"", stmt->relation->relname)));
      }
    }
    else
    {
         
                                                                  
                   
         
      indexOid = get_relname_relid(stmt->indexname, rel->rd_rel->relnamespace);
      if (!OidIsValid(indexOid))
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("index \"%s\" for table \"%s\" does not exist", stmt->indexname, stmt->relation->relname)));
      }
    }

                                               
    table_close(rel, NoLock);

                     
    cluster_rel(tableOid, indexOid, stmt->options);
  }
  else
  {
       
                                                                        
                                                     
       
    MemoryContext cluster_context;
    List *rvs;
    ListCell *rv;

       
                                                                           
                                           
       
    PreventInTransactionBlock(isTopLevel, "CLUSTER");

       
                                                                    
       
                                                                          
                 
       
    cluster_context = AllocSetContextCreate(PortalContext, "Cluster", ALLOCSET_DEFAULT_SIZES);

       
                                                                        
                        
       
    rvs = get_tables_to_cluster(cluster_context);

                                                   
    PopActiveSnapshot();
    CommitTransactionCommand();

                                                                  
    foreach (rv, rvs)
    {
      RelToCluster *rvtc = (RelToCluster *)lfirst(rv);

                                                      
      StartTransactionCommand();
                                                        
      PushActiveSnapshot(GetTransactionSnapshot());
                       
      cluster_rel(rvtc->tableOid, rvtc->indexOid, stmt->options | CLUOPT_RECHECK);
      PopActiveSnapshot();
      CommitTransactionCommand();
    }

                                                       
    StartTransactionCommand();

                                  
    MemoryContextDelete(cluster_context);
  }
}

   
               
   
                                                                  
                                                                    
                                                                    
                                                                   
                             
   
                                                                               
                                                                            
                                               
   
                                                                            
                                                                           
                                                                           
   
void
cluster_rel(Oid tableOid, Oid indexOid, int options)
{
  Relation OldHeap;
  Oid save_userid;
  int save_sec_context;
  int save_nestlevel;
  bool verbose = ((options & CLUOPT_VERBOSE) != 0);
  bool recheck = ((options & CLUOPT_RECHECK) != 0);

                                       
  CHECK_FOR_INTERRUPTS();

  pgstat_progress_start_command(PROGRESS_COMMAND_CLUSTER, tableOid);
  if (OidIsValid(indexOid))
  {
    pgstat_progress_update_param(PROGRESS_CLUSTER_COMMAND, PROGRESS_CLUSTER_COMMAND_CLUSTER);
  }
  else
  {
    pgstat_progress_update_param(PROGRESS_CLUSTER_COMMAND, PROGRESS_CLUSTER_COMMAND_VACUUM_FULL);
  }

     
                                                                           
                                                                        
                                                                            
                                 
     
  OldHeap = try_relation_open(tableOid, AccessExclusiveLock);

                                                             
  if (!OldHeap)
  {
    pgstat_progress_end_command();
    return;
  }

     
                                                                             
                                                                      
                                                                 
     
  GetUserIdAndSecContext(&save_userid, &save_sec_context);
  SetUserIdAndSecContext(OldHeap->rd_rel->relowner, save_sec_context | SECURITY_RESTRICTED_OPERATION);
  save_nestlevel = NewGUCNestLevel();

     
                                                                             
                                                     
     
                                                                          
                                                                            
                                                  
     
  if (recheck)
  {
    HeapTuple tuple;
    Form_pg_index indexForm;

                                                     
    if (!pg_class_ownercheck(tableOid, save_userid))
    {
      relation_close(OldHeap, AccessExclusiveLock);
      goto out;
    }

       
                                                                         
                                                                         
                                                                        
                                                                         
                                                                          
                                                             
       
    if (RELATION_IS_OTHER_TEMP(OldHeap))
    {
      relation_close(OldHeap, AccessExclusiveLock);
      goto out;
    }

    if (OidIsValid(indexOid))
    {
         
                                           
         
      if (!SearchSysCacheExists1(RELOID, ObjectIdGetDatum(indexOid)))
      {
        relation_close(OldHeap, AccessExclusiveLock);
        goto out;
      }

         
                                                                        
         
      tuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexOid));
      if (!HeapTupleIsValid(tuple))                            
      {
        relation_close(OldHeap, AccessExclusiveLock);
        goto out;
      }
      indexForm = (Form_pg_index)GETSTRUCT(tuple);
      if (!indexForm->indisclustered)
      {
        ReleaseSysCache(tuple);
        relation_close(OldHeap, AccessExclusiveLock);
        goto out;
      }
      ReleaseSysCache(tuple);
    }
  }

     
                                                                         
                                                                         
                                                                            
                                                        
     
  if (OidIsValid(indexOid) && OldHeap->rd_rel->relisshared)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot cluster a shared catalog")));
  }

     
                                                                        
                                   
     
  if (RELATION_IS_OTHER_TEMP(OldHeap))
  {
    if (OidIsValid(indexOid))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot cluster temporary tables of other sessions")));
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot vacuum temporary tables of other sessions")));
    }
  }

     
                                                                            
                                                            
     
  CheckTableNotInUse(OldHeap, OidIsValid(indexOid) ? "CLUSTER" : "VACUUM");

                                                    
  if (OidIsValid(indexOid))
  {
    check_index_is_clusterable(OldHeap, indexOid, recheck, AccessExclusiveLock);
  }

     
                                                                             
                                                                             
                                                                            
                                                                          
               
     
  if (OldHeap->rd_rel->relkind == RELKIND_MATVIEW && !RelationIsPopulated(OldHeap))
  {
    relation_close(OldHeap, AccessExclusiveLock);
    goto out;
  }

     
                                                                     
                                                                       
                                                                       
                
     
  TransferPredicateLocksToHeapRelation(OldHeap);

                                                
  rebuild_relation(OldHeap, indexOid, verbose);

                                                          

out:
                                                             
  AtEOXact_GUC(false, save_nestlevel);

                                           
  SetUserIdAndSecContext(save_userid, save_sec_context);

  pgstat_progress_end_command();
}

   
                                                                    
   
                                                           
                                                                    
                                                                 
                    
   
void
check_index_is_clusterable(Relation OldHeap, Oid indexOid, bool recheck, LOCKMODE lockmode)
{
  Relation OldIndex;

  OldIndex = index_open(indexOid, lockmode);

     
                                                                
     
  if (OldIndex->rd_index == NULL || OldIndex->rd_index->indrelid != RelationGetRelid(OldHeap))
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not an index for table \"%s\"", RelationGetRelationName(OldIndex), RelationGetRelationName(OldHeap))));
  }

                                      
  if (!OldIndex->rd_indam->amclusterable)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot cluster on index \"%s\" because access method does not support clustering", RelationGetRelationName(OldIndex))));
  }

     
                                                                           
                                                                           
                                                                          
                            
     
  if (!heap_attisnull(OldIndex->rd_indextuple, Anum_pg_index_indpred, NULL))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot cluster on partial index \"%s\"", RelationGetRelationName(OldIndex))));
  }

     
                                                                             
                                                                             
                                                                            
                                                                           
                                                                             
                              
     
  if (!OldIndex->rd_index->indisvalid)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot cluster on invalid index \"%s\"", RelationGetRelationName(OldIndex))));
  }

                                                       
  index_close(OldIndex, NoLock);
}

   
                                                                          
   
                                                                            
   
void
mark_index_clustered(Relation rel, Oid indexOid, bool is_internal)
{
  HeapTuple indexTuple;
  Form_pg_index indexForm;
  Relation pg_index;
  ListCell *index;

                                                
  if (rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot mark index clustered in partitioned table")));
  }

     
                                                                       
     
  if (OidIsValid(indexOid))
  {
    indexTuple = SearchSysCache1(INDEXRELID, ObjectIdGetDatum(indexOid));
    if (!HeapTupleIsValid(indexTuple))
    {
      elog(ERROR, "cache lookup failed for index %u", indexOid);
    }
    indexForm = (Form_pg_index)GETSTRUCT(indexTuple);

    if (indexForm->indisclustered)
    {
      ReleaseSysCache(indexTuple);
      return;
    }

    ReleaseSysCache(indexTuple);
  }

     
                                                                       
     
  pg_index = table_open(IndexRelationId, RowExclusiveLock);

  foreach (index, RelationGetIndexList(rel))
  {
    Oid thisIndexOid = lfirst_oid(index);

    indexTuple = SearchSysCacheCopy1(INDEXRELID, ObjectIdGetDatum(thisIndexOid));
    if (!HeapTupleIsValid(indexTuple))
    {
      elog(ERROR, "cache lookup failed for index %u", thisIndexOid);
    }
    indexForm = (Form_pg_index)GETSTRUCT(indexTuple);

       
                                                                         
                
       
    if (indexForm->indisclustered)
    {
      indexForm->indisclustered = false;
      CatalogTupleUpdate(pg_index, &indexTuple->t_self, indexTuple);
    }
    else if (thisIndexOid == indexOid)
    {
                                                            
      if (!indexForm->indisvalid)
      {
        elog(ERROR, "cannot cluster on invalid index %u", indexOid);
      }
      indexForm->indisclustered = true;
      CatalogTupleUpdate(pg_index, &indexTuple->t_self, indexTuple);
    }

    InvokeObjectPostAlterHookArg(IndexRelationId, thisIndexOid, 0, InvalidOid, is_internal);

    heap_freetuple(indexTuple);
  }

  table_close(pg_index, RowExclusiveLock);
}

   
                                                                             
   
                                                                      
                                                                              
   
                                                                         
   
static void
rebuild_relation(Relation OldHeap, Oid indexOid, bool verbose)
{
  Oid tableOid = RelationGetRelid(OldHeap);
  Oid tableSpace = OldHeap->rd_rel->reltablespace;
  Oid OIDNewHeap;
  char relpersistence;
  bool is_system_catalog;
  bool swap_toast_by_content;
  TransactionId frozenXid;
  MultiXactId cutoffMulti;

                                           
  if (OidIsValid(indexOid))
  {
    mark_index_clustered(OldHeap, indexOid, true);
  }

                                                      
  relpersistence = OldHeap->rd_rel->relpersistence;
  is_system_catalog = IsSystemRelation(OldHeap);

                                                                    
  table_close(OldHeap, NoLock);

                                                                        
  OIDNewHeap = make_new_heap(tableOid, tableSpace, relpersistence, AccessExclusiveLock);

                                                                  
  copy_table_data(OIDNewHeap, tableOid, indexOid, verbose, &swap_toast_by_content, &frozenXid, &cutoffMulti);

     
                                                                      
                                                                      
     
  finish_heap_swap(tableOid, OIDNewHeap, is_system_catalog, swap_toast_by_content, false, true, frozenXid, cutoffMulti, relpersistence);
}

   
                                                                       
                                                                      
                                                                     
                                                                            
                                                                           
   
                                                                             
                                                               
   
Oid
make_new_heap(Oid OIDOldHeap, Oid NewTableSpace, char relpersistence, LOCKMODE lockmode)
{
  TupleDesc OldHeapDesc;
  char NewHeapName[NAMEDATALEN];
  Oid OIDNewHeap;
  Oid toastid;
  Relation OldHeap;
  HeapTuple tuple;
  Datum reloptions;
  bool isNull;
  Oid namespaceid;

  OldHeap = table_open(OIDOldHeap, lockmode);
  OldHeapDesc = RelationGetDescr(OldHeap);

     
                                                                   
                                                                             
                                                                        
                  
     

     
                                                                    
     
  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(OIDOldHeap));
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "cache lookup failed for relation %u", OIDOldHeap);
  }
  reloptions = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_reloptions, &isNull);
  if (isNull)
  {
    reloptions = (Datum)0;
  }

  if (relpersistence == RELPERSISTENCE_TEMP)
  {
    namespaceid = LookupCreationNamespace("pg_temp");
  }
  else
  {
    namespaceid = RelationGetNamespace(OldHeap);
  }

     
                                                                          
                                                                          
                                                                           
                                                                            
                                                                            
     
                                                                            
                                                                             
                                                                     
                                                                     
     
  snprintf(NewHeapName, sizeof(NewHeapName), "pg_temp_%u", OIDOldHeap);

  OIDNewHeap = heap_create_with_catalog(NewHeapName, namespaceid, NewTableSpace, InvalidOid, InvalidOid, InvalidOid, OldHeap->rd_rel->relowner, OldHeap->rd_rel->relam, OldHeapDesc, NIL, RELKIND_RELATION, relpersistence, false, RelationIsMapped(OldHeap), ONCOMMIT_NOOP, reloptions, false, true, true, OIDOldHeap, NULL);
  Assert(OIDNewHeap != InvalidOid);

  ReleaseSysCache(tuple);

     
                                                                          
                                           
     
  CommandCounterIncrement();

     
                                                              
     
                                                                           
                                                                             
                                                                             
                                                 
     
                                                                             
                                                         
     
  toastid = OldHeap->rd_rel->reltoastrelid;
  if (OidIsValid(toastid))
  {
                                                            
    tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(toastid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for relation %u", toastid);
    }
    reloptions = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_reloptions, &isNull);
    if (isNull)
    {
      reloptions = (Datum)0;
    }

    NewHeapCreateToastTable(OIDNewHeap, reloptions, lockmode, toastid);

    ReleaseSysCache(tuple);
  }

  table_close(OldHeap, NoLock);

  return OIDNewHeap;
}

   
                                          
   
                                      
                                                                                
                                                                       
                                                                  
   
static void
copy_table_data(Oid OIDNewHeap, Oid OIDOldHeap, Oid OIDOldIndex, bool verbose, bool *pSwapToastByContent, TransactionId *pFreezeXid, MultiXactId *pCutoffMulti)
{
  Relation NewHeap, OldHeap, OldIndex;
  Relation relRelation;
  HeapTuple reltup;
  Form_pg_class relform;
  TupleDesc oldTupDesc PG_USED_FOR_ASSERTS_ONLY;
  TupleDesc newTupDesc PG_USED_FOR_ASSERTS_ONLY;
  TransactionId OldestXmin;
  TransactionId FreezeXid;
  MultiXactId MultiXactCutoff;
  bool use_sort;
  double num_tuples = 0, tups_vacuumed = 0, tups_recently_dead = 0;
  BlockNumber num_pages;
  int elevel = verbose ? INFO : DEBUG2;
  PGRUsage ru0;

  pg_rusage_init(&ru0);

     
                                 
     
  NewHeap = table_open(OIDNewHeap, AccessExclusiveLock);
  OldHeap = table_open(OIDOldHeap, AccessExclusiveLock);
  if (OidIsValid(OIDOldIndex))
  {
    OldIndex = index_open(OIDOldIndex, AccessExclusiveLock);
  }
  else
  {
    OldIndex = NULL;
  }

     
                                                                            
                                                       
     
  oldTupDesc = RelationGetDescr(OldHeap);
  newTupDesc = RelationGetDescr(NewHeap);
  Assert(newTupDesc->natts == oldTupDesc->natts);

     
                                                                           
                                                                          
                                                                          
                                                                         
                                                                             
                                                                            
                                                                        
             
     
                                                                            
                                           
     
  if (OldHeap->rd_rel->reltoastrelid)
  {
    LockRelationOid(OldHeap->rd_rel->reltoastrelid, AccessExclusiveLock);
  }

     
                                                                             
                                                                            
                                                                         
                                                                            
                                                                        
     
  if (OldHeap->rd_rel->reltoastrelid && NewHeap->rd_rel->reltoastrelid)
  {
    *pSwapToastByContent = true;

       
                                                                           
                                                                          
                                                                           
                                                                      
                                                                     
                                                                       
                           
       
                                                                           
                                                                           
                                                                         
                                                                       
                                                                          
                                                                         
                                                                         
                                                                    
       
    NewHeap->rd_toastoid = OldHeap->rd_rel->reltoastrelid;
  }
  else
  {
    *pSwapToastByContent = false;
  }

     
                                                                          
                                                                            
                                      
     
  vacuum_set_xid_limits(OldHeap, 0, 0, 0, 0, &OldestXmin, &FreezeXid, NULL, &MultiXactCutoff, NULL);

     
                                                                             
                                 
     
  if (TransactionIdIsValid(OldHeap->rd_rel->relfrozenxid) && TransactionIdPrecedes(FreezeXid, OldHeap->rd_rel->relfrozenxid))
  {
    FreezeXid = OldHeap->rd_rel->relfrozenxid;
  }

     
                                                                
     
  if (MultiXactIdIsValid(OldHeap->rd_rel->relminmxid) && MultiXactIdPrecedes(MultiXactCutoff, OldHeap->rd_rel->relminmxid))
  {
    MultiXactCutoff = OldHeap->rd_rel->relminmxid;
  }

     
                                                                             
                                                                            
                                                                             
                                                                        
                                   
     
  if (OldIndex != NULL && OldIndex->rd_rel->relam == BTREE_AM_OID)
  {
    use_sort = plan_cluster_use_sort(OIDOldHeap, OIDOldIndex);
  }
  else
  {
    use_sort = false;
  }

                            
  if (OldIndex != NULL && !use_sort)
  {
    ereport(elevel, (errmsg("clustering \"%s.%s\" using index scan on \"%s\"", get_namespace_name(RelationGetNamespace(OldHeap)), RelationGetRelationName(OldHeap), RelationGetRelationName(OldIndex))));
  }
  else if (use_sort)
  {
    ereport(elevel, (errmsg("clustering \"%s.%s\" using sequential scan and sort", get_namespace_name(RelationGetNamespace(OldHeap)), RelationGetRelationName(OldHeap))));
  }
  else
  {
    ereport(elevel, (errmsg("vacuuming \"%s.%s\"", get_namespace_name(RelationGetNamespace(OldHeap)), RelationGetRelationName(OldHeap))));
  }

     
                                                                          
                                                                        
                                                                        
                                                        
     
  table_relation_copy_for_cluster(OldHeap, NewHeap, OldIndex, use_sort, OldestXmin, &FreezeXid, &MultiXactCutoff, &num_tuples, &tups_vacuumed, &tups_recently_dead);

                                                                         
  *pFreezeXid = FreezeXid;
  *pCutoffMulti = MultiXactCutoff;

                                                                             
  NewHeap->rd_toastoid = InvalidOid;

  num_pages = RelationGetNumberOfBlocks(NewHeap);

                       
  ereport(elevel, (errmsg("\"%s\": found %.0f removable, %.0f nonremovable row versions in %u pages", RelationGetRelationName(OldHeap), tups_vacuumed, num_tuples, RelationGetNumberOfBlocks(OldHeap)), errdetail("%.0f dead row versions cannot be removed yet.\n"
                                                                                                                                                                                                                  "%s.",
                                                                                                                                                                                                            tups_recently_dead, pg_rusage_show(&ru0))));

  if (OldIndex != NULL)
  {
    index_close(OldIndex, NoLock);
  }
  table_close(OldHeap, NoLock);
  table_close(NewHeap, NoLock);

                                                                          
  relRelation = table_open(RelationRelationId, RowExclusiveLock);

  reltup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(OIDNewHeap));
  if (!HeapTupleIsValid(reltup))
  {
    elog(ERROR, "cache lookup failed for relation %u", OIDNewHeap);
  }
  relform = (Form_pg_class)GETSTRUCT(reltup);

  relform->relpages = num_pages;
  relform->reltuples = num_tuples;

                                                                      
  if (OIDOldHeap != RelationRelationId)
  {
    CatalogTupleUpdate(relRelation, &reltup->t_self, reltup);
  }
  else
  {
    CacheInvalidateRelcacheByTuple(reltup);
  }

                 
  heap_freetuple(reltup);
  table_close(relRelation, RowExclusiveLock);

                               
  CommandCounterIncrement();
}

   
                                                   
   
                                                                                
                                                                         
                                                                              
             
   
                                                                             
                                                                             
                                                                               
                                                                              
                                                                            
                                         
   
                                                                       
                                                                           
                                                                          
                                                                          
                                                                           
                                                              
   
                                                                          
                                                                         
                                                                      
   
static void
swap_relation_files(Oid r1, Oid r2, bool target_is_pg_class, bool swap_toast_by_content, bool is_internal, TransactionId frozenXid, MultiXactId cutoffMulti, Oid *mapped_tables)
{
  Relation relRelation;
  HeapTuple reltup1, reltup2;
  Form_pg_class relform1, relform2;
  Oid relfilenode1, relfilenode2;
  Oid swaptemp;
  char swptmpchr;

                                                        
  relRelation = table_open(RelationRelationId, RowExclusiveLock);

  reltup1 = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(r1));
  if (!HeapTupleIsValid(reltup1))
  {
    elog(ERROR, "cache lookup failed for relation %u", r1);
  }
  relform1 = (Form_pg_class)GETSTRUCT(reltup1);

  reltup2 = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(r2));
  if (!HeapTupleIsValid(reltup2))
  {
    elog(ERROR, "cache lookup failed for relation %u", r2);
  }
  relform2 = (Form_pg_class)GETSTRUCT(reltup2);

  relfilenode1 = relform1->relfilenode;
  relfilenode2 = relform2->relfilenode;

  if (OidIsValid(relfilenode1) && OidIsValid(relfilenode2))
  {
       
                                                                       
                      
       
    Assert(!target_is_pg_class);

    swaptemp = relform1->relfilenode;
    relform1->relfilenode = relform2->relfilenode;
    relform2->relfilenode = swaptemp;

    swaptemp = relform1->reltablespace;
    relform1->reltablespace = relform2->reltablespace;
    relform2->reltablespace = swaptemp;

    swptmpchr = relform1->relpersistence;
    relform1->relpersistence = relform2->relpersistence;
    relform2->relpersistence = swptmpchr;

                                                           
    if (!swap_toast_by_content)
    {
      swaptemp = relform1->reltoastrelid;
      relform1->reltoastrelid = relform2->reltoastrelid;
      relform2->reltoastrelid = swaptemp;
    }
  }
  else
  {
       
                                                                         
                                                                        
       
    if (OidIsValid(relfilenode1) || OidIsValid(relfilenode2))
    {
      elog(ERROR, "cannot swap mapped relation \"%s\" with non-mapped relation", NameStr(relform1->relname));
    }

       
                                                                           
                                                                           
                                                                        
                                                                          
                                               
       
    if (relform1->reltablespace != relform2->reltablespace)
    {
      elog(ERROR, "cannot change tablespace of mapped relation \"%s\"", NameStr(relform1->relname));
    }
    if (relform1->relpersistence != relform2->relpersistence)
    {
      elog(ERROR, "cannot change persistence of mapped relation \"%s\"", NameStr(relform1->relname));
    }
    if (!swap_toast_by_content && (relform1->reltoastrelid || relform2->reltoastrelid))
    {
      elog(ERROR, "cannot swap toast by links for mapped relation \"%s\"", NameStr(relform1->relname));
    }

       
                                                              
       
    relfilenode1 = RelationMapOidToFilenode(r1, relform1->relisshared);
    if (!OidIsValid(relfilenode1))
    {
      elog(ERROR, "could not find relation mapping for relation \"%s\", OID %u", NameStr(relform1->relname), r1);
    }
    relfilenode2 = RelationMapOidToFilenode(r2, relform2->relisshared);
    if (!OidIsValid(relfilenode2))
    {
      elog(ERROR, "could not find relation mapping for relation \"%s\", OID %u", NameStr(relform2->relname), r2);
    }

       
                                                                          
                                                  
       
    RelationMapUpdateMap(r1, relfilenode2, relform1->relisshared, false);
    RelationMapUpdateMap(r2, relfilenode1, relform2->relisshared, false);

                                                      
    *mapped_tables++ = r2;
  }

     
                                                                            
                                                                            
                                                                           
                                                                            
                                                  
     

                                                  
  if (relform1->relkind != RELKIND_INDEX)
  {
    Assert(!TransactionIdIsValid(frozenXid) || TransactionIdIsNormal(frozenXid));
    relform1->relfrozenxid = frozenXid;
    relform1->relminmxid = cutoffMulti;
  }

                                                                         
  {
    int32 swap_pages;
    float4 swap_tuples;
    int32 swap_allvisible;

    swap_pages = relform1->relpages;
    relform1->relpages = relform2->relpages;
    relform2->relpages = swap_pages;

    swap_tuples = relform1->reltuples;
    relform1->reltuples = relform2->reltuples;
    relform2->reltuples = swap_tuples;

    swap_allvisible = relform1->relallvisible;
    relform1->relallvisible = relform2->relallvisible;
    relform2->relallvisible = swap_allvisible;
  }

     
                                                                         
                                                                           
                                                                             
                                                                           
                                                                            
                                                                             
                                                    
     
  if (!target_is_pg_class)
  {
    CatalogIndexState indstate;

    indstate = CatalogOpenIndexes(relRelation);
    CatalogTupleUpdateWithInfo(relRelation, &reltup1->t_self, reltup1, indstate);
    CatalogTupleUpdateWithInfo(relRelation, &reltup2->t_self, reltup2, indstate);
    CatalogCloseIndexes(indstate);
  }
  else
  {
                                                           
    CacheInvalidateRelcacheByTuple(reltup1);
    CacheInvalidateRelcacheByTuple(reltup2);
  }

     
                                                                        
                                                         
     
  InvokeObjectPostAlterHookArg(RelationRelationId, r1, 0, InvalidOid, is_internal);
  InvokeObjectPostAlterHookArg(RelationRelationId, r2, 0, InvalidOid, true);

     
                                                                          
                         
     
  if (relform1->reltoastrelid || relform2->reltoastrelid)
  {
    if (swap_toast_by_content)
    {
      if (relform1->reltoastrelid && relform2->reltoastrelid)
      {
                                                               
        swap_relation_files(relform1->reltoastrelid, relform2->reltoastrelid, target_is_pg_class, swap_toast_by_content, is_internal, frozenXid, cutoffMulti, mapped_tables);
      }
      else
      {
                              
        elog(ERROR, "cannot swap toast files by content when there's only one");
      }
    }
    else
    {
         
                                                                         
                        
         
                                                                     
         
                                                                         
                                                                       
                                                                       
                                           
         
      ObjectAddress baseobject, toastobject;
      long count;

         
                                                                 
                                                                     
                                                                        
                                                        
         
      if (IsSystemClass(r1, relform1))
      {
        elog(ERROR, "cannot swap toast files by links for system catalogs");
      }

                                   
      if (relform1->reltoastrelid)
      {
        count = deleteDependencyRecordsFor(RelationRelationId, relform1->reltoastrelid, false);
        if (count != 1)
        {
          elog(ERROR, "expected one dependency record for TOAST table, found %ld", count);
        }
      }
      if (relform2->reltoastrelid)
      {
        count = deleteDependencyRecordsFor(RelationRelationId, relform2->reltoastrelid, false);
        if (count != 1)
        {
          elog(ERROR, "expected one dependency record for TOAST table, found %ld", count);
        }
      }

                                     
      baseobject.classId = RelationRelationId;
      baseobject.objectSubId = 0;
      toastobject.classId = RelationRelationId;
      toastobject.objectSubId = 0;

      if (relform1->reltoastrelid)
      {
        baseobject.objectId = r1;
        toastobject.objectId = relform1->reltoastrelid;
        recordDependencyOn(&toastobject, &baseobject, DEPENDENCY_INTERNAL);
      }

      if (relform2->reltoastrelid)
      {
        baseobject.objectId = r2;
        toastobject.objectId = relform2->reltoastrelid;
        recordDependencyOn(&toastobject, &baseobject, DEPENDENCY_INTERNAL);
      }
    }
  }

     
                                                                          
                                                                             
                   
     
  if (swap_toast_by_content && relform1->relkind == RELKIND_TOASTVALUE && relform2->relkind == RELKIND_TOASTVALUE)
  {
    Oid toastIndex1, toastIndex2;

                                           
    toastIndex1 = toast_get_valid_index(r1, AccessExclusiveLock);
    toastIndex2 = toast_get_valid_index(r2, AccessExclusiveLock);

    swap_relation_files(toastIndex1, toastIndex2, target_is_pg_class, swap_toast_by_content, is_internal, InvalidTransactionId, InvalidMultiXactId, mapped_tables);
  }

                 
  heap_freetuple(reltup1);
  heap_freetuple(reltup2);

  table_close(relRelation, RowExclusiveLock);

     
                                                                          
                                                                             
                                                                            
                                                                            
                                                                            
                                                                         
                                                                         
                                                                     
                              
     
                                                                        
                                                                             
                                                                          
                    
     
  RelationCloseSmgrByOid(r1);
  RelationCloseSmgrByOid(r2);
}

   
                                                                          
                                                                   
   
void
finish_heap_swap(Oid OIDOldHeap, Oid OIDNewHeap, bool is_system_catalog, bool swap_toast_by_content, bool check_constraints, bool is_internal, TransactionId frozenXid, MultiXactId cutoffMulti, char newrelpersistence)
{
  ObjectAddress object;
  Oid mapped_tables[4];
  int reindex_flags;
  int i;

                                                      
  pgstat_progress_update_param(PROGRESS_CLUSTER_PHASE, PROGRESS_CLUSTER_PHASE_SWAP_REL_FILES);

                                                             
  memset(mapped_tables, 0, sizeof(mapped_tables));

     
                                                                           
                                                    
     
  swap_relation_files(OIDOldHeap, OIDNewHeap, (OIDOldHeap == RelationRelationId), swap_toast_by_content, is_internal, frozenXid, cutoffMulti, mapped_tables);

     
                                                                             
                                                           
     
  if (is_system_catalog)
  {
    CacheInvalidateCatalog(OIDOldHeap);
  }

     
                                                                           
                                                                         
                                                                          
                                                                      
                                                                            
                                                                         
                                                                       
     
                                                                             
                                                                            
                                                                             
                                                                         
                                                                
     
  reindex_flags = REINDEX_REL_SUPPRESS_INDEX_USE;
  if (check_constraints)
  {
    reindex_flags |= REINDEX_REL_CHECK_CONSTRAINTS;
  }

     
                                                                     
               
     
  if (newrelpersistence == RELPERSISTENCE_UNLOGGED)
  {
    reindex_flags |= REINDEX_REL_FORCE_INDEXES_UNLOGGED;
  }
  else if (newrelpersistence == RELPERSISTENCE_PERMANENT)
  {
    reindex_flags |= REINDEX_REL_FORCE_INDEXES_PERMANENT;
  }

                                                   
  pgstat_progress_update_param(PROGRESS_CLUSTER_PHASE, PROGRESS_CLUSTER_PHASE_REBUILD_INDEX);

  reindex_relation(OIDOldHeap, reindex_flags, 0);

                                             
  pgstat_progress_update_param(PROGRESS_CLUSTER_PHASE, PROGRESS_CLUSTER_PHASE_FINAL_CLEANUP);

     
                                                                      
                                                                      
                                                                       
                                                                      
                                                                       
                                                                     
                                                                            
                                                                            
                                                                             
                                               
     
  if (OIDOldHeap == RelationRelationId)
  {
    Relation relRelation;
    HeapTuple reltup;
    Form_pg_class relform;

    relRelation = table_open(RelationRelationId, RowExclusiveLock);

    reltup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(OIDOldHeap));
    if (!HeapTupleIsValid(reltup))
    {
      elog(ERROR, "cache lookup failed for relation %u", OIDOldHeap);
    }
    relform = (Form_pg_class)GETSTRUCT(reltup);

    relform->relfrozenxid = frozenXid;
    relform->relminmxid = cutoffMulti;

    CatalogTupleUpdate(relRelation, &reltup->t_self, reltup);

    table_close(relRelation, RowExclusiveLock);
  }

                                          
  object.classId = RelationRelationId;
  object.objectId = OIDNewHeap;
  object.objectSubId = 0;

     
                                                                      
                                                   
     
  performDeletion(&object, DROP_RESTRICT, PERFORM_DELETION_INTERNAL);

                                                           

     
                                                                            
                                                                            
                                                                             
                                                       
     
  for (i = 0; OidIsValid(mapped_tables[i]); i++)
  {
    RelationMapRemoveMapping(mapped_tables[i]);
  }

     
                                                                           
                                                                          
                                                                           
                                                                        
                           
     
                                                                       
                           
     
  if (!swap_toast_by_content)
  {
    Relation newrel;

    newrel = table_open(OIDOldHeap, NoLock);
    if (OidIsValid(newrel->rd_rel->reltoastrelid))
    {
      Oid toastidx;
      char NewToastName[NAMEDATALEN];

                                                        
      toastidx = toast_get_valid_index(newrel->rd_rel->reltoastrelid, NoLock);

                                      
      snprintf(NewToastName, NAMEDATALEN, "pg_toast_%u", OIDOldHeap);
      RenameRelationInternal(newrel->rd_rel->reltoastrelid, NewToastName, true, false);

                                        
      snprintf(NewToastName, NAMEDATALEN, "pg_toast_%u_index", OIDOldHeap);

      RenameRelationInternal(toastidx, NewToastName, true, true);

         
                                                                 
                                                              
                                                                      
         
      CommandCounterIncrement();
      ResetRelRewrite(newrel->rd_rel->reltoastrelid);
    }
    relation_close(newrel, NoLock);
  }

                                                                         
  if (!is_system_catalog)
  {
    Relation newrel;

    newrel = table_open(OIDOldHeap, NoLock);
    RelationClearMissing(newrel);
    relation_close(newrel, NoLock);
  }
}

   
                                                       
                                                                         
                                                                    
              
   
static List *
get_tables_to_cluster(MemoryContext cluster_context)
{
  Relation indRelation;
  TableScanDesc scan;
  ScanKeyData entry;
  HeapTuple indexTuple;
  Form_pg_index index;
  MemoryContext old_context;
  RelToCluster *rvtc;
  List *rvs = NIL;

     
                                                                   
                                                                           
                                                                         
                                          
     
  indRelation = table_open(IndexRelationId, AccessShareLock);
  ScanKeyInit(&entry, Anum_pg_index_indisclustered, BTEqualStrategyNumber, F_BOOLEQ, BoolGetDatum(true));
  scan = table_beginscan_catalog(indRelation, 1, &entry);
  while ((indexTuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    index = (Form_pg_index)GETSTRUCT(indexTuple);

    if (!pg_class_ownercheck(index->indrelid, GetUserId()))
    {
      continue;
    }

       
                                                                          
                                                
       
    old_context = MemoryContextSwitchTo(cluster_context);

    rvtc = (RelToCluster *)palloc(sizeof(RelToCluster));
    rvtc->tableOid = index->indrelid;
    rvtc->indexOid = index->indexrelid;
    rvs = lcons(rvtc, rvs);

    MemoryContextSwitchTo(old_context);
  }
  table_endscan(scan);

  relation_close(indRelation, AccessShareLock);

  return rvs;
}
