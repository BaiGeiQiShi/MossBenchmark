                                                                            
   
             
   
                                                                       
                                                                    
                                                                         
                                                                        
                                                                        
   
                                                               
                                                                         
                                                                        
                                                                        
                                                                      
   
                                                                         
   
                                                
                        
   
                                                                       
                                                                  
                                      
   
                                                                     
                                                                        
                                                
   
                  
                        
   
                                                                            
   

#include "c.h"

#include <math.h>

                                         
#define RAND48_MULT UINT64CONST(0x0005deece66d)
#define RAND48_ADD UINT64CONST(0x000b)

                                                                               
#define RAND48_SEED_0 (0x330e)
#define RAND48_SEED_1 (0xabcd)
#define RAND48_SEED_2 (0x1234)

static unsigned short _rand48_seed[3] = {RAND48_SEED_0, RAND48_SEED_1, RAND48_SEED_2};

   
                                                                           
   
                                                                            
                                                                  
   
static uint64
_dorand48(unsigned short xseed[3])
{
     
                                                                             
     
  uint64 in;
  uint64 out;

  in = (uint64)xseed[2] << 32 | (uint64)xseed[1] << 16 | (uint64)xseed[0];

  out = in * RAND48_MULT + RAND48_ADD;

  xseed[0] = out & 0xFFFF;
  xseed[1] = (out >> 16) & 0xFFFF;
  xseed[2] = (out >> 32) & 0xFFFF;

  return out;
}

   
                                                                       
                                                                  
   
double
pg_erand48(unsigned short xseed[3])
{
  uint64 x = _dorand48(xseed);

  return ldexp((double)(x & UINT64CONST(0xFFFFFFFFFFFF)), -48);
}

   
                                                                       
                                                                 
   
long
pg_lrand48(void)
{
  uint64 x = _dorand48(_rand48_seed);

  return (x >> 17) & UINT64CONST(0x7FFFFFFF);
}

   
                                                                        
                                                                     
   
long
pg_jrand48(unsigned short xseed[3])
{
  uint64 x = _dorand48(xseed);

  return (int32)((x >> 16) & UINT64CONST(0xFFFFFFFF));
}

   
                                                       
   
                                                                          
                                                                       
                                                                          
                                                                   
   
                                                                     
                                                                         
   
void
pg_srand48(long seed)
{
  _rand48_seed[0] = RAND48_SEED_0;
  _rand48_seed[1] = (unsigned short)seed;
  _rand48_seed[2] = (unsigned short)(seed >> 16);
}
