                                                                            
   
                    
                                                             
   
                                                                         
                                                                        
   
                                               
   
                                                                            
   

                   
                                                                         
                                                              
                                                    
                                                             
                                                                         
   

                                                         

                                                               
                    
                                  
                                  
                                         
                                       
                    
                                                              
                                                              
                                                              
                    
                                                               

#include "postgres.h"

#include <math.h>

#include "optimizer/geqo_copy.h"
#include "optimizer/geqo_random.h"
#include "optimizer/geqo_selection.h"

static int
linear_rand(PlannerInfo *root, int max, double bias);

   
                  
                                                     
                                                      
   
void
geqo_selection(PlannerInfo *root, Chromosome *momma, Chromosome *daddy, Pool *pool, double bias)
{
  int first, second;

  first = linear_rand(root, pool->size, bias);
  second = linear_rand(root, pool->size, bias);

     
                                                                          
                         
     
                                                                    
                                                                           
                      
     
  if (pool->size > 1)
  {
    while (first == second)
    {
      second = linear_rand(root, pool->size, bias);
    }
  }

  geqo_copy(root, momma, &pool->data[first], pool->string_length);
  geqo_copy(root, daddy, &pool->data[second], pool->string_length);
}

   
               
                                                             
                             
   
                                                
   
                                                                      
                                                          
   
static int
linear_rand(PlannerInfo *root, int pool_size, double bias)
{
  double index;                                   
  double max = (double)pool_size;

     
                                                                          
                                                                     
                                                                            
                                                                        
                                                   
     
  do
  {
    double sqrtval;

    sqrtval = (bias * bias) - 4.0 * (bias - 1.0) * geqo_rand(root);
    if (sqrtval > 0.0)
    {
      sqrtval = sqrt(sqrtval);
    }
    index = max * (bias - sqrtval) / 2.0 / (bias - 1.0);
  } while (index < 0.0 || index >= max);

  return (int)index;
}
