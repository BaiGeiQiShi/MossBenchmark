                                                                            
   
                
                                         
   
                                                                         
   
                  
                                  
   
                                                                            
   

#include "postgres.h"

#include <math.h>

#include "lib/binaryheap.h"

static void
sift_down(binaryheap *heap, int node_off);
static void
sift_up(binaryheap *heap, int node_off);
static inline void
swap_nodes(binaryheap *heap, int a, int b);

   
                       
   
                                                                        
                                                                      
                                                                            
                                
   
binaryheap *
binaryheap_allocate(int capacity, binaryheap_comparator compare, void *arg)
{
  int sz;
  binaryheap *heap;

  sz = offsetof(binaryheap, bh_nodes) + sizeof(Datum) * capacity;
  heap = (binaryheap *)palloc(sz);
  heap->bh_space = capacity;
  heap->bh_compare = compare;
  heap->bh_arg = arg;

  heap->bh_size = 0;
  heap->bh_has_heap_property = true;

  return heap;
}

   
                    
   
                                                                          
                                    
   
void
binaryheap_reset(binaryheap *heap)
{
  heap->bh_size = 0;
  heap->bh_has_heap_property = true;
}

   
                   
   
                                                 
   
void
binaryheap_free(binaryheap *heap)
{
  pfree(heap);
}

   
                                                                      
                                                                   
   
                                                                    
                                                                      
                                                                       
   

static inline int
left_offset(int i)
{
  return 2 * i + 1;
}

static inline int
right_offset(int i)
{
  return 2 * i + 2;
}

static inline int
parent_offset(int i)
{
  return (i - 1) / 2;
}

   
                            
   
                                                                               
                                                                               
                                                                           
               
   
void
binaryheap_add_unordered(binaryheap *heap, Datum d)
{
  if (heap->bh_size >= heap->bh_space)
  {
    elog(ERROR, "out of binary heap slots");
  }
  heap->bh_has_heap_property = false;
  heap->bh_nodes[heap->bh_size] = d;
  heap->bh_size++;
}

   
                    
   
                                                          
                                                     
   
void
binaryheap_build(binaryheap *heap)
{
  int i;

  for (i = parent_offset(heap->bh_size - 1); i >= 0; i--)
  {
    sift_down(heap, i);
  }
  heap->bh_has_heap_property = true;
}

   
                  
   
                                                                       
                      
   
void
binaryheap_add(binaryheap *heap, Datum d)
{
  if (heap->bh_size >= heap->bh_space)
  {
    elog(ERROR, "out of binary heap slots");
  }
  heap->bh_nodes[heap->bh_size] = d;
  heap->bh_size++;
  sift_up(heap, heap->bh_size - 1);
}

   
                    
   
                                                                   
                                                                
                                                      
   
Datum
binaryheap_first(binaryheap *heap)
{
  Assert(!binaryheap_empty(heap) && heap->bh_has_heap_property);
  return heap->bh_nodes[0];
}

   
                           
   
                                                                    
                                                                    
                                                                  
         
   
Datum
binaryheap_remove_first(binaryheap *heap)
{
  Assert(!binaryheap_empty(heap) && heap->bh_has_heap_property);

  if (heap->bh_size == 1)
  {
    heap->bh_size--;
    return heap->bh_nodes[0];
  }

     
                                                                       
                                                                         
                       
     
  swap_nodes(heap, 0, heap->bh_size - 1);
  heap->bh_size--;
  sift_down(heap, 0);

  return heap->bh_nodes[heap->bh_size];
}

   
                            
   
                                                                        
                                                                         
                              
   
void
binaryheap_replace_first(binaryheap *heap, Datum d)
{
  Assert(!binaryheap_empty(heap) && heap->bh_has_heap_property);

  heap->bh_nodes[0] = d;

  if (heap->bh_size > 1)
  {
    sift_down(heap, 0);
  }
}

   
                                   
   
static inline void
swap_nodes(binaryheap *heap, int a, int b)
{
  Datum swap;

  swap = heap->bh_nodes[a];
  heap->bh_nodes[a] = heap->bh_nodes[b];
  heap->bh_nodes[b] = swap;
}

   
                                                                       
               
   
static void
sift_up(binaryheap *heap, int node_off)
{
  while (node_off != 0)
  {
    int cmp;
    int parent_off;

       
                                                                      
                                  
       
    parent_off = parent_offset(node_off);
    cmp = heap->bh_compare(heap->bh_nodes[node_off], heap->bh_nodes[parent_off], heap->bh_arg);
    if (cmp <= 0)
    {
      break;
    }

       
                                                                      
                          
       
    swap_nodes(heap, node_off, parent_off);
    node_off = parent_off;
  }
}

   
                                                                  
             
   
static void
sift_down(binaryheap *heap, int node_off)
{
  while (true)
  {
    int left_off = left_offset(node_off);
    int right_off = right_offset(node_off);
    int swap_off = 0;

                                                   
    if (left_off < heap->bh_size && heap->bh_compare(heap->bh_nodes[node_off], heap->bh_nodes[left_off], heap->bh_arg) < 0)
    {
      swap_off = left_off;
    }

                                                    
    if (right_off < heap->bh_size && heap->bh_compare(heap->bh_nodes[node_off], heap->bh_nodes[right_off], heap->bh_arg) < 0)
    {
                                      
      if (!swap_off || heap->bh_compare(heap->bh_nodes[left_off], heap->bh_nodes[right_off], heap->bh_arg) < 0)
      {
        swap_off = right_off;
      }
    }

       
                                                                 
                                  
       
    if (!swap_off)
    {
      break;
    }

       
                                                                      
                                                   
       
    swap_nodes(heap, swap_off, node_off);
    node_off = swap_off;
  }
}
