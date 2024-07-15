                                                                           
   
               
                                       
   
                                                                         
                                                                        
   
                                          
   
                                                                            
   

                   
                                                                         
                                                              
                                                    
                                                             
                                                                         
   

                                                                         

#include "postgres.h"

#include <float.h>
#include <limits.h>
#include <math.h>

#include "optimizer/geqo_copy.h"
#include "optimizer/geqo_pool.h"
#include "optimizer/geqo_recombination.h"

static int
compare(const void *arg1, const void *arg2);

   
              
                                 
   
Pool *
alloc_pool(PlannerInfo *root, int pool_size, int string_length)
{
  Pool *new_pool;
  Chromosome *chromo;
  int i;

            
  new_pool = (Pool *)palloc(sizeof(Pool));
  new_pool->size = (int)pool_size;
  new_pool->string_length = (int)string_length;

                      
  new_pool->data = (Chromosome *)palloc(pool_size * sizeof(Chromosome));

                
  chromo = (Chromosome *)new_pool->data;                            
  for (i = 0; i < pool_size; i++)
  {
    chromo[i].string = palloc((string_length + 1) * sizeof(Gene));
  }

  return new_pool;
}

   
             
                                   
   
void
free_pool(PlannerInfo *root, Pool *pool)
{
  Chromosome *chromo;
  int i;

                
  chromo = (Chromosome *)pool->data;                            
  for (i = 0; i < pool->size; i++)
  {
    pfree(chromo[i].string);
  }

                      
  pfree(pool->data);

            
  pfree(pool);
}

   
                    
                            
   
void
random_init_pool(PlannerInfo *root, Pool *pool)
{
  Chromosome *chromo = (Chromosome *)pool->data;
  int i;
  int bad = 0;

     
                                                                          
                                                                   
     
                                                                          
                                                                        
                                              
     
  i = 0;
  while (i < pool->size)
  {
    init_tour(root, chromo[i].string, pool->string_length);
    pool->data[i].worth = geqo_eval(root, chromo[i].string, pool->string_length);
    if (pool->data[i].worth < DBL_MAX)
    {
      i++;
    }
    else
    {
      bad++;
      if (i == 0 && bad >= 10000)
      {
        elog(ERROR, "geqo failed to make a valid plan");
      }
    }
  }

#ifdef GEQO_DEBUG
  if (bad > 0)
  {
    elog(DEBUG1, "%d invalid tours found while selecting %d pool entries", bad, pool->size);
  }
#endif
}

   
             
                                                                  
   
                                                                  
   
void
sort_pool(PlannerInfo *root, Pool *pool)
{
  qsort(pool->data, pool->size, sizeof(Chromosome), compare);
}

   
           
                                            
   
static int
compare(const void *arg1, const void *arg2)
{
  const Chromosome *chromo1 = (const Chromosome *)arg1;
  const Chromosome *chromo2 = (const Chromosome *)arg2;

  if (chromo1->worth == chromo2->worth)
  {
    return 0;
  }
  else if (chromo1->worth > chromo2->worth)
  {
    return 1;
  }
  else
  {
    return -1;
  }
}

                
                                             
   
Chromosome *
alloc_chromo(PlannerInfo *root, int string_length)
{
  Chromosome *chromo;

  chromo = (Chromosome *)palloc(sizeof(Chromosome));
  chromo->string = (Gene *)palloc((string_length + 1) * sizeof(Gene));

  return chromo;
}

               
                                               
   
void
free_chromo(PlannerInfo *root, Chromosome *chromo)
{
  pfree(chromo->string);
  pfree(chromo);
}

                 
                                                                          
                                            
   
void
spread_chromo(PlannerInfo *root, Chromosome *chromo, Pool *pool)
{
  int top, mid, bot;
  int i, index;
  Chromosome swap_chromo, tmp_chromo;

                                            
  if (chromo->worth > pool->data[pool->size - 1].worth)
  {
    return;
  }

                                                              

  top = 0;
  mid = pool->size / 2;
  bot = pool->size - 1;
  index = -1;

  while (index == -1)
  {
                                           

    if (chromo->worth <= pool->data[top].worth)
    {
      index = top;
    }
    else if (chromo->worth == pool->data[mid].worth)
    {
      index = mid;
    }
    else if (chromo->worth == pool->data[bot].worth)
    {
      index = bot;
    }
    else if (bot - top <= 1)
    {
      index = bot;
    }

       
                                                                          
                       
       

    else if (chromo->worth < pool->data[mid].worth)
    {
      bot = mid;
      mid = top + ((bot - top) / 2);
    }
    else
    {                                              
      top = mid;
      mid = top + ((bot - top) / 2);
    }
  }                

                                    

     
                                                                             
     

     
                                                                        
     

  geqo_copy(root, &pool->data[pool->size - 1], chromo, pool->string_length);

  swap_chromo.string = pool->data[pool->size - 1].string;
  swap_chromo.worth = pool->data[pool->size - 1].worth;

  for (i = index; i < pool->size; i++)
  {
    tmp_chromo.string = pool->data[i].string;
    tmp_chromo.worth = pool->data[i].worth;

    pool->data[i].string = swap_chromo.string;
    pool->data[i].worth = swap_chromo.worth;

    swap_chromo.string = tmp_chromo.string;
    swap_chromo.worth = tmp_chromo.worth;
  }
}
