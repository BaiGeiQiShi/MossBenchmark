                                                                            
   
               
                                                       
   
                                                                           
                                                                          
                                                                          
                                                        
   
                                                                           
                                                                       
                                                                            
   
   
                                                                         
                                                                        
   
                  
                                                
   
                                                                            
   

#include "postgres.h"

#include <math.h>

#include "access/tsmapi.h"
#include "catalog/pg_type.h"
#include "optimizer/optimizer.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"

                   
typedef struct
{
  uint64 cutoff;                                               
  uint32 seed;                      
  OffsetNumber lt;                                             
} BernoulliSamplerData;

static void
bernoulli_samplescangetsamplesize(PlannerInfo *root, RelOptInfo *baserel, List *paramexprs, BlockNumber *pages, double *tuples);
static void
bernoulli_initsamplescan(SampleScanState *node, int eflags);
static void
bernoulli_beginsamplescan(SampleScanState *node, Datum *params, int nparams, uint32 seed);
static OffsetNumber
bernoulli_nextsampletuple(SampleScanState *node, BlockNumber blockno, OffsetNumber maxoffset);

   
                                                            
   
Datum
tsm_bernoulli_handler(PG_FUNCTION_ARGS)
{
  TsmRoutine *tsm = makeNode(TsmRoutine);

  tsm->parameterTypes = list_make1_oid(FLOAT4OID);
  tsm->repeatable_across_queries = true;
  tsm->repeatable_across_scans = true;
  tsm->SampleScanGetSampleSize = bernoulli_samplescangetsamplesize;
  tsm->InitSampleScan = bernoulli_initsamplescan;
  tsm->BeginSampleScan = bernoulli_beginsamplescan;
  tsm->NextSampleBlock = NULL;
  tsm->NextSampleTuple = bernoulli_nextsampletuple;
  tsm->EndSampleScan = NULL;

  PG_RETURN_POINTER(tsm);
}

   
                           
   
static void
bernoulli_samplescangetsamplesize(PlannerInfo *root, RelOptInfo *baserel, List *paramexprs, BlockNumber *pages, double *tuples)
{
  Node *pctnode;
  float4 samplefract;

                                                            
  pctnode = (Node *)linitial(paramexprs);
  pctnode = estimate_expression_value(root, pctnode);

  if (IsA(pctnode, Const) && !((Const *)pctnode)->constisnull)
  {
    samplefract = DatumGetFloat4(((Const *)pctnode)->constvalue);
    if (samplefract >= 0 && samplefract <= 100 && !isnan(samplefract))
    {
      samplefract /= 100.0f;
    }
    else
    {
                                                     
      samplefract = 0.1f;
    }
  }
  else
  {
                                                                  
    samplefract = 0.1f;
  }

                                            
  *pages = baserel->pages;

  *tuples = clamp_row_est(baserel->tuples * samplefract);
}

   
                                     
   
static void
bernoulli_initsamplescan(SampleScanState *node, int eflags)
{
  node->tsm_state = palloc0(sizeof(BernoulliSamplerData));
}

   
                                                     
   
static void
bernoulli_beginsamplescan(SampleScanState *node, Datum *params, int nparams, uint32 seed)
{
  BernoulliSamplerData *sampler = (BernoulliSamplerData *)node->tsm_state;
  double percent = DatumGetFloat4(params[0]);
  double dcutoff;

  if (percent < 0 || percent > 100 || isnan(percent))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLESAMPLE_ARGUMENT), errmsg("sample percentage must be between 0 and 100")));
  }

     
                                                                            
                                                                       
                                                                
     
  dcutoff = rint(((double)PG_UINT32_MAX + 1) * percent / 100);
  sampler->cutoff = (uint64)dcutoff;
  sampler->seed = seed;
  sampler->lt = InvalidOffsetNumber;

     
                                                                            
                                                                          
                                                    
     
  node->use_bulkread = true;
  node->use_pagemode = (percent >= 25);
}

   
                                               
   
                                                                             
                                                                            
                                                                             
                                                                              
   
                                                                          
                                   
   
static OffsetNumber
bernoulli_nextsampletuple(SampleScanState *node, BlockNumber blockno, OffsetNumber maxoffset)
{
  BernoulliSamplerData *sampler = (BernoulliSamplerData *)node->tsm_state;
  OffsetNumber tupoffset = sampler->lt;
  uint32 hashinput[3];

                                            
  if (tupoffset == InvalidOffsetNumber)
  {
    tupoffset = FirstOffsetNumber;
  }
  else
  {
    tupoffset++;
  }

     
                                                                        
                                                                           
                                                               
                                                                          
              
     
                                                                      
     
  hashinput[0] = blockno;
  hashinput[2] = sampler->seed;

     
                                                                           
            
     
  for (; tupoffset <= maxoffset; tupoffset++)
  {
    uint32 hash;

    hashinput[1] = tupoffset;

    hash = DatumGetUInt32(hash_any((const unsigned char *)hashinput, (int)sizeof(hashinput)));
    if (hash < sampler->cutoff)
    {
      break;
    }
  }

  if (tupoffset > maxoffset)
  {
    tupoffset = InvalidOffsetNumber;
  }

  sampler->lt = tupoffset;

  return tupoffset;
}
