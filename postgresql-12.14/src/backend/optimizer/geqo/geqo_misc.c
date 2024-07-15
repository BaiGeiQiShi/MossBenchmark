                                                                           
   
               
                                     
   
                                                                         
                                                                        
   
                                          
   
                                                                            
   

                   
                                                                         
                                                              
                                                    
                                                             
                                                                         
   

#include "postgres.h"

#include "optimizer/geqo_misc.h"

#ifdef GEQO_DEBUG

   
            
   
static double
avg_pool(Pool *pool)
{
  int i;
  double cumulative = 0.0;

  if (pool->size <= 0)
  {
    elog(ERROR, "pool_size is zero");
  }

     
                                                                           
                                                                            
                                                                           
                                            
     
  for (i = 0; i < pool->size; i++)
  {
    cumulative += pool->data[i].worth / pool->size;
  }

  return cumulative;
}

              
   
void
print_pool(FILE *fp, Pool *pool, int start, int stop)
{
  int i, j;

                                                             

  if (start < 0)
  {
    start = 0;
  }
  if (stop > pool->size)
  {
    stop = pool->size;
  }

  if (start + stop > pool->size)
  {
    start = 0;
    stop = pool->size;
  }

  for (i = start; i < stop; i++)
  {
    fprintf(fp, "%d)\t", i);
    for (j = 0; j < pool->string_length; j++)
    {
      fprintf(fp, "%d ", pool->data[i].string[j]);
    }
    fprintf(fp, "%g\n", pool->data[i].worth);
  }

  fflush(fp);
}

             
   
                                                        
   
void
print_gen(FILE *fp, Pool *pool, int generation)
{
  int lowest;

                                                       
                                             
  lowest = pool->size > 1 ? pool->size - 2 : 0;

  fprintf(fp, "%5d | Best: %g  Worst: %g  Mean: %g  Avg: %g\n", generation, pool->data[0].worth, pool->data[lowest].worth, pool->data[pool->size / 2].worth, avg_pool(pool));

  fflush(fp);
}

void
print_edge_table(FILE *fp, Edge *edge_table, int num_gene)
{
  int i, j;

  fprintf(fp, "\nEDGE TABLE\n");

  for (i = 1; i <= num_gene; i++)
  {
    fprintf(fp, "%d :", i);
    for (j = 0; j < edge_table[i].unused_edges; j++)
    {
      fprintf(fp, " %d", edge_table[i].edge_list[j]);
    }
    fprintf(fp, "\n");
  }

  fprintf(fp, "\n");

  fflush(fp);
}

#endif                 
