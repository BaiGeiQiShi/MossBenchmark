                                                                           
   
             
   
                                      
                                      
                                                   
   
                                        
   
                                                                            
   

                   
                                                                         
                                                              
                                                    
                                                             
                                                                         
   

                                                
                                                               
                    
                                  
                                  
                                         
                                       
                    
                                                              
                                                              
                                                              
                    
                                                               

#include "postgres.h"
#include "optimizer/geqo_random.h"
#include "optimizer/geqo_recombination.h"

#if defined(PX)

      
   
                       
   
void
px(PlannerInfo *root, Gene *tour1, Gene *tour2, Gene *offspring, int num_gene, City *city_table)
{
  int num_positions;
  int i, pos, tour2_index, offspring_index;

                             
  for (i = 1; i <= num_gene; i++)
  {
    city_table[i].used = 0;
  }

                                                                           
  num_positions = geqo_randint(root, 2 * num_gene / 3, num_gene / 3);

                              
  for (i = 0; i < num_positions; i++)
  {
    pos = geqo_randint(root, num_gene - 1, 0);

    offspring[pos] = tour1[pos];                                        
    city_table[(int)tour1[pos]].used = 1;                     
  }

  tour2_index = 0;
  offspring_index = 0;

                    

  while (offspring_index < num_gene)
  {

                                           
    if (!city_table[(int)tour1[offspring_index]].used)
    {

                                       
      if (!city_table[(int)tour2[tour2_index]].used)
      {

                                
        offspring[offspring_index] = tour2[tour2_index];

        tour2_index++;
        offspring_index++;
      }
      else
      {                                       
        tour2_index++;
      }
    }
    else
    {                                           
      offspring_index++;
    }
  }
}

#endif                  
