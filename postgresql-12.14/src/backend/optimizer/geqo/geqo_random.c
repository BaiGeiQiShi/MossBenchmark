                                                                           
   
                 
                              
   
                                                                         
                                                                        
   
                                            
   
                                                                            
   

#include "postgres.h"

#include "optimizer/geqo_random.h"

void
geqo_set_seed(PlannerInfo *root, double seed)
{
  GeqoPrivateData *private = (GeqoPrivateData *)root->join_search_private;

     
                                                                             
                        
     
  memset(private->random_state, 0, sizeof(private->random_state));
  memcpy(private->random_state, &seed, Min(sizeof(private->random_state), sizeof(seed)));
}

double
geqo_rand(PlannerInfo *root)
{
  GeqoPrivateData *private = (GeqoPrivateData *)root->join_search_private;

  return pg_erand48(private->random_state);
}
