                                                                           
   
                        
                                  
   
                                                   
   
                                                                            
   

                   
                                                                         
                                                              
                                                    
                                                             
                                                                         
   

                                                                         

#include "postgres.h"

#include "optimizer/geqo_random.h"
#include "optimizer/geqo_recombination.h"

   
             
   
                                                         
                                                  
   
void
init_tour(PlannerInfo *root, Gene *tour, int num_gene)
{
  int i, j;

     
                                                                            
                                                                       
                                                                           
                                                                         
                                                                        
                                                                            
                                         
     
  if (num_gene > 0)
  {
    tour[0] = (Gene)1;
  }

  for (i = 1; i < num_gene; i++)
  {
    j = geqo_randint(root, i, 0);
                                                                  
    if (i != j)
    {
      tour[i] = tour[j];
    }
    tour[j] = (Gene)(i + 1);
  }
}

                                                        
#if defined(CX) || defined(PX) || defined(OX1) || defined(OX2)

                    
   
                                   
   
City *
alloc_city_table(PlannerInfo *root, int num_gene)
{
  City *city_table;

     
                                                                          
                                  
     
  city_table = (City *)palloc((num_gene + 1) * sizeof(City));

  return city_table;
}

                   
   
                                     
   
void
free_city_table(PlannerInfo *root, City *city_table)
{
  pfree(city_table);
}

#endif                             
