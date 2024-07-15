                                                                           
   
              
                                      
   
                                         
   
                                                                            
   

                   
                                                                         
                                                              
                                                    
                                                             
                                                                         
   

                                                                
                                                               
                    
                                  
                                  
                                         
                                       
                    
                                                              
                                                              
                                                              
                    
                                                               

#include "postgres.h"
#include "optimizer/geqo_recombination.h"
#include "optimizer/geqo_random.h"

#if defined(ERX)

static int
gimme_edge(PlannerInfo *root, Gene gene1, Gene gene2, Edge *edge_table);
static void
remove_gene(PlannerInfo *root, Gene gene, Edge edge, Edge *edge_table);
static Gene
gimme_gene(PlannerInfo *root, Edge edge, Edge *edge_table);

static Gene
edge_failure(PlannerInfo *root, Gene *gene, int index, Edge *edge_table, int num_gene);

                    
   
                                   
   
   

Edge *
alloc_edge_table(PlannerInfo *root, int num_gene)
{
  Edge *edge_table;

     
                                                                          
                                  
     

  edge_table = (Edge *)palloc((num_gene + 1) * sizeof(Edge));

  return edge_table;
}

                   
   
                                     
   
   
void
free_edge_table(PlannerInfo *root, Edge *edge_table)
{
  pfree(edge_table);
}

                    
   
                                                                
                                                
   
                                                   
   
                                                            
   
                                                         
                                       
   
   
float
gimme_edge_table(PlannerInfo *root, Gene *tour1, Gene *tour2, int num_gene, Edge *edge_table)
{
  int i, index1, index2;
  int edge_total;                                                

                                                
  for (i = 1; i <= num_gene; i++)
  {
    edge_table[i].total_edges = 0;
    edge_table[i].unused_edges = 0;
  }

                                     

  edge_total = 0;

  for (index1 = 0; index1 < num_gene; index1++)
  {
       
                                                                          
                        
       

    index2 = (index1 + 1) % num_gene;

       
                                                                          
                      
       

    edge_total += gimme_edge(root, tour1[index1], tour1[index2], edge_table);
    gimme_edge(root, tour1[index2], tour1[index1], edge_table);

    edge_total += gimme_edge(root, tour2[index1], tour2[index2], edge_table);
    gimme_edge(root, tour2[index2], tour2[index1], edge_table);
  }

                                                
  return ((float)(edge_total * 2) / (float)num_gene);
}

              
   
                                                            
   
                                                   
                                                  
                                                         
                              
                                                                      
                                                  
   
                                                                      
                                                                    
   
static int
gimme_edge(PlannerInfo *root, Gene gene1, Gene gene2, Edge *edge_table)
{
  int i;
  int edges;
  int city1 = (int)gene1;
  int city2 = (int)gene2;

                                                      
  edges = edge_table[city1].total_edges;

  for (i = 0; i < edges; i++)
  {
    if ((Gene)Abs(edge_table[city1].edge_list[i]) == city2)
    {

                                         
      edge_table[city1].edge_list[i] = 0 - city2;

      return 0;
    }
  }

                         
  edge_table[city1].edge_list[edges] = city2;

                                                
  edge_table[city1].total_edges++;
  edge_table[city1].unused_edges++;

  return 1;
}

              
   
                                                         
                                                           
                                                         
                         
   
   
int
gimme_tour(PlannerInfo *root, Edge *edge_table, Gene *new_gene, int num_gene)
{
  int i;
  int edge_failures = 0;

                                         
  new_gene[0] = (Gene)geqo_randint(root, num_gene, 1);

  for (i = 1; i < num_gene; i++)
  {
       
                                                                       
             
       

    remove_gene(root, new_gene[i - 1], edge_table[(int)new_gene[i - 1]], edge_table);

                                                      

    if (edge_table[new_gene[i - 1]].unused_edges > 0)
    {
      new_gene[i] = gimme_gene(root, edge_table[(int)new_gene[i - 1]], edge_table);
    }

    else
    {                      
      edge_failures++;

      new_gene[i] = edge_failure(root, new_gene, i - 1, edge_table, num_gene);
    }

                                        
    edge_table[(int)new_gene[i - 1]].unused_edges = -1;

  }                                 

  return edge_failures;
}

               
   
                                        
                       
                                                      
   
   
static void
remove_gene(PlannerInfo *root, Gene gene, Edge edge, Edge *edge_table)
{
  int i, j;
  int possess_edge;
  int genes_remaining;

     
                                                                    
                               
     

  for (i = 0; i < edge.unused_edges; i++)
  {
    possess_edge = (int)Abs(edge.edge_list[i]);
    genes_remaining = edge_table[possess_edge].unused_edges;

                                                             
    for (j = 0; j < genes_remaining; j++)
    {

      if ((Gene)Abs(edge_table[possess_edge].edge_list[j]) == gene)
      {

        edge_table[possess_edge].unused_edges--;

        edge_table[possess_edge].edge_list[j] = edge_table[possess_edge].edge_list[genes_remaining - 1];

        break;
      }
    }
  }
}

              
   
                                         
                                           
   
   
static Gene
gimme_gene(PlannerInfo *root, Edge edge, Edge *edge_table)
{
  int i;
  Gene friend;
  int minimum_edges;
  int minimum_count = -1;
  int rand_decision;

     
                                                                         
                              
     

  minimum_edges = 5;

                                                          

  for (i = 0; i < edge.unused_edges; i++)
  {
    friend = (Gene)edge.edge_list[i];

       
                                                                      
       

       
                                                                  
                                     
       
    if (friend < 0)
    {
      return (Gene)Abs(friend);
    }

       
                                                                       
                                                           
                                                                     
                                                                
                        
       

       
                                                                        
                                                                          
                                                                           
                                      
       

    if (edge_table[(int)friend].unused_edges < minimum_edges)
    {
      minimum_edges = edge_table[(int)friend].unused_edges;
      minimum_count = 1;
    }
    else if (minimum_count == -1)
    {
      elog(ERROR, "minimum_count not set");
    }
    else if (edge_table[(int)friend].unused_edges == minimum_edges)
    {
      minimum_count++;
    }

  }                                          

                                                         
  rand_decision = geqo_randint(root, minimum_count - 1, 0);

  for (i = 0; i < edge.unused_edges; i++)
  {
    friend = (Gene)edge.edge_list[i];

                                           
    if (edge_table[(int)friend].unused_edges == minimum_edges)
    {
      minimum_count--;

      if (minimum_count == rand_decision)
      {
        return friend;
      }
    }
  }

                                   
  elog(ERROR, "neither shared nor minimum number nor random edge found");
  return 0;                                 
}

                
   
                                       
   
   
static Gene
edge_failure(PlannerInfo *root, Gene *gene, int index, Edge *edge_table, int num_gene)
{
  int i;
  Gene fail_gene = gene[index];
  int remaining_edges = 0;
  int four_count = 0;
  int rand_decision;

     
                                                                          
             
     

  for (i = 1; i <= num_gene; i++)
  {
    if ((edge_table[i].unused_edges != -1) && (i != (int)fail_gene))
    {
      remaining_edges++;

      if (edge_table[i].total_edges == 4)
      {
        four_count++;
      }
    }
  }

     
                                                                            
          
     

  if (four_count != 0)
  {

    rand_decision = geqo_randint(root, four_count - 1, 0);

    for (i = 1; i <= num_gene; i++)
    {

      if ((Gene)i != fail_gene && edge_table[i].unused_edges != -1 && edge_table[i].total_edges == 4)
      {

        four_count--;

        if (rand_decision == four_count)
        {
          return (Gene)i;
        }
      }
    }

    elog(LOG, "no edge found via random decision and total_edges == 4");
  }
  else if (remaining_edges != 0)
  {
                                                          
    rand_decision = geqo_randint(root, remaining_edges - 1, 0);

    for (i = 1; i <= num_gene; i++)
    {

      if ((Gene)i != fail_gene && edge_table[i].unused_edges != -1)
      {

        remaining_edges--;

        if (rand_decision == remaining_edges)
        {
          return i;
        }
      }
    }

    elog(LOG, "no edge found via random decision with remaining edges");
  }

     
                                                                            
                                                                         
                                                      
     

  else
  {                                               
                                                  
              

    for (i = 1; i <= num_gene; i++)
    {
      if (edge_table[i].unused_edges >= 0)
      {
        return (Gene)i;
      }
    }

    elog(LOG, "no edge found via looking for the last unused point");
  }

                                   
  elog(ERROR, "no edge found");
  return 0;                                 
}

#endif                   
