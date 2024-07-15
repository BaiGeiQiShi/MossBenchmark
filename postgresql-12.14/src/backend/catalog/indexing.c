                                                                            
   
              
                                                                      
               
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/pg_subscription.h"
#include "catalog/pg_subscription_rel.h"
#include "executor/executor.h"
#include "utils/rel.h"

   
                                                              
   
                                                                    
                                                     
   
                                                                        
                                                                       
                                                                    
                                                                      
                                                         
                                                                       
                                             
   
CatalogIndexState
CatalogOpenIndexes(Relation heapRel)
{
  ResultRelInfo *resultRelInfo;

  resultRelInfo = makeNode(ResultRelInfo);
  resultRelInfo->ri_RangeTableIndex = 0;            
  resultRelInfo->ri_RelationDesc = heapRel;
  resultRelInfo->ri_TrigDesc = NULL;                             

  ExecOpenIndices(resultRelInfo, false);

  return resultRelInfo;
}

   
                                                                            
   
void
CatalogCloseIndexes(CatalogIndexState indstate)
{
  ExecCloseIndices(indstate);
  pfree(indstate);
}

   
                                                                   
   
                                                                     
   
                                                                    
   
static void
CatalogIndexInsert(CatalogIndexState indstate, HeapTuple heapTuple)
{
  int i;
  int numIndexes;
  RelationPtr relationDescs;
  Relation heapRelation;
  TupleTableSlot *slot;
  IndexInfo **indexInfoArray;
  Datum values[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];

     
                                                                            
                                                                   
                  
     
#ifndef USE_ASSERT_CHECKING
  if (HeapTupleIsHeapOnly(heapTuple))
  {
    return;
  }
#endif

     
                                                                           
     
  numIndexes = indstate->ri_NumIndices;
  if (numIndexes == 0)
  {
    return;
  }
  relationDescs = indstate->ri_IndexRelationDescs;
  indexInfoArray = indstate->ri_IndexRelationInfo;
  heapRelation = indstate->ri_RelationDesc;

                                                    
  slot = MakeSingleTupleTableSlot(RelationGetDescr(heapRelation), &TTSOpsHeapTuple);
  ExecStoreHeapTuple(heapTuple, slot, false);

     
                                                     
     
  for (i = 0; i < numIndexes; i++)
  {
    IndexInfo *indexInfo;
    Relation index;

    indexInfo = indexInfoArray[i];
    index = relationDescs[i];

                                                        
    if (!indexInfo->ii_ReadyForInserts)
    {
      continue;
    }

       
                                                                   
                                                                     
       
    Assert(indexInfo->ii_Expressions == NIL);
    Assert(indexInfo->ii_Predicate == NIL);
    Assert(indexInfo->ii_ExclusionOps == NULL);
    Assert(index->rd_index->indimmediate);
    Assert(indexInfo->ii_NumIndexKeyAttrs != 0);

                                 
#ifdef USE_ASSERT_CHECKING
    if (HeapTupleIsHeapOnly(heapTuple))
    {
      Assert(!ReindexIsProcessingIndex(RelationGetRelid(index)));
      continue;
    }
#endif                          

       
                                                                         
                                                          
       
    FormIndexDatum(indexInfo, slot, NULL,                               
        values, isnull);

       
                                   
       
    index_insert(index,                           
        values,                                          
        isnull,                                  
        &(heapTuple->t_self),                        
        heapRelation, index->rd_index->indisunique ? UNIQUE_CHECK_YES : UNIQUE_CHECK_NO, indexInfo);
  }

  ExecDropSingleTupleTableSlot(slot);
}

   
                                                              
   
                                                                           
                                                                            
                                                                              
                                                                               
   
#ifdef USE_ASSERT_CHECKING

static void
CatalogTupleCheckConstraints(Relation heapRel, HeapTuple tup)
{
     
                                                                         
                             
     
  if (HeapTupleHasNulls(tup))
  {
    TupleDesc tupdesc = RelationGetDescr(heapRel);
    bits8 *bp = tup->t_data->t_bits;

    for (int attnum = 0; attnum < tupdesc->natts; attnum++)
    {
      Form_pg_attribute thisatt = TupleDescAttr(tupdesc, attnum);

         
                                                                       
                                                                      
                                                                       
                
         
      Assert(!(thisatt->attnotnull && att_isnull(attnum, bp) && !((thisatt->attrelid == SubscriptionRelationId && thisatt->attnum == Anum_pg_subscription_subslotname) || (thisatt->attrelid == SubscriptionRelRelationId && thisatt->attnum == Anum_pg_subscription_rel_srsublsn))));
    }
  }
}

#else                           

#define CatalogTupleCheckConstraints(heapRel, tup) ((void)0)

#endif                          

   
                                                                          
   
                                                                       
                                              
   
                                                                           
                                                                           
                                                                           
                                                                   
                                                   
   
void
CatalogTupleInsert(Relation heapRel, HeapTuple tup)
{
  CatalogIndexState indstate;

  CatalogTupleCheckConstraints(heapRel, tup);

  indstate = CatalogOpenIndexes(heapRel);

  simple_heap_insert(heapRel, tup);

  CatalogIndexInsert(indstate, tup);
  CatalogCloseIndexes(indstate);
}

   
                                                                              
   
                                                                           
                                                                          
                                                                              
                                                                           
   
void
CatalogTupleInsertWithInfo(Relation heapRel, HeapTuple tup, CatalogIndexState indstate)
{
  CatalogTupleCheckConstraints(heapRel, tup);

  simple_heap_insert(heapRel, tup);

  CatalogIndexInsert(indstate, tup);
}

   
                                                                               
   
                                                                               
   
                                                                          
                                                                         
                                                                           
                                                                   
                                                   
   
void
CatalogTupleUpdate(Relation heapRel, ItemPointer otid, HeapTuple tup)
{
  CatalogIndexState indstate;

  CatalogTupleCheckConstraints(heapRel, tup);

  indstate = CatalogOpenIndexes(heapRel);

  simple_heap_update(heapRel, otid, tup);

  CatalogIndexInsert(indstate, tup);
  CatalogCloseIndexes(indstate);
}

   
                                                                              
   
                                                                           
                                                                       
                                                                              
                                                                           
   
void
CatalogTupleUpdateWithInfo(Relation heapRel, ItemPointer otid, HeapTuple tup, CatalogIndexState indstate)
{
  CatalogTupleCheckConstraints(heapRel, tup);

  simple_heap_update(heapRel, otid, tup);

  CatalogIndexInsert(indstate, tup);
}

   
                                                                               
   
                                                                  
   
                                                                       
                                                                            
                                                                        
                                                                          
   
                                                                        
                                                                             
                                                                              
                                                                       
   
void
CatalogTupleDelete(Relation heapRel, ItemPointer tid)
{
  simple_heap_delete(heapRel, tid);
}
