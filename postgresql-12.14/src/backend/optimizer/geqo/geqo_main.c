                                                                           
   
               
                                                
                                          
   
                                                                         
                                                                        
   
                                          
   
                                                                            
   

                   
                                                                         
                                                              
                                                    
                                                             
                                                                         
   

                                                                         

#include "postgres.h"

#include <math.h>

#include "optimizer/geqo_misc.h"
#include "optimizer/geqo_mutation.h"
#include "optimizer/geqo_pool.h"
#include "optimizer/geqo_random.h"
#include "optimizer/geqo_selection.h"

   
                         
   
int Geqo_effort;
int Geqo_pool_size;
int Geqo_generations;
double Geqo_selection_bias;
double Geqo_seed;

static int
gimme_pool_size(int nr_rel);
static int
gimme_number_generations(int pool_size);

                                                         
#if !defined(ERX) && !defined(PMX) && !defined(CX) && !defined(PX) && !defined(OX1) && !defined(OX2)
#error "must choose one GEQO recombination mechanism in geqo.h"
#endif

   
        
                                                
                                                               
   

RelOptInfo *
geqo(PlannerInfo *root, int number_of_rels, List *initial_rels)
{
  GeqoPrivateData private;
  int generation;
  Chromosome *momma;
  Chromosome *daddy;
  Chromosome *kid;
  Pool *pool;
  int pool_size, number_generations;

#ifdef GEQO_DEBUG
  int status_interval;
#endif
  Gene *best_tour;
  RelOptInfo *best_rel;

#if defined(ERX)
  Edge *edge_table;                    
  int edge_failures = 0;
#endif
#if defined(CX) || defined(PX) || defined(OX1) || defined(OX2)
  City *city_table;                     
#endif
#if defined(CX)
  int cycle_diffs = 0;
  int mutations = 0;
#endif

                                  
  root->join_search_private = (void *)&private;
  private.initial_rels = initial_rels;

                                           
  geqo_set_seed(root, Geqo_seed);

                         
  pool_size = gimme_pool_size(number_of_rels);
  number_generations = gimme_number_generations(pool_size);
#ifdef GEQO_DEBUG
  status_interval = 10;
#endif

                                    
  pool = alloc_pool(root, pool_size, number_of_rels);

                                         
  random_init_pool(root, pool);

                                                           
  sort_pool(root, pool);                                              
                                                                  
                                                                     

#ifdef GEQO_DEBUG
  elog(DEBUG1, "GEQO selected %d pool entries, best %.2f, worst %.2f", pool_size, pool->data[0].worth, pool->data[pool_size - 1].worth);
#endif

                                                  
  momma = alloc_chromo(root, pool->string_length);
  daddy = alloc_chromo(root, pool->string_length);

#if defined(ERX)
#ifdef GEQO_DEBUG
  elog(DEBUG2, "using edge recombination crossover [ERX]");
#endif
                                  
  edge_table = alloc_edge_table(root, pool->string_length);
#elif defined(PMX)
#ifdef GEQO_DEBUG
  elog(DEBUG2, "using partially matched crossover [PMX]");
#endif
                                      
  kid = alloc_chromo(root, pool->string_length);
#elif defined(CX)
#ifdef GEQO_DEBUG
  elog(DEBUG2, "using cycle crossover [CX]");
#endif
                                  
  kid = alloc_chromo(root, pool->string_length);
  city_table = alloc_city_table(root, pool->string_length);
#elif defined(PX)
#ifdef GEQO_DEBUG
  elog(DEBUG2, "using position crossover [PX]");
#endif
                                  
  kid = alloc_chromo(root, pool->string_length);
  city_table = alloc_city_table(root, pool->string_length);
#elif defined(OX1)
#ifdef GEQO_DEBUG
  elog(DEBUG2, "using order crossover [OX1]");
#endif
                                  
  kid = alloc_chromo(root, pool->string_length);
  city_table = alloc_city_table(root, pool->string_length);
#elif defined(OX2)
#ifdef GEQO_DEBUG
  elog(DEBUG2, "using order crossover [OX2]");
#endif
                                  
  kid = alloc_chromo(root, pool->string_length);
  city_table = alloc_city_table(root, pool->string_length);
#endif

                          
                              

  for (generation = 0; generation < number_generations; generation++)
  {
                                               
    geqo_selection(root, momma, daddy, pool, Geqo_selection_bias);

#if defined(ERX)
                                      
    gimme_edge_table(root, momma->string, daddy->string, pool->string_length, edge_table);

    kid = momma;

                                       
    edge_failures += gimme_tour(root, edge_table, kid->string, pool->string_length);
#elif defined(PMX)
                                     
    pmx(root, momma->string, daddy->string, kid->string, pool->string_length);
#elif defined(CX)
                         
    cycle_diffs = cx(root, momma->string, daddy->string, kid->string, pool->string_length, city_table);
                          
    if (cycle_diffs == 0)
    {
      mutations++;
      geqo_mutation(root, kid->string, pool->string_length);
    }
#elif defined(PX)
                            
    px(root, momma->string, daddy->string, kid->string, pool->string_length, city_table);
#elif defined(OX1)
                         
    ox1(root, momma->string, daddy->string, kid->string, pool->string_length, city_table);
#elif defined(OX2)
                         
    ox2(root, momma->string, daddy->string, kid->string, pool->string_length, city_table);
#endif

                          
    kid->worth = geqo_eval(root, kid->string, pool->string_length);

                                                                         
    spread_chromo(root, kid, pool);

#ifdef GEQO_DEBUG
    if (status_interval && !(generation % status_interval))
    {
      print_gen(stdout, pool, generation);
    }
#endif
  }

#if defined(ERX)
#if defined(GEQO_DEBUG)
  if (edge_failures != 0)
  {
    elog(LOG, "[GEQO] failures: %d, average: %d", edge_failures, (int)number_generations / edge_failures);
  }
  else
  {
    elog(LOG, "[GEQO] no edge failures detected");
  }
#else
                                                                       
  (void)edge_failures;
#endif
#endif

#if defined(CX) && defined(GEQO_DEBUG)
  if (mutations != 0)
  {
    elog(LOG, "[GEQO] mutations: %d, generations: %d", mutations, number_generations);
  }
  else
  {
    elog(LOG, "[GEQO] no mutations processed");
  }
#endif

#ifdef GEQO_DEBUG
  print_pool(stdout, pool, 0, pool_size - 1);
#endif

#ifdef GEQO_DEBUG
  elog(DEBUG1, "GEQO best is %.2f after %d generations", pool->data[0].worth, number_generations);
#endif

     
                                                                         
                                              
     
  best_tour = (Gene *)pool->data[0].string;

  best_rel = gimme_tree(root, best_tour, pool->string_length);

  if (best_rel == NULL)
  {
    elog(ERROR, "geqo failed to make a valid plan");
  }

                                
#ifdef NOT_USED
  print_plan(best_plan, root);
#endif

                             
  free_chromo(root, momma);
  free_chromo(root, daddy);

#if defined(ERX)
  free_edge_table(root, edge_table);
#elif defined(PMX)
  free_chromo(root, kid);
#elif defined(CX)
  free_chromo(root, kid);
  free_city_table(root, city_table);
#elif defined(PX)
  free_chromo(root, kid);
  free_city_table(root, city_table);
#elif defined(OX1)
  free_chromo(root, kid);
  free_city_table(root, city_table);
#elif defined(OX2)
  free_chromo(root, kid);
  free_city_table(root, city_table);
#endif

  free_pool(root, pool);

                                                     
  root->join_search_private = NULL;

  return best_rel;
}

   
                                                        
   
                                                                     
                                                         
   
static int
gimme_pool_size(int nr_rel)
{
  double size;
  int minsize;
  int maxsize;

                                                                           
  if (Geqo_pool_size >= 2)
  {
    return Geqo_pool_size;
  }

  size = pow(2.0, nr_rel + 1.0);

  maxsize = 50 * Geqo_effort;                            
  if (size > maxsize)
  {
    return maxsize;
  }

  minsize = 10 * Geqo_effort;                            
  if (size < minsize)
  {
    return minsize;
  }

  return (int)ceil(size);
}

   
                                                                    
   
                                                                   
                                                                 
                                       
   
static int
gimme_number_generations(int pool_size)
{
  if (Geqo_generations > 0)
  {
    return Geqo_generations;
  }

  return pool_size;
}
