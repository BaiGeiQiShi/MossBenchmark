                                                                            
   
              
                                                                         
                  
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/pathnodes.h"
#include "optimizer/optimizer.h"
#include "utils/array.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

   
                                                                        
                                                                       
                                                                     
                                                       
                                                
   
#define MAX_SAOP_ARRAY_SIZE 100

   
                                                                 
                                                                       
                                                                           
                                                                           
                                                                             
                                            
   
typedef enum
{
  CLASS_ATOM,                                      
  CLASS_AND,                                     
  CLASS_OR                                      
} PredClass;

typedef struct PredIterInfoData *PredIterInfo;

typedef struct PredIterInfoData
{
                                          
  void *state;
                                      
  void (*startup_fn)(Node *clause, PredIterInfo info);
                                         
  Node *(*next_fn)(PredIterInfo info);
                                                  
  void (*cleanup_fn)(PredIterInfo info);
} PredIterInfoData;

#define iterate_begin(item, clause, info)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    Node *item;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    (info).startup_fn((clause), &(info));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
    while ((item = (info).next_fn(&(info))) != NULL)

#define iterate_end(info)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              \
  (info).cleanup_fn(&(info));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  while (0)

static bool
predicate_implied_by_recurse(Node *clause, Node *predicate, bool weak);
static bool
predicate_refuted_by_recurse(Node *clause, Node *predicate, bool weak);
static PredClass
predicate_classify(Node *clause, PredIterInfo info);
static void
list_startup_fn(Node *clause, PredIterInfo info);
static Node *
list_next_fn(PredIterInfo info);
static void
list_cleanup_fn(PredIterInfo info);
static void
boolexpr_startup_fn(Node *clause, PredIterInfo info);
static void
arrayconst_startup_fn(Node *clause, PredIterInfo info);
static Node *
arrayconst_next_fn(PredIterInfo info);
static void
arrayconst_cleanup_fn(PredIterInfo info);
static void
arrayexpr_startup_fn(Node *clause, PredIterInfo info);
static Node *
arrayexpr_next_fn(PredIterInfo info);
static void
arrayexpr_cleanup_fn(PredIterInfo info);
static bool
predicate_implied_by_simple_clause(Expr *predicate, Node *clause, bool weak);
static bool
predicate_refuted_by_simple_clause(Expr *predicate, Node *clause, bool weak);
static Node *
extract_not_arg(Node *clause);
static Node *
extract_strong_not_arg(Node *clause);
static bool
clause_is_strict_for(Node *clause, Node *subexpr, bool allow_false);
static bool
operator_predicate_proof(Expr *predicate, Node *clause, bool refute_it, bool weak);
static bool
operator_same_subexprs_proof(Oid pred_op, Oid clause_op, bool refute_it);
static bool
operator_same_subexprs_lookup(Oid pred_op, Oid clause_op, bool refute_it);
static Oid
get_btree_test_op(Oid pred_op, Oid clause_op, bool refute_it);
static void
InvalidateOprProofCacheCallBack(Datum arg, int cacheid, uint32 hashvalue);

   
                        
                                                                          
                              
   
                                              
   
                                                                               
                                                                        
                                       
   
                                                                       
                                                                               
                                                                              
   
                                                                              
                                                                               
                                                                               
                                                                             
                      
   
                                                                         
                                                                         
                                                                            
                                                           
                                                                     
                                                
   
                                                                    
                                                                           
                                                                            
                                                                             
                                                                          
                                                                               
                                                                               
   
bool
predicate_implied_by(List *predicate_list, List *clause_list, bool weak)
{
  Node *p, *c;

  if (predicate_list == NIL)
  {
    return true;                                           
  }
  if (clause_list == NIL)
  {
    return false;                                            
  }

     
                                                                        
                                                                           
                                                                           
                                                            
     
  if (list_length(predicate_list) == 1)
  {
    p = (Node *)linitial(predicate_list);
  }
  else
  {
    p = (Node *)predicate_list;
  }
  if (list_length(clause_list) == 1)
  {
    c = (Node *)linitial(clause_list);
  }
  else
  {
    c = (Node *)clause_list;
  }

                          
  return predicate_implied_by_recurse(c, p, weak);
}

   
                        
                                                                            
                                          
   
                                                                         
                                               
   
                                             
   
                                                                           
                                                                          
                                                                          
                                                                           
   
                                                                          
                                                                       
                                
   
                                                                               
                                                                          
                                                                             
                                                                         
                                                                           
                               
   
                                                                         
                                                                         
                                                                            
                                                           
                                                                     
                                                
   
                                                                    
                                                                            
                                                                          
                                                           
                                                                               
   
bool
predicate_refuted_by(List *predicate_list, List *clause_list, bool weak)
{
  Node *p, *c;

  if (predicate_list == NIL)
  {
    return false;                                              
  }
  if (clause_list == NIL)
  {
    return false;                                           
  }

     
                                                                        
                                                                           
                                                                           
                                                            
     
  if (list_length(predicate_list) == 1)
  {
    p = (Node *)linitial(predicate_list);
  }
  else
  {
    p = (Node *)predicate_list;
  }
  if (list_length(clause_list) == 1)
  {
    c = (Node *)linitial(clause_list);
  }
  else
  {
    c = (Node *)clause_list;
  }

                          
  return predicate_refuted_by_recurse(c, p, weak);
}

             
                                
                                                                      
                        
   
                                                      
                                                                      
                                                          
                                                        
                                                         
                                                             
                                                            
                                           
                                                         
                                                            
                                                                     
   
                                                                             
                                                                             
                                                                      
   
                                                                   
   
                                                                         
                                                                         
             
                              
                                 
                                  
                                 
                                                                          
                                                                     
   
                                                                           
                                           
             
   
static bool
predicate_implied_by_recurse(Node *clause, Node *predicate, bool weak)
{
  PredIterInfoData clause_info;
  PredIterInfoData pred_info;
  PredClass pclass;
  bool result;

                                 
  Assert(clause != NULL);
  if (IsA(clause, RestrictInfo))
  {
    clause = (Node *)((RestrictInfo *)clause)->clause;
  }

  pclass = predicate_classify(predicate, &pred_info);

  switch (predicate_classify(clause, &clause_info))
  {
  case CLASS_AND:
    switch (pclass)
    {
    case CLASS_AND:

         
                                                                 
         
      result = true;
      iterate_begin(pitem, predicate, pred_info)
      {
        if (!predicate_implied_by_recurse(clause, pitem, weak))
        {
          result = false;
          break;
        }
      }
      iterate_end(pred_info);
      return result;

    case CLASS_OR:

         
                                                               
         
                                                        
         
      result = false;
      iterate_begin(pitem, predicate, pred_info)
      {
        if (predicate_implied_by_recurse(clause, pitem, weak))
        {
          result = true;
          break;
        }
      }
      iterate_end(pred_info);
      if (result)
      {
        return result;
      }

         
                                                  
         
                                                       
         
      iterate_begin(citem, clause, clause_info)
      {
        if (predicate_implied_by_recurse(citem, predicate, weak))
        {
          result = true;
          break;
        }
      }
      iterate_end(clause_info);
      return result;

    case CLASS_ATOM:

         
                                                          
         
      result = false;
      iterate_begin(citem, clause, clause_info)
      {
        if (predicate_implied_by_recurse(citem, predicate, weak))
        {
          result = true;
          break;
        }
      }
      iterate_end(clause_info);
      return result;
    }
    break;

  case CLASS_OR:
    switch (pclass)
    {
    case CLASS_OR:

         
                                                                 
                                                               
         
      result = true;
      iterate_begin(citem, clause, clause_info)
      {
        bool presult = false;

        iterate_begin(pitem, predicate, pred_info)
        {
          if (predicate_implied_by_recurse(citem, pitem, weak))
          {
            presult = true;
            break;
          }
        }
        iterate_end(pred_info);
        if (!presult)
        {
          result = false;                               
          break;
        }
      }
      iterate_end(clause_info);
      return result;

    case CLASS_AND:
    case CLASS_ATOM:

         
                                                                
         
                                                          
         
      result = true;
      iterate_begin(citem, clause, clause_info)
      {
        if (!predicate_implied_by_recurse(citem, predicate, weak))
        {
          result = false;
          break;
        }
      }
      iterate_end(clause_info);
      return result;
    }
    break;

  case CLASS_ATOM:
    switch (pclass)
    {
    case CLASS_AND:

         
                                                           
         
      result = true;
      iterate_begin(pitem, predicate, pred_info)
      {
        if (!predicate_implied_by_recurse(clause, pitem, weak))
        {
          result = false;
          break;
        }
      }
      iterate_end(pred_info);
      return result;

    case CLASS_OR:

         
                                                         
         
      result = false;
      iterate_begin(pitem, predicate, pred_info)
      {
        if (predicate_implied_by_recurse(clause, pitem, weak))
        {
          result = true;
          break;
        }
      }
      iterate_end(pred_info);
      return result;

    case CLASS_ATOM:

         
                                       
         
      return predicate_implied_by_simple_clause((Expr *)predicate, clause, weak);
    }
    break;
  }

                      
  elog(ERROR, "predicate_classify returned a bogus value");
  return false;
}

             
                                
                                                                     
                        
   
                                                       
                                                                       
                                                           
                                                           
                                                           
                                                               
                                            
                                                              
                                                           
                                                                       
                                                             
   
                                                                      
   
                                                                 
                              
                                                                             
                                                                               
                                                                           
                              
                                                                         
                                                                            
   
                                                             
             
   
static bool
predicate_refuted_by_recurse(Node *clause, Node *predicate, bool weak)
{
  PredIterInfoData clause_info;
  PredIterInfoData pred_info;
  PredClass pclass;
  Node *not_arg;
  bool result;

                                 
  Assert(clause != NULL);
  if (IsA(clause, RestrictInfo))
  {
    clause = (Node *)((RestrictInfo *)clause)->clause;
  }

  pclass = predicate_classify(predicate, &pred_info);

  switch (predicate_classify(clause, &clause_info))
  {
  case CLASS_AND:
    switch (pclass)
    {
    case CLASS_AND:

         
                                                                 
         
                                                           
         
      result = false;
      iterate_begin(pitem, predicate, pred_info)
      {
        if (predicate_refuted_by_recurse(clause, pitem, weak))
        {
          result = true;
          break;
        }
      }
      iterate_end(pred_info);
      if (result)
      {
        return result;
      }

         
                                                  
         
                                                           
         
      iterate_begin(citem, clause, clause_info)
      {
        if (predicate_refuted_by_recurse(citem, predicate, weak))
        {
          result = true;
          break;
        }
      }
      iterate_end(clause_info);
      return result;

    case CLASS_OR:

         
                                                                 
         
      result = true;
      iterate_begin(pitem, predicate, pred_info)
      {
        if (!predicate_refuted_by_recurse(clause, pitem, weak))
        {
          result = false;
          break;
        }
      }
      iterate_end(pred_info);
      return result;

    case CLASS_ATOM:

         
                                                            
         
                                                               
                                                              
                                                                
                                                                 
                                                             
                                                        
         
      not_arg = extract_not_arg(predicate);
      if (not_arg && predicate_implied_by_recurse(clause, not_arg, false))
      {
        return true;
      }

         
                                                           
         
      result = false;
      iterate_begin(citem, clause, clause_info)
      {
        if (predicate_refuted_by_recurse(citem, predicate, weak))
        {
          result = true;
          break;
        }
      }
      iterate_end(clause_info);
      return result;
    }
    break;

  case CLASS_OR:
    switch (pclass)
    {
    case CLASS_OR:

         
                                                                
         
      result = true;
      iterate_begin(pitem, predicate, pred_info)
      {
        if (!predicate_refuted_by_recurse(clause, pitem, weak))
        {
          result = false;
          break;
        }
      }
      iterate_end(pred_info);
      return result;

    case CLASS_AND:

         
                                                               
                           
         
      result = true;
      iterate_begin(citem, clause, clause_info)
      {
        bool presult = false;

        iterate_begin(pitem, predicate, pred_info)
        {
          if (predicate_refuted_by_recurse(citem, pitem, weak))
          {
            presult = true;
            break;
          }
        }
        iterate_end(pred_info);
        if (!presult)
        {
          result = false;                            
          break;
        }
      }
      iterate_end(clause_info);
      return result;

    case CLASS_ATOM:

         
                                                            
         
                                                      
         
      not_arg = extract_not_arg(predicate);
      if (not_arg && predicate_implied_by_recurse(clause, not_arg, false))
      {
        return true;
      }

         
                                                           
         
      result = true;
      iterate_begin(citem, clause, clause_info)
      {
        if (!predicate_refuted_by_recurse(citem, predicate, weak))
        {
          result = false;
          break;
        }
      }
      iterate_end(clause_info);
      return result;
    }
    break;

  case CLASS_ATOM:

       
                                                            
       
                                                                   
                                                                      
                                                                 
                                                                     
                             
       
    not_arg = extract_strong_not_arg(clause);
    if (not_arg && predicate_implied_by_recurse(predicate, not_arg, !weak))
    {
      return true;
    }

    switch (pclass)
    {
    case CLASS_AND:

         
                                                           
         
      result = false;
      iterate_begin(pitem, predicate, pred_info)
      {
        if (predicate_refuted_by_recurse(clause, pitem, weak))
        {
          result = true;
          break;
        }
      }
      iterate_end(pred_info);
      return result;

    case CLASS_OR:

         
                                                           
         
      result = true;
      iterate_begin(pitem, predicate, pred_info)
      {
        if (!predicate_refuted_by_recurse(clause, pitem, weak))
        {
          result = false;
          break;
        }
      }
      iterate_end(pred_info);
      return result;

    case CLASS_ATOM:

         
                                                            
         
                                                      
         
      not_arg = extract_not_arg(predicate);
      if (not_arg && predicate_implied_by_recurse(clause, not_arg, false))
      {
        return true;
      }

         
                                        
         
      return predicate_refuted_by_simple_clause((Expr *)predicate, clause, weak);
    }
    break;
  }

                      
  elog(ERROR, "predicate_classify returned a bogus value");
  return false;
}

   
                      
                                                                             
   
                                                                            
                                                                
   
                                                                          
                                                                              
                                                                           
                                                                                
                                                                               
                                                                             
   
static PredClass
predicate_classify(Node *clause, PredIterInfo info)
{
                                                                 
  Assert(clause != NULL);
  Assert(!IsA(clause, RestrictInfo));

     
                                                                             
                                                
     
  if (IsA(clause, List))
  {
    info->startup_fn = list_startup_fn;
    info->next_fn = list_next_fn;
    info->cleanup_fn = list_cleanup_fn;
    return CLASS_AND;
  }

                                                
  if (is_andclause(clause))
  {
    info->startup_fn = boolexpr_startup_fn;
    info->next_fn = list_next_fn;
    info->cleanup_fn = list_cleanup_fn;
    return CLASS_AND;
  }
  if (is_orclause(clause))
  {
    info->startup_fn = boolexpr_startup_fn;
    info->next_fn = list_next_fn;
    info->cleanup_fn = list_cleanup_fn;
    return CLASS_OR;
  }

                                
  if (IsA(clause, ScalarArrayOpExpr))
  {
    ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *)clause;
    Node *arraynode = (Node *)lsecond(saop->args);

       
                                                                          
                                                                         
                                                                       
                         
       
    if (arraynode && IsA(arraynode, Const) && !((Const *)arraynode)->constisnull)
    {
      ArrayType *arrayval;
      int nelems;

      arrayval = DatumGetArrayTypeP(((Const *)arraynode)->constvalue);
      nelems = ArrayGetNItems(ARR_NDIM(arrayval), ARR_DIMS(arrayval));
      if (nelems <= MAX_SAOP_ARRAY_SIZE)
      {
        info->startup_fn = arrayconst_startup_fn;
        info->next_fn = arrayconst_next_fn;
        info->cleanup_fn = arrayconst_cleanup_fn;
        return saop->useOr ? CLASS_OR : CLASS_AND;
      }
    }
    else if (arraynode && IsA(arraynode, ArrayExpr) && !((ArrayExpr *)arraynode)->multidims && list_length(((ArrayExpr *)arraynode)->elements) <= MAX_SAOP_ARRAY_SIZE)
    {
      info->startup_fn = arrayexpr_startup_fn;
      info->next_fn = arrayexpr_next_fn;
      info->cleanup_fn = arrayexpr_cleanup_fn;
      return saop->useOr ? CLASS_OR : CLASS_AND;
    }
  }

                                          
  return CLASS_ATOM;
}

   
                                                                          
                                                 
   
static void
list_startup_fn(Node *clause, PredIterInfo info)
{
  info->state = (void *)list_head((List *)clause);
}

static Node *
list_next_fn(PredIterInfo info)
{
  ListCell *l = (ListCell *)info->state;
  Node *n;

  if (l == NULL)
  {
    return NULL;
  }
  n = lfirst(l);
  info->state = (void *)lnext(l);
  return n;
}

static void
list_cleanup_fn(PredIterInfo info)
{
                           
}

   
                                                                         
                    
   
static void
boolexpr_startup_fn(Node *clause, PredIterInfo info)
{
  info->state = (void *)list_head(((BoolExpr *)clause)->args);
}

   
                                                                       
                           
   
typedef struct
{
  OpExpr opexpr;
  Const constexpr;
  int next_elem;
  int num_elems;
  Datum *elem_values;
  bool *elem_nulls;
} ArrayConstIterState;

static void
arrayconst_startup_fn(Node *clause, PredIterInfo info)
{
  ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *)clause;
  ArrayConstIterState *state;
  Const *arrayconst;
  ArrayType *arrayval;
  int16 elmlen;
  bool elmbyval;
  char elmalign;

                                   
  state = (ArrayConstIterState *)palloc(sizeof(ArrayConstIterState));
  info->state = (void *)state;

                                     
  arrayconst = (Const *)lsecond(saop->args);
  arrayval = DatumGetArrayTypeP(arrayconst->constvalue);
  get_typlenbyvalalign(ARR_ELEMTYPE(arrayval), &elmlen, &elmbyval, &elmalign);
  deconstruct_array(arrayval, ARR_ELEMTYPE(arrayval), elmlen, elmbyval, elmalign, &state->elem_values, &state->elem_nulls, &state->num_elems);

                                                            
  state->opexpr.xpr.type = T_OpExpr;
  state->opexpr.opno = saop->opno;
  state->opexpr.opfuncid = saop->opfuncid;
  state->opexpr.opresulttype = BOOLOID;
  state->opexpr.opretset = false;
  state->opexpr.opcollid = InvalidOid;
  state->opexpr.inputcollid = saop->inputcollid;
  state->opexpr.args = list_copy(saop->args);

                                                                
  state->constexpr.xpr.type = T_Const;
  state->constexpr.consttype = ARR_ELEMTYPE(arrayval);
  state->constexpr.consttypmod = -1;
  state->constexpr.constcollid = arrayconst->constcollid;
  state->constexpr.constlen = elmlen;
  state->constexpr.constbyval = elmbyval;
  lsecond(state->opexpr.args) = &state->constexpr;

                                  
  state->next_elem = 0;
}

static Node *
arrayconst_next_fn(PredIterInfo info)
{
  ArrayConstIterState *state = (ArrayConstIterState *)info->state;

  if (state->next_elem >= state->num_elems)
  {
    return NULL;
  }
  state->constexpr.constvalue = state->elem_values[state->next_elem];
  state->constexpr.constisnull = state->elem_nulls[state->next_elem];
  state->next_elem++;
  return (Node *)&(state->opexpr);
}

static void
arrayconst_cleanup_fn(PredIterInfo info)
{
  ArrayConstIterState *state = (ArrayConstIterState *)info->state;

  pfree(state->elem_values);
  pfree(state->elem_nulls);
  list_free(state->opexpr.args);
  pfree(state);
}

   
                                                                       
                                            
   
typedef struct
{
  OpExpr opexpr;
  ListCell *next;
} ArrayExprIterState;

static void
arrayexpr_startup_fn(Node *clause, PredIterInfo info)
{
  ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *)clause;
  ArrayExprIterState *state;
  ArrayExpr *arrayexpr;

                                   
  state = (ArrayExprIterState *)palloc(sizeof(ArrayExprIterState));
  info->state = (void *)state;

                                                            
  state->opexpr.xpr.type = T_OpExpr;
  state->opexpr.opno = saop->opno;
  state->opexpr.opfuncid = saop->opfuncid;
  state->opexpr.opresulttype = BOOLOID;
  state->opexpr.opretset = false;
  state->opexpr.opcollid = InvalidOid;
  state->opexpr.inputcollid = saop->inputcollid;
  state->opexpr.args = list_copy(saop->args);

                                                                  
  arrayexpr = (ArrayExpr *)lsecond(saop->args);
  state->next = list_head(arrayexpr->elements);
}

static Node *
arrayexpr_next_fn(PredIterInfo info)
{
  ArrayExprIterState *state = (ArrayExprIterState *)info->state;

  if (state->next == NULL)
  {
    return NULL;
  }
  lsecond(state->opexpr.args) = lfirst(state->next);
  state->next = lnext(state->next);
  return (Node *)&(state->opexpr);
}

static void
arrayexpr_cleanup_fn(PredIterInfo info)
{
  ArrayExprIterState *state = (ArrayExprIterState *)info->state;

  list_free(state->opexpr.args);
  pfree(state);
}

             
                                      
                                                                         
                                        
   
                                                                  
   
                                                                      
                    
   
                                                                              
                                                                          
                                                                           
                                                                        
   
                                                                             
                                                                            
                                                                            
                                                                             
                                                                       
                                                   
   
                                                                            
                                                                          
                                                               
             
   
static bool
predicate_implied_by_simple_clause(Expr *predicate, Node *clause, bool weak)
{
                                              
  CHECK_FOR_INTERRUPTS();

                                  
  if (equal((Node *)predicate, clause))
  {
    return true;
  }

                                     
  if (!weak && predicate && IsA(predicate, NullTest))
  {
    NullTest *ntest = (NullTest *)predicate;

                                                                        
    if (ntest->nulltesttype == IS_NOT_NULL && !ntest->argisrow)
    {
                                                                
      if (clause_is_strict_for(clause, (Node *)ntest->arg, true))
      {
        return true;
      }
    }
    return false;                                
  }

                                           
  return operator_predicate_proof(predicate, clause, false, weak);
}

             
                                      
                                                                        
                                        
   
                                                                 
   
                                                                            
                                                                              
                                                                            
                                                                  
   
                                                                         
                                                                             
                                                                             
                       
   
                                                                              
                                                                           
                                                                          
                                                                     
   
                                                                             
                                                                 
   
                                                                            
                                                                          
                                                               
             
   
static bool
predicate_refuted_by_simple_clause(Expr *predicate, Node *clause, bool weak)
{
                                              
  CHECK_FOR_INTERRUPTS();

                                           
                                                                    
  if ((Node *)predicate == clause)
  {
    return false;
  }

                                      
  if (predicate && IsA(predicate, NullTest) && ((NullTest *)predicate)->nulltesttype == IS_NULL)
  {
    Expr *isnullarg = ((NullTest *)predicate)->arg;

                                                                    
    if (((NullTest *)predicate)->argisrow)
    {
      return false;
    }

                                                          
    if (clause_is_strict_for(clause, (Node *)isnullarg, true))
    {
      return true;
    }

                                             
    if (clause && IsA(clause, NullTest) && ((NullTest *)clause)->nulltesttype == IS_NOT_NULL && !((NullTest *)clause)->argisrow && equal(((NullTest *)clause)->arg, isnullarg))
    {
      return true;
    }

    return false;                                
  }

                                   
  if (clause && IsA(clause, NullTest) && ((NullTest *)clause)->nulltesttype == IS_NULL)
  {
    Expr *isnullarg = ((NullTest *)clause)->arg;

                                                                    
    if (((NullTest *)clause)->argisrow)
    {
      return false;
    }

                                             
    if (predicate && IsA(predicate, NullTest) && ((NullTest *)predicate)->nulltesttype == IS_NOT_NULL && !((NullTest *)predicate)->argisrow && equal(((NullTest *)predicate)->arg, isnullarg))
    {
      return true;
    }

                                                                         
    if (weak && clause_is_strict_for((Node *)predicate, (Node *)isnullarg, true))
    {
      return true;
    }

    return false;                                
  }

                                           
  return operator_predicate_proof(predicate, clause, true, weak);
}

   
                                                                          
                          
   
static Node *
extract_not_arg(Node *clause)
{
  if (clause == NULL)
  {
    return NULL;
  }
  if (IsA(clause, BoolExpr))
  {
    BoolExpr *bexpr = (BoolExpr *)clause;

    if (bexpr->boolop == NOT_EXPR)
    {
      return (Node *)linitial(bexpr->args);
    }
  }
  else if (IsA(clause, BooleanTest))
  {
    BooleanTest *btest = (BooleanTest *)clause;

    if (btest->booltesttype == IS_NOT_TRUE || btest->booltesttype == IS_FALSE || btest->booltesttype == IS_UNKNOWN)
    {
      return (Node *)btest->arg;
    }
  }
  return NULL;
}

   
                                                                        
                          
   
static Node *
extract_strong_not_arg(Node *clause)
{
  if (clause == NULL)
  {
    return NULL;
  }
  if (IsA(clause, BoolExpr))
  {
    BoolExpr *bexpr = (BoolExpr *)clause;

    if (bexpr->boolop == NOT_EXPR)
    {
      return (Node *)linitial(bexpr->args);
    }
  }
  else if (IsA(clause, BooleanTest))
  {
    BooleanTest *btest = (BooleanTest *)clause;

    if (btest->booltesttype == IS_FALSE)
    {
      return (Node *)btest->arg;
    }
  }
  return NULL;
}

   
                                                                      
                          
   
                                                                          
                                                                             
                                                                            
                                                                              
                                                                           
                                                                     
                                                                              
                                   
   
                                                                             
                                                                            
                                                                            
                                       
   
                                                         
   
                                                                        
                                                                           
   
static bool
clause_is_strict_for(Node *clause, Node *subexpr, bool allow_false)
{
  ListCell *lc;

                     
  if (clause == NULL || subexpr == NULL)
  {
    return false;
  }

     
                                                                    
                                                                            
                                                                           
                                    
     
  if (IsA(clause, RelabelType))
  {
    clause = (Node *)((RelabelType *)clause)->arg;
  }
  if (IsA(subexpr, RelabelType))
  {
    subexpr = (Node *)((RelabelType *)subexpr)->arg;
  }

                 
  if (equal(clause, subexpr))
  {
    return true;
  }

     
                                                                           
                                                                           
                                                                        
     
  if (is_opclause(clause) && op_strict(((OpExpr *)clause)->opno))
  {
    foreach (lc, ((OpExpr *)clause)->args)
    {
      if (clause_is_strict_for((Node *)lfirst(lc), subexpr, false))
      {
        return true;
      }
    }
    return false;
  }
  if (is_funcclause(clause) && func_strict(((FuncExpr *)clause)->funcid))
  {
    foreach (lc, ((FuncExpr *)clause)->args)
    {
      if (clause_is_strict_for((Node *)lfirst(lc), subexpr, false))
      {
        return true;
      }
    }
    return false;
  }

     
                                                                            
                                                                            
                                                                             
                                                                       
                                                                             
                                                        
     
  if (IsA(clause, CoerceViaIO))
  {
    return clause_is_strict_for((Node *)((CoerceViaIO *)clause)->arg, subexpr, false);
  }
  if (IsA(clause, ArrayCoerceExpr))
  {
    return clause_is_strict_for((Node *)((ArrayCoerceExpr *)clause)->arg, subexpr, false);
  }
  if (IsA(clause, ConvertRowtypeExpr))
  {
    return clause_is_strict_for((Node *)((ConvertRowtypeExpr *)clause)->arg, subexpr, false);
  }
  if (IsA(clause, CoerceToDomain))
  {
    return clause_is_strict_for((Node *)((CoerceToDomain *)clause)->arg, subexpr, false);
  }

     
                                                                          
                                                                            
                                                                       
     
  if (IsA(clause, ScalarArrayOpExpr))
  {
    ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *)clause;
    Node *scalarnode = (Node *)linitial(saop->args);
    Node *arraynode = (Node *)lsecond(saop->args);

       
                                                                        
                                                                           
                                                                           
                                                                           
                                                                        
                                  
       
    if (clause_is_strict_for(scalarnode, subexpr, false) && op_strict(saop->opno))
    {
      int nelems = 0;

      if (allow_false && saop->useOr)
      {
        return true;                                         
      }

      if (arraynode && IsA(arraynode, Const))
      {
        Const *arrayconst = (Const *)arraynode;
        ArrayType *arrval;

           
                                                                    
                       
           
        if (arrayconst->constisnull)
        {
          return true;
        }

                                                               
        arrval = DatumGetArrayTypeP(arrayconst->constvalue);
        nelems = ArrayGetNItems(ARR_NDIM(arrval), ARR_DIMS(arrval));
      }
      else if (arraynode && IsA(arraynode, ArrayExpr) && !((ArrayExpr *)arraynode)->multidims)
      {
           
                                                                      
                                                           
           
        nelems = list_length(((ArrayExpr *)arraynode)->elements);
      }

                                                           
      if (nelems > 0)
      {
        return true;
      }
    }

       
                                                                         
                                                                        
                                                
       
    return clause_is_strict_for(arraynode, subexpr, false);
  }

     
                                                                       
                                                               
     
  if (IsA(clause, Const))
  {
    return ((Const *)clause)->constisnull;
  }

  return false;
}

   
                                                                            
                                      
   
                                                                              
                            
                                                                          
                                                                        
                                                                                
                                                                     
   
                                                                              
                                                                         
                                                                         
                                                                            
                                        
                                             
                                                 
                                             
                                                 
                                                                         
                                                                           
            
   
                                                                      
                                                              
   
                                                      
   
                                                                           
                                      
   
                                                                              
                                                                            
                                                                               
                                                                           
                                                
   
                                                                        
                                                                       
   
                                                                
   
                                                                              
                                                                        
                                                                               
                                                                            
                                               
   
                                                                        
                                                                       
   
                                                                           
   

#define BTLT BTLessStrategyNumber
#define BTLE BTLessEqualStrategyNumber
#define BTEQ BTEqualStrategyNumber
#define BTGE BTGreaterEqualStrategyNumber
#define BTGT BTGreaterStrategyNumber
#define BTNE ROWCOMPARE_NE

                                                               
#define none 0

static const bool BT_implies_table[6][6] = {
       
                                 
                                     
       
    {true, true, none, none, none, true},         
    {none, true, none, none, none, none},         
    {none, true, true, true, none, none},         
    {none, none, none, true, none, none},         
    {none, none, none, true, true, true},         
    {none, none, none, none, none, true}          
};

static const bool BT_refutes_table[6][6] = {
       
                                 
                                     
       
    {none, none, true, true, true, none},         
    {none, none, none, none, true, none},         
    {true, none, none, none, true, true},         
    {true, none, none, none, none, none},         
    {true, true, true, none, none, none},         
    {none, none, true, none, none, none}          
};

static const StrategyNumber BT_implic_table[6][6] = {
       
                                 
                                     
       
    {BTGE, BTGE, none, none, none, BTGE},         
    {BTGT, BTGE, none, none, none, BTGT},         
    {BTGT, BTGE, BTEQ, BTLE, BTLT, BTNE},         
    {none, none, none, BTLE, BTLT, BTLT},         
    {none, none, none, BTLE, BTLE, BTLE},         
    {none, none, none, none, none, BTEQ}          
};

static const StrategyNumber BT_refute_table[6][6] = {
       
                                 
                                     
       
    {none, none, BTGE, BTGE, BTGE, none},         
    {none, none, BTGT, BTGT, BTGE, none},         
    {BTLE, BTLT, BTNE, BTGT, BTGE, BTEQ},         
    {BTLE, BTLT, BTLT, none, none, none},         
    {BTLE, BTLE, BTLE, none, none, none},         
    {none, none, BTEQ, none, none, none}          
};

   
                            
                                                                             
                                                                         
                                                                      
   
                                                                 
                                                                 
                                                                    
                                                                     
                            
   
                                                                               
                                                                          
                                                                               
                                                                           
                                                                           
                                                                              
                                                                             
                                                                           
                                                                           
                                                                             
                                                                              
                                                                              
                                                                               
                                                                            
                                                                             
   
                                                                               
                                                                      
                                                               
                                                                  
                                                                             
                                                                         
                                                                         
                                                                               
   
                                                                              
                                                                               
                                                                             
                                                                            
                                                                        
                                                                             
                                                                           
                                            
   
                                                                           
                                                                                
                                                                          
                                                                          
                                                                             
                                               
   
static bool
operator_predicate_proof(Expr *predicate, Node *clause, bool refute_it, bool weak)
{
  OpExpr *pred_opexpr, *clause_opexpr;
  Oid pred_collation, clause_collation;
  Oid pred_op, clause_op, test_op;
  Node *pred_leftop, *pred_rightop, *clause_leftop, *clause_rightop;
  Const *pred_const, *clause_const;
  Expr *test_expr;
  ExprState *test_exprstate;
  Datum test_result;
  bool isNull;
  EState *estate;
  MemoryContext oldcontext;

     
                                                                           
     
                                                                        
                                                                        
                                                                            
                                         
     
  if (!is_opclause(predicate))
  {
    return false;
  }
  pred_opexpr = (OpExpr *)predicate;
  if (list_length(pred_opexpr->args) != 2)
  {
    return false;
  }
  if (!is_opclause(clause))
  {
    return false;
  }
  clause_opexpr = (OpExpr *)clause;
  if (list_length(clause_opexpr->args) != 2)
  {
    return false;
  }

     
                                                                            
                                                                
     
  pred_collation = pred_opexpr->inputcollid;
  clause_collation = clause_opexpr->inputcollid;
  if (pred_collation != clause_collation)
  {
    return false;
  }

                                                                    
  pred_op = pred_opexpr->opno;
  clause_op = clause_opexpr->opno;

     
                                                                 
     
  pred_leftop = (Node *)linitial(pred_opexpr->args);
  pred_rightop = (Node *)lsecond(pred_opexpr->args);
  clause_leftop = (Node *)linitial(clause_opexpr->args);
  clause_rightop = (Node *)lsecond(clause_opexpr->args);

  if (equal(pred_leftop, clause_leftop))
  {
    if (equal(pred_rightop, clause_rightop))
    {
                                       
      return operator_same_subexprs_proof(pred_op, clause_op, refute_it);
    }
    else
    {
                                                
      if (pred_rightop == NULL || !IsA(pred_rightop, Const))
      {
        return false;
      }
      pred_const = (Const *)pred_rightop;
      if (clause_rightop == NULL || !IsA(clause_rightop, Const))
      {
        return false;
      }
      clause_const = (Const *)clause_rightop;
    }
  }
  else if (equal(pred_rightop, clause_rightop))
  {
                                             
    if (pred_leftop == NULL || !IsA(pred_leftop, Const))
    {
      return false;
    }
    pred_const = (Const *)pred_leftop;
    if (clause_leftop == NULL || !IsA(clause_leftop, Const))
    {
      return false;
    }
    clause_const = (Const *)clause_leftop;
                                                                         
    pred_op = get_commutator(pred_op);
    if (!OidIsValid(pred_op))
    {
      return false;
    }
    clause_op = get_commutator(clause_op);
    if (!OidIsValid(clause_op))
    {
      return false;
    }
  }
  else if (equal(pred_leftop, clause_rightop))
  {
    if (equal(pred_rightop, clause_leftop))
    {
                                       
                                                                        
      pred_op = get_commutator(pred_op);
      if (!OidIsValid(pred_op))
      {
        return false;
      }
      return operator_same_subexprs_proof(pred_op, clause_op, refute_it);
    }
    else
    {
                                                                  
      if (pred_rightop == NULL || !IsA(pred_rightop, Const))
      {
        return false;
      }
      pred_const = (Const *)pred_rightop;
      if (clause_leftop == NULL || !IsA(clause_leftop, Const))
      {
        return false;
      }
      clause_const = (Const *)clause_leftop;
                                                                      
      clause_op = get_commutator(clause_op);
      if (!OidIsValid(clause_op))
      {
        return false;
      }
    }
  }
  else if (equal(pred_rightop, clause_leftop))
  {
                                                                
    if (pred_leftop == NULL || !IsA(pred_leftop, Const))
    {
      return false;
    }
    pred_const = (Const *)pred_leftop;
    if (clause_rightop == NULL || !IsA(clause_rightop, Const))
    {
      return false;
    }
    clause_const = (Const *)clause_rightop;
                                                                  
    pred_op = get_commutator(pred_op);
    if (!OidIsValid(pred_op))
    {
      return false;
    }
  }
  else
  {
                                                                  
    return false;
  }

     
                                                                             
                                                                     
                                                                             
                                                                             
                                                                         
     
  if (clause_const->constisnull)
  {
                                                            
    if (!op_strict(clause_op))
    {
      return false;
    }

       
                                                                      
                                                                      
                                                                          
                                      
       
    if (!(weak && !refute_it))
    {
      return true;
    }

       
                                                                           
                                                                         
                                                        
       
    if (pred_const->constisnull && op_strict(pred_op))
    {
      return true;
    }
                              
    return false;
  }
  if (pred_const->constisnull)
  {
       
                                                                          
                                                                    
                   
       
    if (weak && op_strict(pred_op))
    {
      return true;
    }
                              
    return false;
  }

     
                                                                           
                                      
     
  test_op = get_btree_test_op(pred_op, clause_op, refute_it);

  if (!OidIsValid(test_op))
  {
                                                      
    return false;
  }

     
                                                     
     
  estate = CreateExecutorState();

                                                                      
  oldcontext = MemoryContextSwitchTo(estate->es_query_cxt);

                             
  test_expr = make_opclause(test_op, BOOLOID, false, (Expr *)pred_const, (Expr *)clause_const, InvalidOid, pred_collation);

                         
  fix_opfuncids((Node *)test_expr);

                                
  test_exprstate = ExecInitExpr(test_expr, NULL);

                       
  test_result = ExecEvalExprSwitchContext(test_exprstate, GetPerTupleExprContext(estate), &isNull);

                                        
  MemoryContextSwitchTo(oldcontext);

                                            
  FreeExecutorState(estate);

  if (isNull)
  {
                                                                       
    elog(DEBUG2, "null predicate test result");
    return false;
  }
  return DatumGetBool(test_result);
}

   
                                
                                                                         
                          
   
                                                                         
   
static bool
operator_same_subexprs_proof(Oid pred_op, Oid clause_op, bool refute_it)
{
     
                                                                            
                                                                             
                                                                       
                                                                          
                                                                          
                                                                          
     
                                                                             
                                                                            
                                                                           
                                                                     
     
  if (refute_it)
  {
    if (get_negator(pred_op) == clause_op)
    {
      return true;
    }
  }
  else
  {
    if (pred_op == clause_op)
    {
      return true;
    }
  }

     
                                                                       
                                                      
     
  return operator_same_subexprs_lookup(pred_op, clause_op, refute_it);
}

   
                                                                        
                                                                           
                                                                        
                                                                        
                                                                          
                                             
   
typedef struct OprProofCacheKey
{
  Oid pred_op;                           
  Oid clause_op;                      
} OprProofCacheKey;

typedef struct OprProofCacheEntry
{
                                         
  OprProofCacheKey key;

  bool have_implic;                                                   
  bool have_refute;                                                  
  bool same_subexprs_implies;                                         
  bool same_subexprs_refutes;                                         
  Oid implic_test_op;                                                     
  Oid refute_test_op;                                                     
} OprProofCacheEntry;

static HTAB *OprProofCacheHash = NULL;

   
                      
                                                                 
   
static OprProofCacheEntry *
lookup_proof_cache(Oid pred_op, Oid clause_op, bool refute_it)
{
  OprProofCacheKey key;
  OprProofCacheEntry *cache_entry;
  bool cfound;
  bool same_subexprs = false;
  Oid test_op = InvalidOid;
  bool found = false;
  List *pred_op_infos, *clause_op_infos;
  ListCell *lcp, *lcc;

     
                                                            
     
  if (OprProofCacheHash == NULL)
  {
                                                       
    HASHCTL ctl;

    MemSet(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(OprProofCacheKey);
    ctl.entrysize = sizeof(OprProofCacheEntry);
    OprProofCacheHash = hash_create("Btree proof lookup cache", 256, &ctl, HASH_ELEM | HASH_BLOBS);

                                                   
    CacheRegisterSyscacheCallback(AMOPOPID, InvalidateOprProofCacheCallBack, (Datum)0);
  }

  key.pred_op = pred_op;
  key.clause_op = clause_op;
  cache_entry = (OprProofCacheEntry *)hash_search(OprProofCacheHash, (void *)&key, HASH_ENTER, &cfound);
  if (!cfound)
  {
                                         
    cache_entry->have_implic = false;
    cache_entry->have_refute = false;
  }
  else
  {
                                                                 
    if (refute_it ? cache_entry->have_refute : cache_entry->have_implic)
    {
      return cache_entry;
    }
  }

     
                                                                  
     
                                                                          
                                                                         
                                                                       
     
                                                                             
                                                                             
                                                                      
                            
     
                                                                
                                                                        
                                                                            
                           
     
  clause_op_infos = get_op_btree_interpretation(clause_op);
  if (clause_op_infos)
  {
    pred_op_infos = get_op_btree_interpretation(pred_op);
  }
  else                          
  {
    pred_op_infos = NIL;
  }

  foreach (lcp, pred_op_infos)
  {
    OpBtreeInterpretation *pred_op_info = lfirst(lcp);
    Oid opfamily_id = pred_op_info->opfamily_id;

    foreach (lcc, clause_op_infos)
    {
      OpBtreeInterpretation *clause_op_info = lfirst(lcc);
      StrategyNumber pred_strategy, clause_strategy, test_strategy;

                                           
      if (opfamily_id != clause_op_info->opfamily_id)
      {
        continue;
      }
                                  
      Assert(clause_op_info->oplefttype == pred_op_info->oplefttype);

      pred_strategy = pred_op_info->strategy;
      clause_strategy = clause_op_info->strategy;

         
                                                                     
                                                                      
         
      if (refute_it)
      {
        same_subexprs |= BT_refutes_table[clause_strategy - 1][pred_strategy - 1];
      }
      else
      {
        same_subexprs |= BT_implies_table[clause_strategy - 1][pred_strategy - 1];
      }

         
                                                                     
         
      if (refute_it)
      {
        test_strategy = BT_refute_table[clause_strategy - 1][pred_strategy - 1];
      }
      else
      {
        test_strategy = BT_implic_table[clause_strategy - 1][pred_strategy - 1];
      }

      if (test_strategy == 0)
      {
                                                                   
        continue;
      }

         
                                                                       
                    
         
      if (test_strategy == BTNE)
      {
        test_op = get_opfamily_member(opfamily_id, pred_op_info->oprighttype, clause_op_info->oprighttype, BTEqualStrategyNumber);
        if (OidIsValid(test_op))
        {
          test_op = get_negator(test_op);
        }
      }
      else
      {
        test_op = get_opfamily_member(opfamily_id, pred_op_info->oprighttype, clause_op_info->oprighttype, test_strategy);
      }

      if (!OidIsValid(test_op))
      {
        continue;
      }

         
                                                
         
                                                                        
                                                                       
                                                                         
                                                                       
                        
         
      if (op_volatile(test_op) == PROVOLATILE_IMMUTABLE)
      {
        found = true;
        break;
      }
    }

    if (found)
    {
      break;
    }
  }

  list_free_deep(pred_op_infos);
  list_free_deep(clause_op_infos);

  if (!found)
  {
                                                      
    test_op = InvalidOid;
  }

     
                                                                           
                                                                           
                                                                       
                                                                           
                             
     
  if (same_subexprs && op_volatile(clause_op) != PROVOLATILE_IMMUTABLE)
  {
    same_subexprs = false;
  }

                                                       
  if (refute_it)
  {
    cache_entry->refute_test_op = test_op;
    cache_entry->same_subexprs_refutes = same_subexprs;
    cache_entry->have_refute = true;
  }
  else
  {
    cache_entry->implic_test_op = test_op;
    cache_entry->same_subexprs_implies = same_subexprs;
    cache_entry->have_implic = true;
  }

  return cache_entry;
}

   
                                 
                                                             
                                
   
static bool
operator_same_subexprs_lookup(Oid pred_op, Oid clause_op, bool refute_it)
{
  OprProofCacheEntry *cache_entry;

  cache_entry = lookup_proof_cache(pred_op, clause_op, refute_it);
  if (refute_it)
  {
    return cache_entry->same_subexprs_refutes;
  }
  else
  {
    return cache_entry->same_subexprs_implies;
  }
}

   
                     
                                                                  
                                                            
   
                                                                            
                                                                            
                                                                              
                          
   
                                                                        
             
   
static Oid
get_btree_test_op(Oid pred_op, Oid clause_op, bool refute_it)
{
  OprProofCacheEntry *cache_entry;

  cache_entry = lookup_proof_cache(pred_op, clause_op, refute_it);
  if (refute_it)
  {
    return cache_entry->refute_test_op;
  }
  else
  {
    return cache_entry->implic_test_op;
  }
}

   
                                     
   
static void
InvalidateOprProofCacheCallBack(Datum arg, int cacheid, uint32 hashvalue)
{
  HASH_SEQ_STATUS status;
  OprProofCacheEntry *hentry;

  Assert(OprProofCacheHash != NULL);

                                                                   
  hash_seq_init(&status, OprProofCacheHash);

  while ((hentry = (OprProofCacheEntry *)hash_seq_search(&status)) != NULL)
  {
    hentry->have_implic = false;
    hentry->have_refute = false;
  }
}
