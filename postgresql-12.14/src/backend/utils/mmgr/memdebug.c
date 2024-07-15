                                                                            
   
              
                                                                          
                                                    
   
   
                                                                         
                                                                        
   
                                     
   
   
                               
   
                                                                           
                                                                           
   
                                  
   
                                                                         
                                                                       
                                                                        
                                                                           
                                                                       
                                                                          
                                                                          
                                                                          
                                                                          
   
   
                                                    
   
                                                                            
                                                                          
                          
   
                                                                             
                                                                            
                                                                           
                                                                            
                                                                             
                                                                            
                                                  
   
                                                                          
                                                                         
                                                                             
                                                                           
   
                                                                
   
                                                                            
   

#include "postgres.h"

#include "utils/memdebug.h"

#ifdef RANDOMIZE_ALLOCATED_MEMORY

   
                                                                              
                                                                            
                                                                            
                                                                    
   
                                                                             
                                                                          
                                                                         
                                                                          
                                     
   
void
randomize_mem(char *ptr, size_t size)
{
  static int save_ctr = 1;
  size_t remaining = size;
  int ctr;

  ctr = save_ctr;
  VALGRIND_MAKE_MEM_UNDEFINED(ptr, size);
  while (remaining-- > 0)
  {
    *ptr++ = ctr;
    if (++ctr > 251)
    {
      ctr = 1;
    }
  }
  VALGRIND_MAKE_MEM_UNDEFINED(ptr - size, size);
  save_ctr = ctr;
}

#endif                                 
