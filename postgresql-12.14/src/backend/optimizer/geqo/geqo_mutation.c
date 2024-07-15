                                                                           
   
                   
   
                          
   
                                              
   
                                                                            
   

                   
                                                                         
                                                              
                                                    
                                                             
                                                                         
   

                                    
                                                               
                    
                                  
                                  
                                         
                                       
                    
                                                              
                                                              
                                                              
                    
                                                               

#include "postgres.h"
#include "optimizer/geqo_mutation.h"
#include "optimizer/geqo_random.h"

#if defined(CX)                                     

void
geqo_mutation(PlannerInfo *root, Gene *tour, int num_gene)
{
  int swap1;
  int swap2;
  int num_swaps = geqo_randint(root, num_gene / 3, 0);
  Gene temp;

  while (num_swaps > 0)
  {
    swap1 = geqo_randint(root, num_gene - 1, 0);
    swap2 = geqo_randint(root, num_gene - 1, 0);

    while (swap1 == swap2)
    {
      swap2 = geqo_randint(root, num_gene - 1, 0);
    }

    temp = tour[swap1];
    tour[swap1] = tour[swap2];
    tour[swap2] = temp;

    num_swaps -= 1;
  }
}

#endif                  
