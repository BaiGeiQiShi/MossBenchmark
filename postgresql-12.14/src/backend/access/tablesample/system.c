                                                                            
   
            
                                                    
   
                                                                           
                                                                          
                                                                          
                                                        
   
                                                                               
                                                                            
                                                                            
   
   
                                                                         
                                                                        
   
                  
                                             
   
                                                                            
   

#include "postgres.h"

#include <math.h>

#include "access/relscan.h"
#include "access/tsmapi.h"
#include "catalog/pg_type.h"
#include "optimizer/optimizer.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"

                   
typedef struct
{
  uint64 cutoff;                                                     
  uint32 seed;                            
  BlockNumber nextblock;                                      
  OffsetNumber lt;                                                   
} SystemSamplerData;

static void
system_samplescangetsamplesize(PlannerInfo *root, RelOptInfo *baserel, List *paramexprs, BlockNumber *pages, double *tuples);
static void
system_initsamplescan(SampleScanState *node, int eflags);
static void
system_beginsamplescan(SampleScanState *node, Datum *params, int nparams, uint32 seed);
static BlockNumber
system_nextsampleblock(SampleScanState *node, BlockNumber nblocks);
static OffsetNumber
system_nextsampletuple(SampleScanState *node, BlockNumber blockno, OffsetNumber maxoffset);

   
                                                         
   
Datum
tsm_system_handler(PG_FUNCTION_ARGS)
{
  TsmRoutine *tsm = makeNode(TsmRoutine);

  tsm->parameterTypes = list_make1_oid(FLOAT4OID);
  tsm->repeatable_across_queries = true;
  tsm->repeatable_across_scans = true;
  tsm->SampleScanGetSampleSize = system_samplescangetsamplesize;
  tsm->InitSampleScan = system_initsamplescan;
  tsm->BeginSampleScan = system_beginsamplescan;
  tsm->NextSampleBlock = system_nextsampleblock;
  tsm->NextSampleTuple = system_nextsampletuple;
  tsm->EndSampleScan = NULL;

  PG_RETURN_POINTER(tsm);
}

   
                           
   
static void
system_samplescangetsamplesize(PlannerInfo *root, RelOptInfo *baserel, List *paramexprs, BlockNumber *pages, double *tuples)
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

                                             
  *pages = clamp_row_est(baserel->pages * samplefract);

                                                                         
  *tuples = clamp_row_est(baserel->tuples * samplefract);
}

   
                                     
   
static void
system_initsamplescan(SampleScanState *node, int eflags)
{
  node->tsm_state = palloc0(sizeof(SystemSamplerData));
}

   
                                                     
   
static void
system_beginsamplescan(SampleScanState *node, Datum *params, int nparams, uint32 seed)
{
  SystemSamplerData *sampler = (SystemSamplerData *)node->tsm_state;
  double percent = DatumGetFloat4(params[0]);
  double dcutoff;

  if (percent < 0 || percent > 100 || isnan(percent))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TABLESAMPLE_ARGUMENT), errmsg("sample percentage must be between 0 and 100")));
  }

     
                                                                            
                                                                       
                                                                
     
  dcutoff = rint(((double)PG_UINT32_MAX + 1) * percent / 100);
  sampler->cutoff = (uint64)dcutoff;
  sampler->seed = seed;
  sampler->nextblock = 0;
  sampler->lt = InvalidOffsetNumber;

     
                                                                       
                                                                           
                                                                           
                                   
     
  node->use_bulkread = (percent >= 1);
  node->use_pagemode = true;
}

   
                                
   
static BlockNumber
system_nextsampleblock(SampleScanState *node, BlockNumber nblocks)
{
  SystemSamplerData *sampler = (SystemSamplerData *)node->tsm_state;
  BlockNumber nextblock = sampler->nextblock;
  uint32 hashinput[2];

     
                                                                        
                                                                             
                                                           
                                                                          
              
     
                                                                      
     
  hashinput[1] = sampler->seed;

     
                                                                             
               
     
  for (; nextblock < nblocks; nextblock++)
  {
    uint32 hash;

    hashinput[0] = nextblock;

    hash = DatumGetUInt32(hash_any((const unsigned char *)hashinput, (int)sizeof(hashinput)));
    if (hash < sampler->cutoff)
    {
      break;
    }
  }

  if (nextblock < nblocks)
  {
                                                                          
    sampler->nextblock = nextblock + 1;
    return nextblock;
  }

                                                        
  sampler->nextblock = 0;
  return InvalidBlockNumber;
}

   
                                               
   
                                                                             
          
   
                                                                             
                                                           
   
                                                                          
                                   
   
static OffsetNumber
system_nextsampletuple(SampleScanState *node, BlockNumber blockno, OffsetNumber maxoffset)
{
  SystemSamplerData *sampler = (SystemSamplerData *)node->tsm_state;
  OffsetNumber tupoffset = sampler->lt;

                                               
  if (tupoffset == InvalidOffsetNumber)
  {
    tupoffset = FirstOffsetNumber;
  }
  else
  {
    tupoffset++;
  }

             
  if (tupoffset > maxoffset)
  {
    tupoffset = InvalidOffsetNumber;
  }

  sampler->lt = tupoffset;

  return tupoffset;
}
