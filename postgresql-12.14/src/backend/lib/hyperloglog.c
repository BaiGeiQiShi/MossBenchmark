                                                                            
   
                 
                                       
   
                                                                         
   
                                                                             
                                                                               
                                                                             
                                                                             
                                                                        
   
                                                                          
             
   
                                                                            
   
                  
                                   
   
                                                                            
   

   
                                                             
   
                                                                                
                                                                            
                                                                              
                                                                               
                                                                              
                                                            
   
                                                                              
                                                       
   
                                                                              
                                                                            
                                                                               
                                                                          
                                                                           
                                                                                
                    
   

#include "postgres.h"

#include <math.h>

#include "lib/hyperloglog.h"

#define POW_2_32 (4294967296.0)
#define NEG_POW_2_32 (-4294967296.0)

static inline uint8
rho(uint32 x, uint8 b);

   
                                                    
   
                                                                            
                                       
   
void
initHyperLogLog(hyperLogLogState *cState, uint8 bwidth)
{
  double alpha;

  if (bwidth < 4 || bwidth > 16)
  {
    elog(ERROR, "bit width must be between 4 and 16 inclusive");
  }

  cState->registerWidth = bwidth;
  cState->nRegisters = (Size)1 << bwidth;
  cState->arrSize = sizeof(uint8) * cState->nRegisters + 1;

     
                                                                            
                                                              
     
  cState->hashesArr = palloc0(cState->arrSize);

     
                                                                          
                                                                            
                                                                     
                             
     
  switch (cState->nRegisters)
  {
  case 16:
    alpha = 0.673;
    break;
  case 32:
    alpha = 0.697;
    break;
  case 64:
    alpha = 0.709;
    break;
  default:
    alpha = 0.7213 / (1.0 + 1.079 / cState->nRegisters);
  }

     
                                                                        
                
     
  cState->alphaMM = alpha * cState->nRegisters * cState->nRegisters;
}

   
                                                     
   
                                                                        
                                                                         
                                               
   
                       
   
                                                                     
                                                                        
                                           
   
                                                                       
                                                    
   
void
initHyperLogLogError(hyperLogLogState *cState, double error)
{
  uint8 bwidth = 4;

  while (bwidth < 16)
  {
    double m = (Size)1 << bwidth;

    if (1.04 / sqrt(m) < error)
    {
      break;
    }
    bwidth++;
  }

  initHyperLogLog(cState, bwidth);
}

   
                                
   
                                                                            
                         
   
void
freeHyperLogLog(hyperLogLogState *cState)
{
  Assert(cState->hashesArr != NULL);
  pfree(cState->hashesArr);
}

   
                                                             
   
                                                                                
                                                                               
                                                                         
                                                                                
             
   
void
addHyperLogLog(hyperLogLogState *cState, uint32 hash)
{
  uint8 count;
  uint32 index;

                                                                    
  index = hash >> (BITS_PER_BYTE * sizeof(uint32) - cState->registerWidth);

                                                                       
  count = rho(hash << cState->registerWidth, BITS_PER_BYTE * sizeof(uint32) - cState->registerWidth);

  cState->hashesArr[index] = Max(count, cState->hashesArr[index]);
}

   
                                                         
   
double
estimateHyperLogLog(hyperLogLogState *cState)
{
  double result;
  double sum = 0.0;
  int i;

  for (i = 0; i < cState->nRegisters; i++)
  {
    sum += 1.0 / pow(2.0, cState->hashesArr[i]);
  }

                                                                             
  result = cState->alphaMM / sum;

  if (result <= (5.0 / 2.0) * cState->nRegisters)
  {
                                
    int zero_count = 0;

    for (i = 0; i < cState->nRegisters; i++)
    {
      if (cState->hashesArr[i] == 0)
      {
        zero_count++;
      }
    }

    if (zero_count != 0)
    {
      result = cState->nRegisters * log((double)cState->nRegisters / zero_count);
    }
  }
  else if (result > (1.0 / 30.0) * POW_2_32)
  {
                                
    result = NEG_POW_2_32 * log(1.0 - (result / POW_2_32));
  }

  return result;
}

   
                                
   
                                                                              
                                                                               
         
   
                                                 
   
                                     
                                     
                                         
   
                                                            
   
                                                        
   
static inline uint8
rho(uint32 x, uint8 b)
{
  uint8 j = 1;

  while (j <= b && !(x & 0x80000000))
  {
    j++;
    x <<= 1;
  }

  return j;
}
