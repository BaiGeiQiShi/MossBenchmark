                                                                           
   
              
   
                                   
                                       
                                                  
   
                                         
   
                                                                            
   

                   
                                                                         
                                                              
                                                    
                                                             
                                                                         
   

                                                
                                                               
                    
                                  
                                  
                                         
                                       
                    
                                                              
                                                              
                                                              
                    
                                                               

#include "postgres.h"
#include "optimizer/geqo_random.h"
#include "optimizer/geqo_recombination.h"

#if defined(OX2)

       
   
                       
   
void
ox2(PlannerInfo *root, Gene *tour1, Gene *tour2, Gene *offspring, int num_gene, City *city_table)
{
  int k, j, count, pos, select, num_positions;

                             
  for (k = 1; k <= num_gene; k++)
  {
    city_table[k].used = 0;
    city_table[k - 1].select_list = -1;
  }

                                                                     
  num_positions = geqo_randint(root, 2 * num_gene / 3, num_gene / 3);

                                      
  for (k = 0; k < num_positions; k++)
  {
    pos = geqo_randint(root, num_gene - 1, 0);
    city_table[pos].select_list = (int)tour1[pos];
    city_table[(int)tour1[pos]].used = 1;                
  }

  count = 0;
  k = 0;

                                                         
  while (count < num_positions)
  {
    if (city_table[k].select_list == -1)
    {
      j = k + 1;
      while ((city_table[j].select_list == -1) && (j < num_gene))
      {
        j++;
      }

      city_table[k].select_list = city_table[j].select_list;
      city_table[j].select_list = -1;
      count++;
    }
    else
    {
      count++;
    }
    k++;
  }

  select = 0;

  for (k = 0; k < num_gene; k++)
  {
    if (city_table[(int)tour2[k]].used)
    {
      offspring[k] = (Gene)city_table[select].select_list;
      select++;                                      
    }
    else
    {
                                                      
      offspring[k] = tour2[k];
    }
  }
}

#endif                   
