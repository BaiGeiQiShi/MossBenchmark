                                                                            
   
                     
                                                                      
   
                                                        
   
                                                                                              
   
                                                                
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include <limits.h>

#include "lib/bipartite_match.h"
#include "miscadmin.h"

   
                                                                           
                                                                         
                                                                             
   
#define HK_INFINITY SHRT_MAX

static bool
hk_breadth_search(BipartiteMatchState *state);
static bool
hk_depth_search(BipartiteMatchState *state, int u);

   
                                                                              
                                                              
   
BipartiteMatchState *
BipartiteMatch(int u_size, int v_size, short **adjacency)
{
  BipartiteMatchState *state = palloc(sizeof(BipartiteMatchState));

  if (u_size < 0 || u_size >= SHRT_MAX || v_size < 0 || v_size >= SHRT_MAX)
  {
    elog(ERROR, "invalid set size for BipartiteMatch");
  }

  state->u_size = u_size;
  state->v_size = v_size;
  state->adjacency = adjacency;
  state->matching = 0;
  state->pair_uv = (short *)palloc0((u_size + 1) * sizeof(short));
  state->pair_vu = (short *)palloc0((v_size + 1) * sizeof(short));
  state->distance = (short *)palloc((u_size + 1) * sizeof(short));
  state->queue = (short *)palloc((u_size + 2) * sizeof(short));

  while (hk_breadth_search(state))
  {
    int u;

    for (u = 1; u <= u_size; u++)
    {
      if (state->pair_uv[u] == 0)
      {
        if (hk_depth_search(state, u))
        {
          state->matching++;
        }
      }
    }

    CHECK_FOR_INTERRUPTS();                   
  }

  return state;
}

   
                                                                              
                                                                                 
   
void
BipartiteMatchFree(BipartiteMatchState *state)
{
                                                          
  pfree(state->pair_uv);
  pfree(state->pair_vu);
  pfree(state->distance);
  pfree(state->queue);
  pfree(state);
}

   
                                                          
                               
   
static bool
hk_breadth_search(BipartiteMatchState *state)
{
  int usize = state->u_size;
  short *queue = state->queue;
  short *distance = state->distance;
  int qhead = 0;                                               
  int qtail = 0;                                            
  int u;

  distance[0] = HK_INFINITY;

  for (u = 1; u <= usize; u++)
  {
    if (state->pair_uv[u] == 0)
    {
      distance[u] = 0;
      queue[qhead++] = u;
    }
    else
    {
      distance[u] = HK_INFINITY;
    }
  }

  while (qtail < qhead)
  {
    u = queue[qtail++];

    if (distance[u] < distance[0])
    {
      short *u_adj = state->adjacency[u];
      int i = u_adj ? u_adj[0] : 0;

      for (; i > 0; i--)
      {
        int u_next = state->pair_vu[u_adj[i]];

        if (distance[u_next] == HK_INFINITY)
        {
          distance[u_next] = 1 + distance[u];
          Assert(qhead < usize + 2);
          queue[qhead++] = u_next;
        }
      }
    }
  }

  return (distance[0] != HK_INFINITY);
}

   
                                                        
                               
   
static bool
hk_depth_search(BipartiteMatchState *state, int u)
{
  short *distance = state->distance;
  short *pair_uv = state->pair_uv;
  short *pair_vu = state->pair_vu;
  short *u_adj = state->adjacency[u];
  int i = u_adj ? u_adj[0] : 0;
  short nextdist;

  if (u == 0)
  {
    return true;
  }
  if (distance[u] == HK_INFINITY)
  {
    return false;
  }
  nextdist = distance[u] + 1;

  check_stack_depth();

  for (; i > 0; i--)
  {
    int v = u_adj[i];

    if (distance[pair_vu[v]] == nextdist)
    {
      if (hk_depth_search(state, pair_vu[v]))
      {
        pair_vu[v] = u;
        pair_uv[u] = v;
        return true;
      }
    }
  }

  distance[u] = HK_INFINITY;
  return false;
}
