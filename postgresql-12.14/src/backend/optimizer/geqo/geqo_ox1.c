                                                                           
   
              
   
                                   
                                    
                                  
   
                                         
   
                                                                            
   

                   
                                                                         
                                                              
                                                    
                                                             
                                                                         
   

                                                
                                                               
                    
                                  
                                  
                                         
                                       
                    
                                                              
                                                              
                                                              
                    
                                                               

#include "postgres.h"
#include "optimizer/geqo_random.h"
#include "optimizer/geqo_recombination.h"

#if defined(OX1)

       
   
                       
   
void
ox1(PlannerInfo *root, Gene *tour1, Gene *tour2, Gene *offspring, int num_gene, City *city_table)
{
  int left, right, k, p, temp;

                             
  for (k = 1; k <= num_gene; k++)
  {
    city_table[k].used = 0;
  }

                                         
  left = geqo_randint(root, num_gene - 1, 0);
  right = geqo_randint(root, num_gene - 1, 0);

  if (left > right)
  {
    temp = left;
    left = right;
    right = temp;
  }

                                            
  for (k = left; k <= right; k++)
  {
    offspring[k] = tour1[k];
    city_table[(int)tour1[k]].used = 1;
  }

  k = (right + 1) % num_gene;                           
  p = k;                                            

                                          
  while (k != left)
  {
    if (!city_table[(int)tour2[p]].used)
    {
      offspring[k] = tour2[p];
      k = (k + 1) % num_gene;
      city_table[(int)tour2[p]].used = 1;
    }
    p = (p + 1) % num_gene;                            
  }
}

#endif                   
