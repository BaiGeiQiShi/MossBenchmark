                                                                            
   
              
                                       
   
                                                                         
                                                                        
   
   
                  
                                       
   
                                                                            
   

#include "postgres.h"

#include <math.h>

#include "utils/sampling.h"

   
                                                                    
   
                                                                          
                                                                 
                                                          
                                                               
                                               
   
                                                                       
                                                                      
              
   
void
BlockSampler_Init(BlockSampler bs, BlockNumber nblocks, int samplesize, long randseed)
{
  bs->N = nblocks;                          

     
                                                                             
                                                              
     
  bs->n = samplesize;
  bs->t = 0;                            
  bs->m = 0;                             

  sampler_random_init_state(randseed, bs->randstate);
}

bool
BlockSampler_HasMore(BlockSampler bs)
{
  return (bs->t < bs->N) && (bs->m < bs->n);
}

BlockNumber
BlockSampler_Next(BlockSampler bs)
{
  BlockNumber K = bs->N - bs->t;                       
  int k = bs->n - bs->m;                                     
  double p;                                                     
  double V;                                  

  Assert(BlockSampler_HasMore(bs));                            

  if ((BlockNumber)k >= K)
  {
                           
    bs->m++;
    return bs->t++;
  }

               
                                                                   
                                                                    
                                                                    
                                                                       
                                                                         
                                                                         
                                                                      
                                                                            
                                                                            
                                                                     
                                               
     
                                                                     
                                                                    
                                                                   
                                                                      
                                                                         
                                                                           
               
     
  V = sampler_random_fract(bs->randstate);
  p = 1.0 - (double)k / (double)K;
  while (V < p)
  {
              
    bs->t++;
    K--;                      

                                                          
    p *= 1.0 - (double)k / (double)K;
  }

              
  bs->m++;
  return bs->t++;
}

   
                                                                      
                                                                     
                                                                      
                                                                       
                                                                            
                                                                            
   
                                                                
   
                                                                          
                                                                      
              
   
void
reservoir_init_selection_state(ReservoirState rs, int n)
{
     
                                                                           
                                                          
     
  sampler_random_init_state(random(), rs->randstate);

                                                                      
  rs->W = exp(-log(sampler_random_fract(rs->randstate)) / n);
}

double
reservoir_get_next_S(ReservoirState rs, double t, int n)
{
  double S;

                                                        
  if (t <= (22.0 * n))
  {
                                                                   
    double V, quot;

    V = sampler_random_fract(rs->randstate);                 
    S = 0;
    t += 1;
                                                               
    quot = (t - (double)n) / t;
                                     
    while (quot > V)
    {
      S += 1;
      t += 1;
      quot *= (t - (double)n) / t;
    }
  }
  else
  {
                               
    double W = rs->W;
    double term = t - (double)n + 1;

    for (;;)
    {
      double numer, numer_lim, denom;
      double U, X, lhs, rhs, y, tmp;

                            
      U = sampler_random_fract(rs->randstate);
      X = t * (W - 1.0);
      S = floor(X);                                       
                                                          
      tmp = (t + 1) / term;
      lhs = exp(log(((U * tmp * tmp) * (term + S)) / (t + X)) / n);
      rhs = (((t + X) / (term + S)) * term) / t;
      if (lhs <= rhs)
      {
        W = rhs / lhs;
        break;
      }
                                   
      y = (((U * (t + 1)) / term) * (t + S + 1)) / (t + X);
      if ((double)n < S)
      {
        denom = t;
        numer_lim = term + S;
      }
      else
      {
        denom = t - (double)n + S;
        numer_lim = t + 1;
      }
      for (numer = t + S; numer >= numer_lim; numer -= 1)
      {
        y *= numer / denom;
        denom -= 1;
      }
      W = exp(-log(sampler_random_fract(rs->randstate)) / n);                            
      if (exp(log(y) / n) <= (t + X) / t)
      {
        break;
      }
    }
    rs->W = W;
  }
  return S;
}

             
                                            
             
   
void
sampler_random_init_state(long seed, SamplerRandomState randstate)
{
  randstate[0] = 0x330e;                                                
  randstate[1] = (unsigned short)seed;
  randstate[2] = (unsigned short)(seed >> 16);
}

                                                              
double
sampler_random_fract(SamplerRandomState randstate)
{
  double res;

                                                                      
  do
  {
    res = pg_erand48(randstate);
  } while (res == 0.0);
  return res;
}

   
                                               
   
                                                                          
                                                                            
                                                                             
                                                                 
   
static ReservoirStateData oldrs;

double
anl_random_fract(void)
{
                                        
  if (oldrs.randstate[0] == 0)
  {
    sampler_random_init_state(random(), oldrs.randstate);
  }

                                     
  return sampler_random_fract(oldrs.randstate);
}

double
anl_init_selection_state(int n)
{
                                        
  if (oldrs.randstate[0] == 0)
  {
    sampler_random_init_state(random(), oldrs.randstate);
  }

                                                                      
  return exp(-log(sampler_random_fract(oldrs.randstate)) / n);
}

double
anl_get_next_S(double t, int n, double *stateptr)
{
  double result;

  oldrs.W = *stateptr;
  result = reservoir_get_next_S(&oldrs, t, n);
  *stateptr = oldrs.W;
  return result;
}
