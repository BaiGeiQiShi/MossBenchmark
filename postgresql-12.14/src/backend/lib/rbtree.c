                                                                            
   
            
                                                                         
                                                          
   
                                                                            
                
   
                                                                           
                                                                             
                                            
   
                                                                               
                                                                              
                                                                               
                                                                               
                                                     
   
                                                                
   
                  
                              
   
                                                                            
   
#include "postgres.h"

#include "lib/rbtree.h"

   
                                             
   
#define RBTBLACK (0)
#define RBTRED (1)

   
                            
   
struct RBTree
{
  RBTNode *root;                                            

                                                      

  Size node_size;                                
                                                  
  rbt_comparator comparator;
  rbt_combiner combiner;
  rbt_allocfunc allocfunc;
  rbt_freefunc freefunc;
                                                            
  void *arg;
};

   
                                                               
                                                                  
   
#define RBTNIL (&sentinel)

static RBTNode sentinel = {RBTBLACK, RBTNIL, RBTNIL, NULL};

   
                                      
   
                  
                                                            
                               
                                                           
                                                         
                                     
                                 
                                                                              
   
                                                                               
                                                                             
                                                                               
                                                                            
                                                                               
       
   
                                                                          
                                                                              
                                                                     
                      
   
                                                                          
                                                                            
   
                                                                         
                                                                       
                                                                           
                                         
   
RBTree *
rbt_create(Size node_size, rbt_comparator comparator, rbt_combiner combiner, rbt_allocfunc allocfunc, rbt_freefunc freefunc, void *arg)
{
  RBTree *tree = (RBTree *)palloc(sizeof(RBTree));

  Assert(node_size > sizeof(RBTNode));

  tree->root = RBTNIL;
  tree->node_size = node_size;
  tree->comparator = comparator;
  tree->combiner = combiner;
  tree->allocfunc = allocfunc;
  tree->freefunc = freefunc;

  tree->arg = arg;

  return tree;
}

                                                                 
static inline void
rbt_copy_data(RBTree *rbt, RBTNode *dest, const RBTNode *src)
{
  memcpy(dest + 1, src + 1, rbt->node_size - sizeof(RBTNode));
}

                                                                        
                             
                                                                        

   
                                             
   
                                                                          
                                                                           
   
                                                                  
   
RBTNode *
rbt_find(RBTree *rbt, const RBTNode *data)
{
  RBTNode *node = rbt->root;

  while (node != RBTNIL)
  {
    int cmp = rbt->comparator(data, node, rbt->arg);

    if (cmp == 0)
    {
      return node;
    }
    else if (cmp < 0)
    {
      node = node->left;
    }
    else
    {
      node = node->right;
    }
  }

  return NULL;
}

   
                                                                 
                                  
   
                                                                          
                                                                            
             
   
RBTNode *
rbt_leftmost(RBTree *rbt)
{
  RBTNode *node = rbt->root;
  RBTNode *leftmost = rbt->root;

  while (node != RBTNIL)
  {
    leftmost = node;
    node = node->left;
  }

  if (leftmost != RBTNIL)
  {
    return leftmost;
  }

  return NULL;
}

                                                                        
                                
                                                                        

   
                          
   
                                                                       
                       
   
static void
rbt_rotate_left(RBTree *rbt, RBTNode *x)
{
  RBTNode *y = x->right;

                               
  x->right = y->left;
  if (y->left != RBTNIL)
  {
    y->left->parent = x;
  }

                                
  if (y != RBTNIL)
  {
    y->parent = x->parent;
  }
  if (x->parent)
  {
    if (x == x->parent->left)
    {
      x->parent->left = y;
    }
    else
    {
      x->parent->right = y;
    }
  }
  else
  {
    rbt->root = y;
  }

                    
  y->left = x;
  if (x != RBTNIL)
  {
    x->parent = y;
  }
}

   
                           
   
                                                                             
                       
   
static void
rbt_rotate_right(RBTree *rbt, RBTNode *x)
{
  RBTNode *y = x->left;

                              
  x->left = y->right;
  if (y->right != RBTNIL)
  {
    y->right->parent = x;
  }

                                
  if (y != RBTNIL)
  {
    y->parent = x->parent;
  }
  if (x->parent)
  {
    if (x == x->parent->right)
    {
      x->parent->right = y;
    }
    else
    {
      x->parent->left = y;
    }
  }
  else
  {
    rbt->root = y;
  }

                    
  y->right = x;
  if (x != RBTNIL)
  {
    x->parent = y;
  }
}

   
                                                           
   
                                                                             
                                                                              
                                                                           
                                                                              
                                                                            
                           
   
                                                                            
                                                          
   
static void
rbt_insert_fixup(RBTree *rbt, RBTNode *x)
{
     
                                                                             
                                                            
     
  while (x != rbt->root && x->parent->color == RBTRED)
  {
       
                                                                          
                                                                     
                                                                  
       
                                                                       
                                                                          
                                        
       
                                                                        
                                                              
                                                                       
                                                                        
                                                                        
                                
       
    if (x->parent == x->parent->parent->left)
    {
      RBTNode *y = x->parent->parent->right;

      if (y->color == RBTRED)
      {
                             
        x->parent->color = RBTBLACK;
        y->color = RBTBLACK;
        x->parent->parent->color = RBTRED;

        x = x->parent->parent;
      }
      else
      {
                               
        if (x == x->parent->right)
        {
                                   
          x = x->parent;
          rbt_rotate_left(rbt, x);
        }

                                
        x->parent->color = RBTBLACK;
        x->parent->parent->color = RBTRED;

        rbt_rotate_right(rbt, x->parent->parent);
      }
    }
    else
    {
                                      
      RBTNode *y = x->parent->parent->left;

      if (y->color == RBTRED)
      {
                             
        x->parent->color = RBTBLACK;
        y->color = RBTBLACK;
        x->parent->parent->color = RBTRED;

        x = x->parent->parent;
      }
      else
      {
                               
        if (x == x->parent->left)
        {
          x = x->parent;
          rbt_rotate_right(rbt, x);
        }
        x->parent->color = RBTBLACK;
        x->parent->parent->color = RBTRED;

        rbt_rotate_left(rbt, x->parent->parent);
      }
    }
  }

     
                                                                             
                                        
     
  rbt->root->color = RBTBLACK;
}

   
                                                 
   
                                                                     
                                                                           
   
                                                                       
                                                                             
            
   
                                                                           
                                                                          
                                           
   
                                                                    
                           
   
RBTNode *
rbt_insert(RBTree *rbt, const RBTNode *data, bool *isNew)
{
  RBTNode *current, *parent, *x;
  int cmp;

                               
  current = rbt->root;
  parent = NULL;
  cmp = 0;                                       

  while (current != RBTNIL)
  {
    cmp = rbt->comparator(data, current, rbt->arg);
    if (cmp == 0)
    {
         
                                                     
         
      rbt->combiner(current, data, rbt->arg);
      *isNew = false;
      return current;
    }
    parent = current;
    current = (cmp < 0) ? current->left : current->right;
  }

     
                                                                 
     
  *isNew = true;

  x = rbt->allocfunc(rbt->arg);

  x->color = RBTRED;

  x->left = RBTNIL;
  x->right = RBTNIL;
  x->parent = parent;
  rbt_copy_data(rbt, x, data);

                           
  if (parent)
  {
    if (cmp < 0)
    {
      parent->left = x;
    }
    else
    {
      parent->right = x;
    }
  }
  else
  {
    rbt->root = x;
  }

  rbt_insert_fixup(rbt, x);

  return x;
}

                                                                        
                             
                                                                        

   
                                                                
   
static void
rbt_delete_fixup(RBTree *rbt, RBTNode *x)
{
     
                                                                         
                                                                          
           
     
  while (x != rbt->root && x->color == RBTBLACK)
  {
       
                                                                           
                                                                         
                                                                         
                                                                           
                                                        
       
    if (x == x->parent->left)
    {
      RBTNode *w = x->parent->right;

      if (w->color == RBTRED)
      {
        w->color = RBTBLACK;
        x->parent->color = RBTRED;

        rbt_rotate_left(rbt, x->parent);
        w = x->parent->right;
      }

      if (w->left->color == RBTBLACK && w->right->color == RBTBLACK)
      {
        w->color = RBTRED;

        x = x->parent;
      }
      else
      {
        if (w->right->color == RBTBLACK)
        {
          w->left->color = RBTBLACK;
          w->color = RBTRED;

          rbt_rotate_right(rbt, w);
          w = x->parent->right;
        }
        w->color = x->parent->color;
        x->parent->color = RBTBLACK;
        w->right->color = RBTBLACK;

        rbt_rotate_left(rbt, x->parent);
        x = rbt->root;                                     
      }
    }
    else
    {
      RBTNode *w = x->parent->left;

      if (w->color == RBTRED)
      {
        w->color = RBTBLACK;
        x->parent->color = RBTRED;

        rbt_rotate_right(rbt, x->parent);
        w = x->parent->left;
      }

      if (w->right->color == RBTBLACK && w->left->color == RBTBLACK)
      {
        w->color = RBTRED;

        x = x->parent;
      }
      else
      {
        if (w->left->color == RBTBLACK)
        {
          w->right->color = RBTBLACK;
          w->color = RBTRED;

          rbt_rotate_left(rbt, w);
          w = x->parent->left;
        }
        w->color = x->parent->color;
        x->parent->color = RBTBLACK;
        w->left->color = RBTBLACK;

        rbt_rotate_right(rbt, x->parent);
        x = rbt->root;                                     
      }
    }
  }
  x->color = RBTBLACK;
}

   
                            
   
static void
rbt_delete_node(RBTree *rbt, RBTNode *z)
{
  RBTNode *x, *y;

                                                                        
  if (!z || z == RBTNIL)
  {
    return;
  }

     
                                                                           
                                                                       
                
     
  if (z->left == RBTNIL || z->right == RBTNIL)
  {
                                        
    y = z;
  }
  else
  {
                             
    y = z->right;
    while (y->left != RBTNIL)
    {
      y = y->left;
    }
  }

                           
  if (y->left != RBTNIL)
  {
    x = y->left;
  }
  else
  {
    x = y->right;
  }

                               
  x->parent = y->parent;
  if (y->parent)
  {
    if (y == y->parent->left)
    {
      y->parent->left = x;
    }
    else
    {
      y->parent->right = x;
    }
  }
  else
  {
    rbt->root = x;
  }

     
                                                                           
                                                                          
     
  if (y != z)
  {
    rbt_copy_data(rbt, z, y);
  }

     
                                                                           
                                                                             
     
  if (y->color == RBTBLACK)
  {
    rbt_delete_fixup(rbt, x);
  }

                                     
  if (rbt->freefunc)
  {
    rbt->freefunc(y, rbt->arg);
  }
}

   
                                           
   
                                                                        
                                                                      
                                                                      
                                                                   
                                   
   
void
rbt_delete(RBTree *rbt, RBTNode *node)
{
  rbt_delete_node(rbt, node);
}

                                                                        
                               
                                                                        

static RBTNode *
rbt_left_right_iterator(RBTreeIterator *iter)
{
  if (iter->last_visited == NULL)
  {
    iter->last_visited = iter->rbt->root;
    while (iter->last_visited->left != RBTNIL)
    {
      iter->last_visited = iter->last_visited->left;
    }

    return iter->last_visited;
  }

  if (iter->last_visited->right != RBTNIL)
  {
    iter->last_visited = iter->last_visited->right;
    while (iter->last_visited->left != RBTNIL)
    {
      iter->last_visited = iter->last_visited->left;
    }

    return iter->last_visited;
  }

  for (;;)
  {
    RBTNode *came_from = iter->last_visited;

    iter->last_visited = iter->last_visited->parent;
    if (iter->last_visited == NULL)
    {
      iter->is_over = true;
      break;
    }

    if (iter->last_visited->left == came_from)
    {
      break;                                            
                       
    }

                                                              
  }

  return iter->last_visited;
}

static RBTNode *
rbt_right_left_iterator(RBTreeIterator *iter)
{
  if (iter->last_visited == NULL)
  {
    iter->last_visited = iter->rbt->root;
    while (iter->last_visited->right != RBTNIL)
    {
      iter->last_visited = iter->last_visited->right;
    }

    return iter->last_visited;
  }

  if (iter->last_visited->left != RBTNIL)
  {
    iter->last_visited = iter->last_visited->left;
    while (iter->last_visited->right != RBTNIL)
    {
      iter->last_visited = iter->last_visited->right;
    }

    return iter->last_visited;
  }

  for (;;)
  {
    RBTNode *came_from = iter->last_visited;

    iter->last_visited = iter->last_visited->parent;
    if (iter->last_visited == NULL)
    {
      iter->is_over = true;
      break;
    }

    if (iter->last_visited->right == came_from)
    {
      break;                                             
                       
    }

                                                             
  }

  return iter->last_visited;
}

   
                                                                            
   
                                                                         
                                                          
   
                                                                        
                                                                           
                     
   
                                                                         
                                 
   
void
rbt_begin_iterate(RBTree *rbt, RBTOrderControl ctrl, RBTreeIterator *iter)
{
                                                      
  iter->rbt = rbt;
  iter->last_visited = NULL;
  iter->is_over = (rbt->root == RBTNIL);

  switch (ctrl)
  {
  case LeftRightWalk:                                        
    iter->iterate = rbt_left_right_iterator;
    break;
  case RightLeftWalk:                                        
    iter->iterate = rbt_right_left_iterator;
    break;
  default:
    elog(ERROR, "unrecognized rbtree iteration order: %d", ctrl);
  }
}

   
                                                                            
   
RBTNode *
rbt_iterate(RBTreeIterator *iter)
{
  if (iter->is_over)
  {
    return NULL;
  }

  return iter->iterate(iter);
}
