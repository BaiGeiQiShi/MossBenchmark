                                                                            
   
              
                                                     
   
                                                                           
                                                                        
                                                                             
   
                                                                     
                                                                      
                                                                      
                                                                          
                                 
   
   
                                                                         
                                                                        
   
                  
                                        
   
                                                                            
   

#include "postgres.h"

#include "access/hash.h"
#include "commands/progress.h"
#include "miscadmin.h"
#include "pgstat.h"
#include "utils/tuplesort.h"

   
                                             
   
struct HSpool
{
  Tuplesortstate *sortstate;                                 
  Relation index;

     
                                                                            
                                                                            
          
     
  uint32 high_mask;
  uint32 low_mask;
  uint32 max_buckets;
};

   
                                           
   
HSpool *
_h_spoolinit(Relation heap, Relation index, uint32 num_buckets)
{
  HSpool *hspool = (HSpool *)palloc0(sizeof(HSpool));

  hspool->index = index;

     
                                                                            
                                                                            
                 
     
                                                                      
                                           
     
  hspool->high_mask = (((uint32)1) << _hash_log2(num_buckets + 1)) - 1;
  hspool->low_mask = (hspool->high_mask >> 1);
  hspool->max_buckets = num_buckets - 1;

     
                                                                           
                                                                           
                                               
     
  hspool->sortstate = tuplesort_begin_index_hash(heap, index, hspool->high_mask, hspool->low_mask, hspool->max_buckets, maintenance_work_mem, NULL, false);

  return hspool;
}

   
                                                     
   
void
_h_spooldestroy(HSpool *hspool)
{
  tuplesort_end(hspool->sortstate);
  pfree(hspool);
}

   
                                            
   
void
_h_spool(HSpool *hspool, ItemPointer self, Datum *values, bool *isnull)
{
  tuplesort_putindextuplevalues(hspool->sortstate, hspool->index, self, values, isnull);
}

   
                                                         
                           
   
void
_h_indexbuild(HSpool *hspool, Relation heapRel)
{
  IndexTuple itup;
  int64 tups_done = 0;
#ifdef USE_ASSERT_CHECKING
  uint32 hashkey = 0;
#endif

  tuplesort_performsort(hspool->sortstate);

  while ((itup = tuplesort_getindextuple(hspool->sortstate, true)) != NULL)
  {
       
                                                                        
                                                                      
                                                                         
                                                                     
                                     
       
#ifdef USE_ASSERT_CHECKING
    uint32 lasthashkey = hashkey;

    hashkey = _hash_hashkey2bucket(_hash_get_indextuple_hashkey(itup), hspool->max_buckets, hspool->high_mask, hspool->low_mask);
    Assert(hashkey >= lasthashkey);
#endif

    _hash_doinsert(hspool->index, itup, heapRel);

    pgstat_progress_update_param(PROGRESS_CREATEIDX_TUPLES_DONE, ++tups_done);
  }
}
