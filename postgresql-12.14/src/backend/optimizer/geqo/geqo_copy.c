                                                                           
   
               
   
                                                                         
                                                                        
   
                                          
   
                                                                            
   

                   
                                                                         
                                                              
                                                    
                                                             
                                                                         
   

                                                         

                                                               
                    
                                  
                                  
                                         
                                       
                    
                                                              
                                                              
                                                              
                    
                                                               

#include "postgres.h"
#include "optimizer/geqo_copy.h"

             
   
                               
   
   
void
geqo_copy(PlannerInfo *root, Chromosome *chromo1, Chromosome *chromo2, int string_length)
{
  int i;

  for (i = 0; i < string_length; i++)
  {
    chromo1->string[i] = chromo2->string[i];
  }

  chromo1->worth = chromo2->worth;
}
