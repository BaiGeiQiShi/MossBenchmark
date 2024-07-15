                                                                             
   
                      
                                              
   
                                                                
   
                  
                                                         
   
                                                                             
   
#include "postgres.h"

#include "fmgr.h"
#include "lib/bloomfilter.h"
#include "miscadmin.h"

PG_MODULE_MAGIC;

                                                            
#define MAX_ELEMENT_BYTES 21
                                                 
#define FPOSITIVE_THRESHOLD 0.01

   
                                                                  
   
static void
populate_with_dummy_strings(bloom_filter *filter, int64 nelements)
{
  char element[MAX_ELEMENT_BYTES];
  int64 i;

  for (i = 0; i < nelements; i++)
  {
    CHECK_FOR_INTERRUPTS();

    snprintf(element, sizeof(element), "i" INT64_FORMAT, i);
    bloom_add_element(filter, (unsigned char *)element, strlen(element));
  }
}

   
                                                                               
                                                                          
                              
   
static int64
nfalsepos_for_missing_strings(bloom_filter *filter, int64 nelements)
{
  char element[MAX_ELEMENT_BYTES];
  int64 nfalsepos = 0;
  int64 i;

  for (i = 0; i < nelements; i++)
  {
    CHECK_FOR_INTERRUPTS();

    snprintf(element, sizeof(element), "M" INT64_FORMAT, i);
    if (!bloom_lacks_element(filter, (unsigned char *)element, strlen(element)))
    {
      nfalsepos++;
    }
  }

  return nfalsepos;
}

static void
create_and_test_bloom(int power, int64 nelements, int callerseed)
{
  int bloom_work_mem;
  uint64 seed;
  int64 nfalsepos;
  bloom_filter *filter;

  bloom_work_mem = (1L << power) / 8L / 1024L;

  elog(DEBUG1, "bloom_work_mem (KB): %d", bloom_work_mem);

     
                                                                     
                                                                           
                                                                         
                                                              
     
  seed = callerseed < 0 ? random() % PG_INT32_MAX : callerseed;

                                                                           
  filter = bloom_create(nelements, bloom_work_mem, seed);
  populate_with_dummy_strings(filter, nelements);
  nfalsepos = nfalsepos_for_missing_strings(filter, nelements);

  ereport((nfalsepos > nelements * FPOSITIVE_THRESHOLD) ? WARNING : DEBUG1, (errmsg_internal("seed: " UINT64_FORMAT " false positives: " INT64_FORMAT " (%.6f%%) bitset %.2f%% set", seed, nfalsepos, (double)nfalsepos / nelements, 100.0 * bloom_prop_bits_set(filter))));

  bloom_free(filter);
}

PG_FUNCTION_INFO_V1(test_bloomfilter);

   
                                                  
   
                                                                
   
                                        
   
Datum
test_bloomfilter(PG_FUNCTION_ARGS)
{
  int power = PG_GETARG_INT32(0);
  int64 nelements = PG_GETARG_INT64(1);
  int seed = PG_GETARG_INT32(2);
  int tests = PG_GETARG_INT32(3);
  int i;

  if (power < 23 || power > 32)
  {
    elog(ERROR, "power argument must be between 23 and 32 inclusive");
  }

  if (tests <= 0)
  {
    elog(ERROR, "invalid number of tests: %d", tests);
  }

  if (nelements < 0)
  {
    elog(ERROR, "invalid number of elements: %d", tests);
  }

  for (i = 0; i < tests; i++)
  {
    elog(DEBUG1, "beginning test #%d...", i + 1);

    create_and_test_bloom(power, nelements, seed);
  }

  PG_RETURN_VOID();
}
