                                                                            
   
                     
                                                  
                                       
   
                                                                         
   
   
                  
                                             
   
                                                                            
   

#include "postgres.h"

#include "tsearch/ts_utils.h"
#include "miscadmin.h"

typedef struct NODE
{
  struct NODE *left;
  struct NODE *right;
  QueryItem *valnode;
} NODE;

   
                                            
   
static NODE *
maketree(QueryItem *in)
{
  NODE *node = (NODE *)palloc(sizeof(NODE));

                                                                           
  check_stack_depth();

  node->valnode = in;
  node->right = node->left = NULL;
  if (in->type == QI_OPR)
  {
    node->right = maketree(in + 1);
    if (in->qoperator.oper != OP_NOT)
    {
      node->left = maketree(in + in->qoperator.left);
    }
  }
  return node;
}

   
                                              
   
typedef struct
{
  QueryItem *ptr;
  int len;                            
  int cur;                                
} PLAINTREE;

static void
plainnode(PLAINTREE *state, NODE *node)
{
                                                                           
  check_stack_depth();

  if (state->cur == state->len)
  {
    state->len *= 2;
    state->ptr = (QueryItem *)repalloc((void *)state->ptr, state->len * sizeof(QueryItem));
  }
  memcpy((void *)&(state->ptr[state->cur]), (void *)node->valnode, sizeof(QueryItem));
  if (node->valnode->type == QI_VAL)
  {
    state->cur++;
  }
  else if (node->valnode->qoperator.oper == OP_NOT)
  {
    state->ptr[state->cur].qoperator.left = 1;
    state->cur++;
    plainnode(state, node->right);
  }
  else
  {
    int cur = state->cur;

    state->cur++;
    plainnode(state, node->right);
    state->ptr[cur].qoperator.left = state->cur - cur;
    plainnode(state, node->left);
  }
  pfree(node);
}

   
                                                           
   
static QueryItem *
plaintree(NODE *root, int *len)
{
  PLAINTREE pl;

  pl.cur = 0;
  pl.len = 16;
  if (root && (root->valnode->type == QI_VAL || root->valnode->type == QI_OPR))
  {
    pl.ptr = (QueryItem *)palloc(pl.len * sizeof(QueryItem));
    plainnode(&pl, root);
  }
  else
  {
    pl.ptr = NULL;
  }
  *len = pl.cur;
  return pl.ptr;
}

static void
freetree(NODE *node)
{
                                                                           
  check_stack_depth();

  if (!node)
  {
    return;
  }
  if (node->left)
  {
    freetree(node->left);
  }
  if (node->right)
  {
    freetree(node->right);
  }
  pfree(node);
}

   
                              
                                 
                                                       
                                 
   
static NODE *
clean_NOT_intree(NODE *node)
{
                                                                           
  check_stack_depth();

  if (node->valnode->type == QI_VAL)
  {
    return node;
  }

  if (node->valnode->qoperator.oper == OP_NOT)
  {
    freetree(node);
    return NULL;
  }

                       
  if (node->valnode->qoperator.oper == OP_OR)
  {
    if ((node->left = clean_NOT_intree(node->left)) == NULL || (node->right = clean_NOT_intree(node->right)) == NULL)
    {
      freetree(node);
      return NULL;
    }
  }
  else
  {
    NODE *res = node;

    Assert(node->valnode->qoperator.oper == OP_AND || node->valnode->qoperator.oper == OP_PHRASE);

    node->left = clean_NOT_intree(node->left);
    node->right = clean_NOT_intree(node->right);
    if (node->left == NULL && node->right == NULL)
    {
      pfree(node);
      res = NULL;
    }
    else if (node->left == NULL)
    {
      res = node->right;
      pfree(node);
    }
    else if (node->right == NULL)
    {
      res = node->left;
      pfree(node);
    }
    return res;
  }
  return node;
}

QueryItem *
clean_NOT(QueryItem *ptr, int *len)
{
  NODE *root = maketree(ptr);

  return plaintree(clean_NOT_intree(root), len);
}

   
                                                       
   
                                                                              
   
                                                                       
                                                                      
                                                  
                                           
                                           
                                                   
                                                   
                                                    
                                                                      
                                                                       
                                                                             
                                                
                                            
                                                                              
                                                                            
                                                                             
                                                             
                            
                                                                        
                                                                               
                                                                               
                                                                            
                                                                               
                                                                             
   
                                                                               
                                                                               
                                                                              
                                                                              
                                                                              
                                                                         
                                                                  
                                                  
   
static NODE *
clean_stopword_intree(NODE *node, int *ladd, int *radd)
{
                                                                           
  check_stack_depth();

                                                                       
  *ladd = *radd = 0;

  if (node->valnode->type == QI_VAL)
  {
    return node;
  }
  else if (node->valnode->type == QI_VALSTOP)
  {
    pfree(node);
    return NULL;
  }

  Assert(node->valnode->type == QI_OPR);

  if (node->valnode->qoperator.oper == OP_NOT)
  {
                                                                          
    node->right = clean_stopword_intree(node->right, ladd, radd);
    if (!node->right)
    {
      freetree(node);
      return NULL;
    }
  }
  else
  {
    NODE *res = node;
    bool isphrase;
    int ndistance, lladd, lradd, rladd, rradd;

                        
    node->left = clean_stopword_intree(node->left, &lladd, &lradd);
    node->right = clean_stopword_intree(node->right, &rladd, &rradd);

                                                              
    isphrase = (node->valnode->qoperator.oper == OP_PHRASE);
    ndistance = isphrase ? node->valnode->qoperator.distance : 0;

    if (node->left == NULL && node->right == NULL)
    {
         
                                                                        
                                                                         
                                                                    
                                                                        
                                                                         
                                                                        
                                                                
                                                                       
         
      if (isphrase)
      {
        *ladd = *radd = lladd + ndistance + rladd;
      }
      else
      {
        *ladd = *radd = Max(lladd, rladd);
      }
      freetree(node);
      return NULL;
    }
    else if (node->left == NULL)
    {
                                                   
                                                                 
      if (isphrase)
      {
                                                            
        *ladd = lladd + ndistance + rladd;
        *radd = rradd;
      }
      else
      {
                                                                     
        *ladd = rladd;
        *radd = rradd;
      }
      res = node->right;
      pfree(node);
    }
    else if (node->right == NULL)
    {
                                                    
                                                                 
      if (isphrase)
      {
                                                             
        *ladd = lladd;
        *radd = lradd + ndistance + rradd;
      }
      else
      {
                                                                      
        *ladd = lladd;
        *radd = lradd;
      }
      res = node->left;
      pfree(node);
    }
    else if (isphrase)
    {
                                                        
      node->valnode->qoperator.distance += lradd + rladd;
                                                        
      *ladd = lladd;
      *radd = rradd;
    }
    else
    {
                                                                      
    }

    return res;
  }
  return node;
}

   
                                    
   
static int32
calcstrlen(NODE *node)
{
  int32 size = 0;

  if (node->valnode->type == QI_VAL)
  {
    size = node->valnode->qoperand.length + 1;
  }
  else
  {
    Assert(node->valnode->type == QI_OPR);

    size = calcstrlen(node->right);
    if (node->valnode->qoperator.oper != OP_NOT)
    {
      size += calcstrlen(node->left);
    }
  }

  return size;
}

   
                                                    
   
TSQuery
cleanup_tsquery_stopwords(TSQuery in)
{
  int32 len, lenstr, commonlen, i;
  NODE *root;
  int ladd, radd;
  TSQuery out;
  QueryItem *items;
  char *operands;

  if (in->size == 0)
  {
    return in;
  }

                            
  root = clean_stopword_intree(maketree(GETQUERY(in)), &ladd, &radd);
  if (root == NULL)
  {
    ereport(NOTICE, (errmsg("text-search query contains only stop words or doesn't contain lexemes, ignored")));
    out = palloc(HDRSIZETQ);
    out->size = 0;
    SET_VARSIZE(out, HDRSIZETQ);
    return out;
  }

     
                                   
     

  lenstr = calcstrlen(root);
  items = plaintree(root, &len);
  commonlen = COMPUTESIZE(len, lenstr);

  out = palloc(commonlen);
  SET_VARSIZE(out, commonlen);
  out->size = len;

  memcpy(GETQUERY(out), items, len * sizeof(QueryItem));

  items = GETQUERY(out);
  operands = GETOPERAND(out);
  for (i = 0; i < out->size; i++)
  {
    QueryOperand *op = (QueryOperand *)&items[i];

    if (op->type != QI_VAL)
    {
      continue;
    }

    memcpy(operands, GETOPERAND(in) + op->distance, op->length);
    operands[op->length] = '\0';
    op->distance = operands - GETOPERAND(out);
    operands += op->length + 1;
  }

  return out;
}
