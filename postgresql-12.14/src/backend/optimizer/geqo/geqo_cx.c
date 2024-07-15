                                                                           
   
             
   
                                   
                                          
                                  
   
                                        
   
                                                                            
   

                   
                                                                         
                                                              
                                                    
                                                             
                                                                         
   

                                                
                                                               
                    
                                  
                                  
                                         
                                       
                    
                                                              
                                                              
                                                              
                    
                                                               

#include "postgres.h"
#include "optimizer/geqo_recombination.h"
#include "optimizer/geqo_random.h"

#if defined(CX)

      
   
                    
   
int
cx(PlannerInfo *root, Gene *tour1, Gene *tour2, Gene *offspring, int num_gene, City *city_table)
{
  int i, start_pos, curr_pos;
  int count = 0;
  int num_diffs = 0;

                             
  for (i = 1; i <= num_gene; i++)
  {
    city_table[i].used = 0;
    city_table[tour2[i - 1]].tour2_position = i - 1;
    city_table[tour1[i - 1]].tour1_position = i - 1;
  }

                                             
  start_pos = geqo_randint(root, num_gene - 1, 0);

                                  
  offspring[start_pos] = tour1[start_pos];

                              
  curr_pos = start_pos;
  city_table[(int)tour1[start_pos]].used = 1;

  count++;

                    

              

  while (tour2[curr_pos] != tour1[start_pos])
  {
    city_table[(int)tour2[curr_pos]].used = 1;
    curr_pos = city_table[(int)tour2[curr_pos]].tour1_position;
    offspring[curr_pos] = tour1[curr_pos];
    count++;
  }

              

                                        
  if (count < num_gene)
  {
    for (i = 1; i <= num_gene; i++)
    {
      if (!city_table[i].used)
      {
        offspring[city_table[i].tour2_position] = tour2[(int)city_table[i].tour2_position];
        count++;
      }
    }
  }

              

                                              
  if (count < num_gene)
  {

                                                                   
    for (i = 0; i < num_gene; i++)
    {
      if (tour1[i] != offspring[i])
      {
        num_diffs++;
      }
    }
  }

  return num_diffs;
}

#endif                  
