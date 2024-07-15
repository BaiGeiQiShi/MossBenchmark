                                                                            
   
           
                                          
   
                                                                         
                                                                        
   
   
                  
                                      
   
         
                                                                  
                                              
   
                                                                            
   

#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/relscan.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "catalog/index.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/rls.h"
#include "utils/ruleutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

                                                                    
                                   
   
                                                                
                                                                       
                                 
   
                                                                         
                                                                        
                                       
   
                                                                       
                                                                        
                                                                        
                                                                  
   
                                                                   
                                                                      
                                                                     
                           
                                                                    
   

                    
                                                             
   
                                                                     
                     
   
                
                                                
                                                           
                                                      
   
             
                                    
                    
   
IndexScanDesc
RelationGetIndexScan(Relation indexRelation, int nkeys, int norderbys)
{
  IndexScanDesc scan;

  scan = (IndexScanDesc)palloc(sizeof(IndexScanDescData));

  scan->heapRelation = NULL;                       
  scan->xs_heapfetch = NULL;
  scan->indexRelation = indexRelation;
  scan->xs_snapshot = InvalidSnapshot;                                  
  scan->numberOfKeys = nkeys;
  scan->numberOfOrderBys = norderbys;

     
                                                                             
     
  if (nkeys > 0)
  {
    scan->keyData = (ScanKey)palloc(sizeof(ScanKeyData) * nkeys);
  }
  else
  {
    scan->keyData = NULL;
  }
  if (norderbys > 0)
  {
    scan->orderByData = (ScanKey)palloc(sizeof(ScanKeyData) * norderbys);
  }
  else
  {
    scan->orderByData = NULL;
  }

  scan->xs_want_itup = false;                       

     
                                                                           
                                                                             
                                                                       
                                                                           
                                                                           
                                                                             
                                                                         
                                         
     
  scan->kill_prior_tuple = false;
  scan->xactStartedInRecovery = TransactionStartedDuringRecovery();
  scan->ignore_killed_tuples = !scan->xactStartedInRecovery;

  scan->opaque = NULL;

  scan->xs_itup = NULL;
  scan->xs_itupdesc = NULL;
  scan->xs_hitup = NULL;
  scan->xs_hitupdesc = NULL;

  return scan;
}

                    
                                      
   
                                                       
                                                        
                                                      
                     
   
            
          
                    
   
void
IndexScanEnd(IndexScanDesc scan)
{
  if (scan->keyData != NULL)
  {
    pfree(scan->keyData);
  }
  if (scan->orderByData != NULL)
  {
    pfree(scan->orderByData);
  }

  pfree(scan);
}

   
                              
   
                                                                        
                                                                    
                                                                           
                                                             
   
                                                                      
                                                                            
                                                                             
                                                            
   
                                                                          
                                                                             
                                                                
   
                                                    
                                                                        
           
   
char *
BuildIndexValueDescription(Relation indexRelation, Datum *values, bool *isnull)
{
  StringInfoData buf;
  Form_pg_index idxrec;
  int indnkeyatts;
  int i;
  int keyno;
  Oid indexrelid = RelationGetRelid(indexRelation);
  Oid indrelid;
  AclResult aclresult;

  indnkeyatts = IndexRelationGetNumberOfKeyAttributes(indexRelation);

     
                                                                            
                                                         
     
                                                                            
                         
     
                                                                           
                                                      
     
  idxrec = indexRelation->rd_index;
  indrelid = idxrec->indrelid;
  Assert(indexrelid == idxrec->indexrelid);

                                                                   
  if (check_enable_rls(indrelid, InvalidOid, true) == RLS_ENABLED)
  {
    return NULL;
  }

                                                        
  aclresult = pg_class_aclcheck(indrelid, GetUserId(), ACL_SELECT);
  if (aclresult != ACLCHECK_OK)
  {
       
                                                                           
                                                            
       
    for (keyno = 0; keyno < indnkeyatts; keyno++)
    {
      AttrNumber attnum = idxrec->indkey.values[keyno];

         
                                                                         
                                                                        
                                                                         
                                         
         
      if (attnum == InvalidAttrNumber || pg_attribute_aclcheck(indrelid, attnum, GetUserId(), ACL_SELECT) != ACLCHECK_OK)
      {
                                               
        return NULL;
      }
    }
  }

  initStringInfo(&buf);
  appendStringInfo(&buf, "(%s)=(", pg_get_indexdef_columns(indexrelid, true));

  for (i = 0; i < indnkeyatts; i++)
  {
    char *val;

    if (isnull[i])
    {
      val = "null";
    }
    else
    {
      Oid foutoid;
      bool typisvarlena;

         
                                                                        
                                                                        
                                                
         
                                                                      
                                                                 
                                                                       
                                                          
         
      getTypeOutputInfo(indexRelation->rd_opcintype[i], &foutoid, &typisvarlena);
      val = OidOutputFunctionCall(foutoid, values[i]);
    }

    if (i > 0)
    {
      appendStringInfoString(&buf, ", ");
    }
    appendStringInfoString(&buf, val);
  }

  appendStringInfoChar(&buf, ')');

  return buf.data;
}

   
                                                                           
                         
   
TransactionId
index_compute_xid_horizon_for_tuples(Relation irel, Relation hrel, Buffer ibuf, OffsetNumber *itemnos, int nitems)
{
  ItemPointerData *ttids = (ItemPointerData *)palloc(sizeof(ItemPointerData) * nitems);
  TransactionId latestRemovedXid = InvalidTransactionId;
  Page ipage = BufferGetPage(ibuf);
  IndexTuple itup;

                                                                   
  for (int i = 0; i < nitems; i++)
  {
    ItemId iitemid;

    iitemid = PageGetItemId(ipage, itemnos[i]);
    itup = (IndexTuple)PageGetItem(ipage, iitemid);

    ItemPointerCopy(&itup->t_tid, &ttids[i]);
  }

                                        
  latestRemovedXid = table_compute_xid_horizon_for_tuples(hrel, ttids, nitems);

  pfree(ttids);

  return latestRemovedXid;
}

                                                                    
                                                 
   
                                                                      
                                                                    
                                           
   
                                                                     
                                                                     
                                                               
   
                                                                
                                                                    
                                                              
                                                     
                                                                    
   

   
                                                        
   
                                                            
                                              
                                                           
                                                                   
                         
   
                                                                          
                                                                       
                                                                         
                                                                            
                                                             
   
                                                                         
                                                                           
                                                         
   
SysScanDesc
systable_beginscan(Relation heapRelation, Oid indexId, bool indexOK, Snapshot snapshot, int nkeys, ScanKey key)
{
  SysScanDesc sysscan;
  Relation irel;

  if (indexOK && !IgnoreSystemIndexes && !ReindexIsProcessingIndex(indexId))
  {
    irel = index_open(indexId, AccessShareLock);
  }
  else
  {
    irel = NULL;
  }

  sysscan = (SysScanDesc)palloc(sizeof(SysScanDescData));

  sysscan->heap_rel = heapRelation;
  sysscan->irel = irel;
  sysscan->slot = table_slot_create(heapRelation, NULL);

  if (snapshot == NULL)
  {
    Oid relid = RelationGetRelid(heapRelation);

    snapshot = RegisterSnapshot(GetCatalogSnapshot(relid));
    sysscan->snapshot = snapshot;
  }
  else
  {
                                                 
    sysscan->snapshot = NULL;
  }

  if (irel)
  {
    int i;

                                                              
    for (i = 0; i < nkeys; i++)
    {
      int j;

      for (j = 0; j < IndexRelationGetNumberOfAttributes(irel); j++)
      {
        if (key[i].sk_attno == irel->rd_index->indkey.values[j])
        {
          key[i].sk_attno = j + 1;
          break;
        }
      }
      if (j == IndexRelationGetNumberOfAttributes(irel))
      {
        elog(ERROR, "column is not in index");
      }
    }

    sysscan->iscan = index_beginscan(heapRelation, irel, snapshot, nkeys, 0);
    index_rescan(sysscan->iscan, key, nkeys, NULL, 0);
    sysscan->scan = NULL;
  }
  else
  {
       
                                                                         
                                                                       
                                                                     
                                                                       
                                                             
       
    sysscan->scan = table_beginscan_strat(heapRelation, snapshot, nkeys, key, true, false);
    sysscan->iscan = NULL;
  }

  return sysscan;
}

   
                                                               
   
                                             
   
                                                                     
                                                                      
                                     
   
                                                                           
               
   
HeapTuple
systable_getnext(SysScanDesc sysscan)
{
  HeapTuple htup = NULL;

  if (sysscan->irel)
  {
    if (index_getnext_slot(sysscan->iscan, ForwardScanDirection, sysscan->slot))
    {
      bool shouldFree;

      htup = ExecFetchSlotHeapTuple(sysscan->slot, false, &shouldFree);
      Assert(!shouldFree);

         
                                                                      
                                                                         
                                                                      
                                                                     
                                                              
                      
         
      if (sysscan->iscan->xs_recheck)
      {
        elog(ERROR, "system catalog scans with lossy index conditions are not implemented");
      }
    }
  }
  else
  {
    if (table_scan_getnextslot(sysscan->scan, ForwardScanDirection, sysscan->slot))
    {
      bool shouldFree;

      htup = ExecFetchSlotHeapTuple(sysscan->slot, false, &shouldFree);
      Assert(!shouldFree);
    }
  }

  return htup;
}

   
                                                                                
   
                                                                             
                                                                            
                                     
   
                                                                           
                       
   
                                                                        
                                                                     
   
bool
systable_recheck_tuple(SysScanDesc sysscan, HeapTuple tup)
{
  Snapshot freshsnap;
  bool result;

  Assert(tup == ExecFetchSlotHeapTuple(sysscan->slot, false, NULL));

     
                                                                      
                                                                            
                                                                     
                                                                        
     
  freshsnap = GetCatalogSnapshot(RelationGetRelid(sysscan->heap_rel));

  result = table_tuple_satisfies_snapshot(sysscan->heap_rel, sysscan->slot, freshsnap);

  return result;
}

   
                                                      
   
                                                                     
   
void
systable_endscan(SysScanDesc sysscan)
{
  if (sysscan->slot)
  {
    ExecDropSingleTupleTableSlot(sysscan->slot);
    sysscan->slot = NULL;
  }

  if (sysscan->irel)
  {
    index_endscan(sysscan->iscan);
    index_close(sysscan->irel, AccessShareLock);
  }
  else
  {
    table_endscan(sysscan->scan);
  }

  if (sysscan->snapshot)
  {
    UnregisterSnapshot(sysscan->snapshot);
  }

  pfree(sysscan);
}

   
                                                                  
   
                                                                           
                                                                    
                                                                        
                                                 
   
                                                                          
                                                                    
                                                                      
                                                                         
                                                                             
                                                                         
                     
   
SysScanDesc
systable_beginscan_ordered(Relation heapRelation, Relation indexRelation, Snapshot snapshot, int nkeys, ScanKey key)
{
  SysScanDesc sysscan;
  int i;

                                                     
  if (ReindexIsProcessingIndex(RelationGetRelid(indexRelation)))
  {
    elog(ERROR, "cannot do ordered scan on index \"%s\", because it is being reindexed", RelationGetRelationName(indexRelation));
  }
                                                                           
  if (IgnoreSystemIndexes)
  {
    elog(WARNING, "using index \"%s\" despite IgnoreSystemIndexes", RelationGetRelationName(indexRelation));
  }

  sysscan = (SysScanDesc)palloc(sizeof(SysScanDescData));

  sysscan->heap_rel = heapRelation;
  sysscan->irel = indexRelation;
  sysscan->slot = table_slot_create(heapRelation, NULL);

  if (snapshot == NULL)
  {
    Oid relid = RelationGetRelid(heapRelation);

    snapshot = RegisterSnapshot(GetCatalogSnapshot(relid));
    sysscan->snapshot = snapshot;
  }
  else
  {
                                                 
    sysscan->snapshot = NULL;
  }

                                                            
  for (i = 0; i < nkeys; i++)
  {
    int j;

    for (j = 0; j < IndexRelationGetNumberOfAttributes(indexRelation); j++)
    {
      if (key[i].sk_attno == indexRelation->rd_index->indkey.values[j])
      {
        key[i].sk_attno = j + 1;
        break;
      }
    }
    if (j == IndexRelationGetNumberOfAttributes(indexRelation))
    {
      elog(ERROR, "column is not in index");
    }
  }

  sysscan->iscan = index_beginscan(heapRelation, indexRelation, snapshot, nkeys, 0);
  index_rescan(sysscan->iscan, key, nkeys, NULL, 0);
  sysscan->scan = NULL;

  return sysscan;
}

   
                                                                          
   
HeapTuple
systable_getnext_ordered(SysScanDesc sysscan, ScanDirection direction)
{
  HeapTuple htup = NULL;

  Assert(sysscan->irel);
  if (index_getnext_slot(sysscan->iscan, direction, sysscan->slot))
  {
    htup = ExecFetchSlotHeapTuple(sysscan->slot, false, NULL);
  }

                                     
  if (htup && sysscan->iscan->xs_recheck)
  {
    elog(ERROR, "system catalog scans with lossy index conditions are not implemented");
  }

  return htup;
}

   
                                                              
   
void
systable_endscan_ordered(SysScanDesc sysscan)
{
  if (sysscan->slot)
  {
    ExecDropSingleTupleTableSlot(sysscan->slot);
    sysscan->slot = NULL;
  }

  Assert(sysscan->irel);
  index_endscan(sysscan->iscan);
  if (sysscan->snapshot)
  {
    UnregisterSnapshot(sysscan->snapshot);
  }
  pfree(sysscan);
}
