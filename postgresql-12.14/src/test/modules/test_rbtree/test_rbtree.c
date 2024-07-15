                                                                             
   
                 
                                                   
   
                                                                
   
                  
                                               
   
                                                                             
   

#include "postgres.h"

#include "fmgr.h"
#include "lib/rbtree.h"
#include "utils/memutils.h"

PG_MODULE_MAGIC;

   
                                                          
   
typedef struct IntRBTreeNode
{
  RBTNode rbtnode;
  int key;
} IntRBTreeNode;

   
                                                                       
                                             
   
static int
irbt_cmp(const RBTNode *a, const RBTNode *b, void *arg)
{
  const IntRBTreeNode *ea = (const IntRBTreeNode *)a;
  const IntRBTreeNode *eb = (const IntRBTreeNode *)b;

  return ea->key - eb->key;
}

   
                                                                         
                                
   
static void
irbt_combine(RBTNode *existing, const RBTNode *newdata, void *arg)
{
  const IntRBTreeNode *eexist = (const IntRBTreeNode *)existing;
  const IntRBTreeNode *enew = (const IntRBTreeNode *)newdata;

  if (eexist->key != enew->key)
  {
    elog(ERROR, "red-black tree combines %d into %d", enew->key, eexist->key);
  }
}

                    
static RBTNode *
irbt_alloc(void *arg)
{
  return (RBTNode *)palloc(sizeof(IntRBTreeNode));
}

                
static void
irbt_free(RBTNode *node, void *arg)
{
  pfree(node);
}

   
                                                       
   
static RBTree *
create_int_rbtree(void)
{
  return rbt_create(sizeof(IntRBTreeNode), irbt_cmp, irbt_combine, irbt_alloc, irbt_free, NULL);
}

   
                                                           
   
static int *
GetPermutation(int size)
{
  int *permutation;
  int i;

  permutation = (int *)palloc(size * sizeof(int));

  permutation[0] = 0;

     
                                                                             
                                                                             
                                                                         
                                                                          
                                                               
     
  for (i = 1; i < size; i++)
  {
    int j = random() % (i + 1);

    if (j < i)                                           
    {
      permutation[i] = permutation[j];
    }
    permutation[j] = i;
  }

  return permutation;
}

   
                                                                   
                                                                
   
static void
rbt_populate(RBTree *tree, int size, int step)
{
  int *permutation = GetPermutation(size);
  IntRBTreeNode node;
  bool isNew;
  int i;

                                                       
  for (i = 0; i < size; i++)
  {
    node.key = step * permutation[i];
    rbt_insert(tree, (RBTNode *)&node, &isNew);
    if (!isNew)
    {
      elog(ERROR, "unexpected !isNew result from rbt_insert");
    }
  }

     
                                                                         
                                                                          
     
  if (size > 0)
  {
    node.key = step * permutation[0];
    rbt_insert(tree, (RBTNode *)&node, &isNew);
    if (isNew)
    {
      elog(ERROR, "unexpected isNew result from rbt_insert");
    }
  }

  pfree(permutation);
}

   
                                                  
                                                       
                                
   
static void
testleftright(int size)
{
  RBTree *tree = create_int_rbtree();
  IntRBTreeNode *node;
  RBTreeIterator iter;
  int lastKey = -1;
  int count = 0;

                                       
  rbt_begin_iterate(tree, LeftRightWalk, &iter);
  if (rbt_iterate(&iter) != NULL)
  {
    elog(ERROR, "left-right walk over empty tree produced an element");
  }

                                                  
  rbt_populate(tree, size, 1);

                             
  rbt_begin_iterate(tree, LeftRightWalk, &iter);

  while ((node = (IntRBTreeNode *)rbt_iterate(&iter)) != NULL)
  {
                                        
    if (node->key <= lastKey)
    {
      elog(ERROR, "left-right walk gives elements not in sorted order");
    }
    lastKey = node->key;
    count++;
  }

  if (lastKey != size - 1)
  {
    elog(ERROR, "left-right walk did not reach end");
  }
  if (count != size)
  {
    elog(ERROR, "left-right walk missed some elements");
  }
}

   
                                                  
                                                       
                                
   
static void
testrightleft(int size)
{
  RBTree *tree = create_int_rbtree();
  IntRBTreeNode *node;
  RBTreeIterator iter;
  int lastKey = size;
  int count = 0;

                                       
  rbt_begin_iterate(tree, RightLeftWalk, &iter);
  if (rbt_iterate(&iter) != NULL)
  {
    elog(ERROR, "right-left walk over empty tree produced an element");
  }

                                                  
  rbt_populate(tree, size, 1);

                             
  rbt_begin_iterate(tree, RightLeftWalk, &iter);

  while ((node = (IntRBTreeNode *)rbt_iterate(&iter)) != NULL)
  {
                                        
    if (node->key >= lastKey)
    {
      elog(ERROR, "right-left walk gives elements not in sorted order");
    }
    lastKey = node->key;
    count++;
  }

  if (lastKey != 0)
  {
    elog(ERROR, "right-left walk did not reach end");
  }
  if (count != size)
  {
    elog(ERROR, "right-left walk missed some elements");
  }
}

   
                                                                    
                                                     
   
static void
testfind(int size)
{
  RBTree *tree = create_int_rbtree();
  int i;

                                                   
  rbt_populate(tree, size, 2);

                                                     
  for (i = 0; i < size; i++)
  {
    IntRBTreeNode node;
    IntRBTreeNode *resultNode;

    node.key = 2 * i;
    resultNode = (IntRBTreeNode *)rbt_find(tree, (RBTNode *)&node);
    if (resultNode == NULL)
    {
      elog(ERROR, "inserted element was not found");
    }
    if (node.key != resultNode->key)
    {
      elog(ERROR, "find operation in rbtree gave wrong result");
    }
  }

     
                                                                          
                                                         
     
  for (i = -1; i <= 2 * size; i += 2)
  {
    IntRBTreeNode node;
    IntRBTreeNode *resultNode;

    node.key = i;
    resultNode = (IntRBTreeNode *)rbt_find(tree, (RBTNode *)&node);
    if (resultNode != NULL)
    {
      elog(ERROR, "not-inserted element was found");
    }
  }
}

   
                                                        
                                                                         
   
static void
testleftmost(int size)
{
  RBTree *tree = create_int_rbtree();
  IntRBTreeNode *result;

                                                     
  if (rbt_leftmost(tree) != NULL)
  {
    elog(ERROR, "leftmost node of empty tree is not NULL");
  }

                                                  
  rbt_populate(tree, size, 1);

                                                       
  result = (IntRBTreeNode *)rbt_leftmost(tree);
  if (result == NULL || result->key != 0)
  {
    elog(ERROR, "rbt_leftmost gave wrong result");
  }
}

   
                                                      
   
static void
testdelete(int size, int delsize)
{
  RBTree *tree = create_int_rbtree();
  int *deleteIds;
  bool *chosen;
  int i;

                                                  
  rbt_populate(tree, size, 1);

                                   
  deleteIds = (int *)palloc(delsize * sizeof(int));
  chosen = (bool *)palloc0(size * sizeof(bool));

  for (i = 0; i < delsize; i++)
  {
    int k = random() % size;

    while (chosen[k])
    {
      k = (k + 1) % size;
    }
    deleteIds[i] = k;
    chosen[k] = true;
  }

                       
  for (i = 0; i < delsize; i++)
  {
    IntRBTreeNode find;
    IntRBTreeNode *node;

    find.key = deleteIds[i];
                                       
    node = (IntRBTreeNode *)rbt_find(tree, (RBTNode *)&find);
    if (node == NULL || node->key != deleteIds[i])
    {
      elog(ERROR, "expected element was not found during deleting");
    }
                   
    rbt_delete(tree, (RBTNode *)node);
  }

                                               
  for (i = 0; i < size; i++)
  {
    IntRBTreeNode node;
    IntRBTreeNode *result;

    node.key = i;
    result = (IntRBTreeNode *)rbt_find(tree, (RBTNode *)&node);
    if (chosen[i])
    {
                                            
      if (result != NULL)
      {
        elog(ERROR, "deleted element still present in the rbtree");
      }
    }
    else
    {
                                     
      if (result == NULL || result->key != i)
      {
        elog(ERROR, "delete operation removed wrong rbtree value");
      }
    }
  }

                                                                           
  for (i = 0; i < size; i++)
  {
    IntRBTreeNode find;
    IntRBTreeNode *node;

    if (chosen[i])
    {
      continue;
    }
    find.key = i;
                                       
    node = (IntRBTreeNode *)rbt_find(tree, (RBTNode *)&find);
    if (node == NULL || node->key != i)
    {
      elog(ERROR, "expected element was not found during deleting");
    }
                   
    rbt_delete(tree, (RBTNode *)node);
  }

                                
  if (rbt_leftmost(tree) != NULL)
  {
    elog(ERROR, "deleting all elements failed");
  }

  pfree(deleteIds);
  pfree(chosen);
}

   
                                                 
   
                                                         
   
PG_FUNCTION_INFO_V1(test_rb_tree);

Datum
test_rb_tree(PG_FUNCTION_ARGS)
{
  int size = PG_GETARG_INT32(0);

  if (size <= 0 || size > MaxAllocSize / sizeof(int))
  {
    elog(ERROR, "invalid size for test_rb_tree: %d", size);
  }
  testleftright(size);
  testrightleft(size);
  testfind(size);
  testleftmost(size);
  testdelete(size, Max(size / 10, 1));
  PG_RETURN_VOID();
}
