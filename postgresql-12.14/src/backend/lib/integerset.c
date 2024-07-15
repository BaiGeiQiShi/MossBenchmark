                                                                            
   
                
                                                                       
   
                                                                    
                                                                      
                                                                        
                                                                     
                 
   
                                                                       
                                                                      
                                                                          
                                                                        
                                                                       
                                                                          
                                                                  
   
   
             
             
   
                                             
                                                  
                                                        
                                                                      
                                                         
   
                                                                              
                                                                            
                                                        
   
                                                                             
                                                                              
                    
   
   
               
               
   
                                                                      
                                                      
   
                                                            
   
                                     
   
                                                                            
                                                                         
                                           
   
   
              
              
   
                                   
   
                                                                       
                                                                          
                                       
   
   
                                                                         
                                                                        
   
                  
                                  
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "lib/integerset.h"
#include "port/pg_bitutils.h"
#include "utils/memutils.h"

   
                                                                        
                                                                            
                
   
#define SIMPLE8B_MAX_VALUES_PER_CODEWORD 240

   
                                                 
   
                                                                              
                                                                         
                                                 
   
                                                                   
   
#define MAX_INTERNAL_ITEMS 64
#define MAX_LEAF_ITEMS 64

   
                               
   
                                                                       
                                                                           
                                             
   
                                                                          
   
                                                                         
                                                                       
   
#define MAX_TREE_LEVELS 11

   
                                              
   
                                                                          
                                                                         
                                                                           
                                                                              
                                                    
   
                                                                      
                                                                            
                                                                               
                                                                          
                                                                         
                                                                           
                                                                              
                                                                              
                        
   
                                                                             
                                                                    
   
typedef struct intset_node intset_node;
typedef struct intset_leaf_node intset_leaf_node;
typedef struct intset_internal_node intset_internal_node;

                                                       
struct intset_node
{
  uint16 level;                                  
  uint16 num_items;                                   
};

                   
struct intset_internal_node
{
                                             
  uint16 level;                             
  uint16 num_items;

     
                                                                         
                                                         
     
  uint64 values[MAX_INTERNAL_ITEMS];
  intset_node *downlinks[MAX_INTERNAL_ITEMS];
};

               
typedef struct
{
  uint64 first;                                    
  uint64 codeword;                                                
} leaf_item;

#define MAX_VALUES_PER_LEAF_ITEM (1 + SIMPLE8B_MAX_VALUES_PER_CODEWORD)

struct intset_leaf_node
{
                                             
  uint16 level;                 
  uint16 num_items;

  intset_leaf_node *next;                            

  leaf_item items[MAX_LEAF_ITEMS];
};

   
                                                                             
                                                                           
                                                                          
                                                                              
                                                                               
   
#define MAX_BUFFERED_VALUES (MAX_VALUES_PER_LEAF_ITEM * 2)

   
                                                            
   
                                                                           
                                                                              
                                                                      
                                               
   
struct IntegerSet
{
     
                                                                          
                 
     
                                                                             
                                                                    
                            
     
  MemoryContext context;
  uint64 mem_used;

  uint64 num_entries;                                     
  uint64 highest_value;                                       

     
                                       
     
                                                                          
                                                                       
                                                                             
                                                                            
               
     
  int num_levels;                            
  intset_node *root;                
  intset_node *rightmost_nodes[MAX_TREE_LEVELS];
  intset_leaf_node *leftmost_leaf;                         

     
                                                                            
     
  uint64 buffered_values[MAX_BUFFERED_VALUES];
  int num_buffered_values;

     
                       
     
                                                                       
                                                                
                                                                            
                                                                            
                     
     
                                                                            
                                                                            
                                                                    
                                       
     
  bool iter_active;                                

  const uint64 *iter_values;
  int iter_num_values;                                          
  int iter_valueno;                                       

  intset_leaf_node *iter_node;                        
  int iter_itemno;                                                     

  uint64 iter_values_buf[MAX_VALUES_PER_LEAF_ITEM];
};

   
                                      
   
static void
intset_update_upper(IntegerSet *intset, int level, intset_node *child, uint64 child_key);
static void
intset_flush_buffered_values(IntegerSet *intset);

static int
intset_binsrch_uint64(uint64 value, uint64 *arr, int arr_elems, bool nextkey);
static int
intset_binsrch_leaf(uint64 value, leaf_item *arr, int arr_elems, bool nextkey);

static uint64
simple8b_encode(const uint64 *ints, int *num_encoded, uint64 base);
static int
simple8b_decode(uint64 codeword, uint64 *decoded, uint64 base);
static bool
simple8b_contains(uint64 codeword, uint64 key, uint64 base);

   
                                               
   
                                                             
                                                                              
                                                                              
   
IntegerSet *
intset_create(void)
{
  IntegerSet *intset;

  intset = (IntegerSet *)palloc(sizeof(IntegerSet));
  intset->context = CurrentMemoryContext;
  intset->mem_used = GetMemoryChunkSpace(intset);

  intset->num_entries = 0;
  intset->highest_value = 0;

  intset->num_levels = 0;
  intset->root = NULL;
  memset(intset->rightmost_nodes, 0, sizeof(intset->rightmost_nodes));
  intset->leftmost_leaf = NULL;

  intset->num_buffered_values = 0;

  intset->iter_active = false;
  intset->iter_node = NULL;
  intset->iter_itemno = 0;
  intset->iter_valueno = 0;
  intset->iter_num_values = 0;
  intset->iter_values = NULL;

  return intset;
}

   
                        
   
static intset_internal_node *
intset_new_internal_node(IntegerSet *intset)
{
  intset_internal_node *n;

  n = (intset_internal_node *)MemoryContextAlloc(intset->context, sizeof(intset_internal_node));
  intset->mem_used += GetMemoryChunkSpace(n);

  n->level = 0;                      
  n->num_items = 0;

  return n;
}

static intset_leaf_node *
intset_new_leaf_node(IntegerSet *intset)
{
  intset_leaf_node *n;

  n = (intset_leaf_node *)MemoryContextAlloc(intset->context, sizeof(intset_leaf_node));
  intset->mem_used += GetMemoryChunkSpace(n);

  n->level = 0;
  n->num_items = 0;
  n->next = NULL;

  return n;
}

   
                                                    
   
uint64
intset_num_entries(IntegerSet *intset)
{
  return intset->num_entries;
}

   
                                                        
   
uint64
intset_memory_usage(IntegerSet *intset)
{
  return intset->mem_used;
}

   
                           
   
                                  
   
void
intset_add_member(IntegerSet *intset, uint64 x)
{
  if (intset->iter_active)
  {
    elog(ERROR, "cannot add new values to integer set while iteration is in progress");
  }

  if (x <= intset->highest_value && intset->num_entries > 0)
  {
    elog(ERROR, "cannot add value to integer set out of order");
  }

  if (intset->num_buffered_values >= MAX_BUFFERED_VALUES)
  {
                                  
    intset_flush_buffered_values(intset);
    Assert(intset->num_buffered_values < MAX_BUFFERED_VALUES);
  }

                                                  
  intset->buffered_values[intset->num_buffered_values] = x;
  intset->num_buffered_values++;
  intset->num_entries++;
  intset->highest_value = x;
}

   
                                                                   
   
static void
intset_flush_buffered_values(IntegerSet *intset)
{
  uint64 *values = intset->buffered_values;
  uint64 num_values = intset->num_buffered_values;
  int num_packed = 0;
  intset_leaf_node *leaf;

  leaf = (intset_leaf_node *)intset->rightmost_nodes[0];

     
                                                                           
                    
     
  if (leaf == NULL)
  {
       
                                               
       
                                             
       
    leaf = intset_new_leaf_node(intset);

    intset->root = (intset_node *)leaf;
    intset->leftmost_leaf = leaf;
    intset->rightmost_nodes[0] = (intset_node *)leaf;
    intset->num_levels = 1;
  }

     
                                                                           
                                                                         
                                                                          
                   
     
  while (num_values - num_packed >= MAX_VALUES_PER_LEAF_ITEM)
  {
    leaf_item item;
    int num_encoded;

       
                                                                        
                 
       
    item.first = values[num_packed];
    item.codeword = simple8b_encode(&values[num_packed + 1], &num_encoded, item.first);

       
                                                                         
             
       
    if (leaf->num_items >= MAX_LEAF_ITEMS)
    {
                                                     
      intset_leaf_node *old_leaf = leaf;

      leaf = intset_new_leaf_node(intset);
      old_leaf->next = leaf;
      intset->rightmost_nodes[0] = (intset_node *)leaf;
      intset_update_upper(intset, 1, (intset_node *)leaf, item.first);
    }
    leaf->items[leaf->num_items++] = item;

    num_packed += 1 + num_encoded;
  }

     
                                                                       
     
  if (num_packed < intset->num_buffered_values)
  {
    memmove(&intset->buffered_values[0], &intset->buffered_values[num_packed], (intset->num_buffered_values - num_packed) * sizeof(uint64));
  }
  intset->num_buffered_values -= num_packed;
}

   
                                                                  
   
                                             
   
static void
intset_update_upper(IntegerSet *intset, int level, intset_node *child, uint64 child_key)
{
  intset_internal_node *parent;

  Assert(level > 0);

     
                                           
     
  if (level >= intset->num_levels)
  {
    intset_node *oldroot = intset->root;
    uint64 downlink_key;

                                                                           
    if (intset->num_levels == MAX_TREE_LEVELS)
    {
      elog(ERROR, "could not expand integer set, maximum number of levels reached");
    }
    intset->num_levels++;

       
                                                                   
                 
       
    if (intset->root->level == 0)
    {
      downlink_key = ((intset_leaf_node *)oldroot)->items[0].first;
    }
    else
    {
      downlink_key = ((intset_internal_node *)oldroot)->values[0];
    }

    parent = intset_new_internal_node(intset);
    parent->level = level;
    parent->values[0] = downlink_key;
    parent->downlinks[0] = oldroot;
    parent->num_items = 1;

    intset->root = (intset_node *)parent;
    intset->rightmost_nodes[level] = (intset_node *)parent;
  }

     
                                            
     
  parent = (intset_internal_node *)intset->rightmost_nodes[level];

  if (parent->num_items < MAX_INTERNAL_ITEMS)
  {
    parent->values[parent->num_items] = child_key;
    parent->downlinks[parent->num_items] = child;
    parent->num_items++;
  }
  else
  {
       
                                                                         
                                                                         
                           
       
    parent = intset_new_internal_node(intset);
    parent->level = level;
    parent->values[0] = child_key;
    parent->downlinks[0] = child;
    parent->num_items = 1;

    intset->rightmost_nodes[level] = (intset_node *)parent;

    intset_update_upper(intset, level + 1, (intset_node *)parent, child_key);
  }
}

   
                                         
   
bool
intset_is_member(IntegerSet *intset, uint64 x)
{
  intset_node *node;
  intset_leaf_node *leaf;
  int level;
  int itemno;
  leaf_item *item;

     
                                                             
     
  if (intset->num_buffered_values > 0 && x >= intset->buffered_values[0])
  {
    int itemno;

    itemno = intset_binsrch_uint64(x, intset->buffered_values, intset->num_buffered_values, false);
    if (itemno >= intset->num_buffered_values)
    {
      return false;
    }
    else
    {
      return (intset->buffered_values[itemno] == x);
    }
  }

     
                                                                          
           
     
  if (!intset->root)
  {
    return false;
  }
  node = intset->root;
  for (level = intset->num_levels - 1; level > 0; level--)
  {
    intset_internal_node *n = (intset_internal_node *)node;

    Assert(node->level == level);

    itemno = intset_binsrch_uint64(x, n->values, n->num_items, true);
    if (itemno == 0)
    {
      return false;
    }
    node = n->downlinks[itemno - 1];
  }
  Assert(node->level == 0);
  leaf = (intset_leaf_node *)node;

     
                                                           
     
  itemno = intset_binsrch_leaf(x, leaf->items, leaf->num_items, true);
  if (itemno == 0)
  {
    return false;
  }
  item = &leaf->items[itemno - 1];

                                                       
  if (item->first == x)
  {
    return true;
  }
  Assert(x > item->first);

                                     
  if (simple8b_contains(item->codeword, x, item->first))
  {
    return true;
  }

  return false;
}

   
                                               
   
                                                                             
   
void
intset_begin_iterate(IntegerSet *intset)
{
                                                              
  intset->iter_active = true;
  intset->iter_node = intset->leftmost_leaf;
  intset->iter_itemno = 0;
  intset->iter_valueno = 0;
  intset->iter_num_values = 0;
  intset->iter_values = intset->iter_values_buf;
}

   
                                             
   
                                                                               
                                                                             
                                                         
   
bool
intset_iterate_next(IntegerSet *intset, uint64 *next)
{
  Assert(intset->iter_active);
  for (;;)
  {
                                                
    if (intset->iter_valueno < intset->iter_num_values)
    {
      *next = intset->iter_values[intset->iter_valueno++];
      return true;
    }

                                                       
    if (intset->iter_node && intset->iter_itemno < intset->iter_node->num_items)
    {
      leaf_item *item;
      int num_decoded;

      item = &intset->iter_node->items[intset->iter_itemno++];

      intset->iter_values_buf[0] = item->first;
      num_decoded = simple8b_decode(item->codeword, &intset->iter_values_buf[1], item->first);
      intset->iter_num_values = num_decoded + 1;
      intset->iter_valueno = 0;
      continue;
    }

                                                       
    if (intset->iter_node)
    {
      intset->iter_node = intset->iter_node->next;
      intset->iter_itemno = 0;
      continue;
    }

       
                                                                       
                                                          
       
    if (intset->iter_values == (const uint64 *)intset->iter_values_buf)
    {
      intset->iter_values = intset->buffered_values;
      intset->iter_num_values = intset->num_buffered_values;
      intset->iter_valueno = 0;
      continue;
    }

    break;
  }

                        
  intset->iter_active = false;
  *next = 0;                                              
  return false;
}

   
                                                               
   
                                                                         
                                                                           
                                                                  
   
                                                                           
                                                                           
                                                                            
   
static int
intset_binsrch_uint64(uint64 item, uint64 *arr, int arr_elems, bool nextkey)
{
  int low, high, mid;

  low = 0;
  high = arr_elems;
  while (high > low)
  {
    mid = low + (high - low) / 2;

    if (nextkey)
    {
      if (item >= arr[mid])
      {
        low = mid + 1;
      }
      else
      {
        high = mid;
      }
    }
    else
    {
      if (item > arr[mid])
      {
        low = mid + 1;
      }
      else
      {
        high = mid;
      }
    }
  }

  return low;
}

                                          
static int
intset_binsrch_leaf(uint64 item, leaf_item *arr, int arr_elems, bool nextkey)
{
  int low, high, mid;

  low = 0;
  high = arr_elems;
  while (high > low)
  {
    mid = low + (high - low) / 2;

    if (nextkey)
    {
      if (item >= arr[mid].first)
      {
        low = mid + 1;
      }
      else
      {
        high = mid;
      }
    }
    else
    {
      if (item > arr[mid].first)
      {
        low = mid + 1;
      }
      else
      {
        high = mid;
      }
    }
  }

  return low;
}

   
                       
   
                                                                               
                                                                             
                                                                          
                                                                         
                                                        
   
                                                                             
                                                                          
                                                                            
                             
   
                                                                             
                                                                               
                                                                            
                                                                                
                                                                              
                                                               
   
                                                                 
                                                                       
     
            
   
                                                                           
                                                                               
                                                                               
                                                                             
           
   
                                                                           
                                                                          
                                                                         
                 
   
                                                                             
                                                                            
                                                                           
                                                                               
                                                 
   
static const struct simple8b_mode
{
  uint8 bits_per_int;
  uint8 num_ints;
} simple8b_modes[17] =

    {
        {0, 240},                          
        {0, 120},                          
        {1, 60},                                     
        {2, 30},                                      
        {3, 20},                                      
        {4, 15},                                       
        {5, 12},                                      
        {6, 10},                                   
        {7, 8},                                               
                                   
        {8, 7},                                               
                                   
        {10, 6},                                    
        {12, 5},                                     
        {15, 4},                                     
        {20, 3},                                      
        {30, 2},                                    
        {60, 1},                                   

        {0, 0}                     
};

   
                                                                    
                                                                           
   
                                                                      
                                                                           
   
#define EMPTY_CODEWORD UINT64CONST(0x0FFFFFFFFFFFFFFF)

   
                                                          
   
                                                                    
                                        
   
                                                                          
                                                           
   
                                                                        
                                                                           
                               
   
static uint64
simple8b_encode(const uint64 *ints, int *num_encoded, uint64 base)
{
  int selector;
  int nints;
  int bits;
  uint64 diff;
  uint64 last_val;
  uint64 codeword;
  int i;

  Assert(ints[0] > base);

     
                                                 
     
                                                                          
                                                                          
                                                                       
                                                                          
     
                                                                      
                                                                       
                                                                            
                                                                            
                                                                            
                           
     
  selector = 0;
  nints = simple8b_modes[0].num_ints;
  bits = simple8b_modes[0].bits_per_int;
  diff = ints[0] - base - 1;
  last_val = ints[0];
  i = 0;                                        
  for (;;)
  {
    if (diff >= (UINT64CONST(1) << bits))
    {
                                           
      selector++;
      nints = simple8b_modes[selector].num_ints;
      bits = simple8b_modes[selector].bits_per_int;
                                                                      
      if (i >= nints)
      {
        break;
      }
    }
    else
    {
                                                            
      i++;
      if (i >= nints)
      {
        break;
      }
                              
      Assert(ints[i] > last_val);
      diff = ints[i] - last_val - 1;
      last_val = ints[i];
    }
  }

  if (nints == 0)
  {
       
                                                                  
       
                                                                       
                                                                         
                                                          
       
    Assert(i == 0);
    *num_encoded = 0;
    return EMPTY_CODEWORD;
  }

     
                                                                           
                                                                           
                                   
     
  codeword = 0;
  if (bits > 0)
  {
    for (i = nints - 1; i > 0; i--)
    {
      diff = ints[i] - ints[i - 1] - 1;
      codeword |= diff;
      codeword <<= bits;
    }
    diff = ints[0] - base - 1;
    codeword |= diff;
  }

                                                
  codeword |= (uint64)selector << 60;

  *num_encoded = nints;
  return codeword;
}

   
                                                
                                           
   
static int
simple8b_decode(uint64 codeword, uint64 *decoded, uint64 base)
{
  int selector = (codeword >> 60);
  int nints = simple8b_modes[selector].num_ints;
  int bits = simple8b_modes[selector].bits_per_int;
  uint64 mask = (UINT64CONST(1) << bits) - 1;
  uint64 curr_value;

  if (codeword == EMPTY_CODEWORD)
  {
    return 0;
  }

  curr_value = base;
  for (int i = 0; i < nints; i++)
  {
    uint64 diff = codeword & mask;

    curr_value += 1 + diff;
    decoded[i] = curr_value;
    codeword >>= bits;
  }
  return nints;
}

   
                                                                          
                                                                        
                 
   
static bool
simple8b_contains(uint64 codeword, uint64 key, uint64 base)
{
  int selector = (codeword >> 60);
  int nints = simple8b_modes[selector].num_ints;
  int bits = simple8b_modes[selector].bits_per_int;

  if (codeword == EMPTY_CODEWORD)
  {
    return false;
  }

  if (bits == 0)
  {
                                           
    return (key - base) <= nints;
  }
  else
  {
    uint64 mask = (UINT64CONST(1) << bits) - 1;
    uint64 curr_value;

    curr_value = base;
    for (int i = 0; i < nints; i++)
    {
      uint64 diff = codeword & mask;

      curr_value += 1 + diff;

      if (curr_value >= key)
      {
        if (curr_value == key)
        {
          return true;
        }
        else
        {
          return false;
        }
      }

      codeword >>= bits;
    }
  }
  return false;
}
