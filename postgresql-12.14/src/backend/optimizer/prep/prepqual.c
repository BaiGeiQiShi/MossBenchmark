                                                                            
   
              
                                                          
   
   
                                                                          
                                                                             
                                                                          
                                                                            
                                                                         
                                               
   
                                                                           
                                                                            
                                                                           
                                                                   
                                                                          
                                                                         
   
   
                                                                         
                                                                        
   
   
                  
                                           
   
                                                                            
   

#include "postgres.h"

#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "optimizer/prep.h"
#include "utils/lsyscache.h"

static List *
pull_ands(List *andlist);
static List *
pull_ors(List *orlist);
static Expr *
find_duplicate_ors(Expr *qual, bool is_check);
static Expr *
process_duplicate_ors(List *orlist);

   
                 
                                  
   
                                                                         
                                                                        
   
                                                                             
                                                                        
                                                                           
                                                                          
                                                                             
                                                           
   
                                                                          
                                                                             
                                                                            
                                                                            
                                                                            
                                                                       
                                                                            
                                                                           
                                                                         
                                                                             
                             
   
Node *
negate_clause(Node *node)
{
  if (node == NULL)                        
  {
    elog(ERROR, "can't negate an empty subexpression");
  }
  switch (nodeTag(node))
  {
  case T_Const:
  {
    Const *c = (Const *)node;

                                
    if (c->constisnull)
    {
      return makeBoolConst(false, true);
    }
                               
    return makeBoolConst(!DatumGetBool(c->constvalue), false);
  }
  break;
  case T_OpExpr:
  {
       
                                                              
       
    OpExpr *opexpr = (OpExpr *)node;
    Oid negator = get_negator(opexpr->opno);

    if (negator)
    {
      OpExpr *newopexpr = makeNode(OpExpr);

      newopexpr->opno = negator;
      newopexpr->opfuncid = InvalidOid;
      newopexpr->opresulttype = opexpr->opresulttype;
      newopexpr->opretset = opexpr->opretset;
      newopexpr->opcollid = opexpr->opcollid;
      newopexpr->inputcollid = opexpr->inputcollid;
      newopexpr->args = opexpr->args;
      newopexpr->location = opexpr->location;
      return (Node *)newopexpr;
    }
  }
  break;
  case T_ScalarArrayOpExpr:
  {
       
                                                                 
                                                          
       
    ScalarArrayOpExpr *saopexpr = (ScalarArrayOpExpr *)node;
    Oid negator = get_negator(saopexpr->opno);

    if (negator)
    {
      ScalarArrayOpExpr *newopexpr = makeNode(ScalarArrayOpExpr);

      newopexpr->opno = negator;
      newopexpr->opfuncid = InvalidOid;
      newopexpr->useOr = !saopexpr->useOr;
      newopexpr->inputcollid = saopexpr->inputcollid;
      newopexpr->args = saopexpr->args;
      newopexpr->location = saopexpr->location;
      return (Node *)newopexpr;
    }
  }
  break;
  case T_BoolExpr:
  {
    BoolExpr *expr = (BoolExpr *)node;

    switch (expr->boolop)
    {
                             
                                
                                                  
                                                  
                                                          
         
                                                            
                                                                 
                                                               
                                                                
                                                                 
                                                           
                                                               
                             
         
    case AND_EXPR:
    {
      List *nargs = NIL;
      ListCell *lc;

      foreach (lc, expr->args)
      {
        nargs = lappend(nargs, negate_clause(lfirst(lc)));
      }
      return (Node *)make_orclause(nargs);
    }
    break;
    case OR_EXPR:
    {
      List *nargs = NIL;
      ListCell *lc;

      foreach (lc, expr->args)
      {
        nargs = lappend(nargs, negate_clause(lfirst(lc)));
      }
      return (Node *)make_andclause(nargs);
    }
    break;
    case NOT_EXPR:

         
                                                         
                                                             
         
      return (Node *)linitial(expr->args);
    default:
      elog(ERROR, "unrecognized boolop: %d", (int)expr->boolop);
      break;
    }
  }
  break;
  case T_NullTest:
  {
    NullTest *expr = (NullTest *)node;

       
                                                                  
                                                                 
                             
       
    if (!expr->argisrow)
    {
      NullTest *newexpr = makeNode(NullTest);

      newexpr->arg = expr->arg;
      newexpr->nulltesttype = (expr->nulltesttype == IS_NULL ? IS_NOT_NULL : IS_NULL);
      newexpr->argisrow = expr->argisrow;
      newexpr->location = expr->location;
      return (Node *)newexpr;
    }
  }
  break;
  case T_BooleanTest:
  {
    BooleanTest *expr = (BooleanTest *)node;
    BooleanTest *newexpr = makeNode(BooleanTest);

    newexpr->arg = expr->arg;
    switch (expr->booltesttype)
    {
    case IS_TRUE:
      newexpr->booltesttype = IS_NOT_TRUE;
      break;
    case IS_NOT_TRUE:
      newexpr->booltesttype = IS_TRUE;
      break;
    case IS_FALSE:
      newexpr->booltesttype = IS_NOT_FALSE;
      break;
    case IS_NOT_FALSE:
      newexpr->booltesttype = IS_FALSE;
      break;
    case IS_UNKNOWN:
      newexpr->booltesttype = IS_NOT_UNKNOWN;
      break;
    case IS_NOT_UNKNOWN:
      newexpr->booltesttype = IS_UNKNOWN;
      break;
    default:
      elog(ERROR, "unrecognized booltesttype: %d", (int)expr->booltesttype);
      break;
    }
    newexpr->location = expr->location;
    return (Node *)newexpr;
  }
  break;
  default:
                           
    break;
  }

     
                                                                      
                        
     
  return (Node *)make_notclause((Expr *)node);
}

   
                     
                                                                 
   
                                                                         
                                                                           
                                                                             
                                                                            
   
                                                                           
                                                                      
                                                                        
                                                                        
                                        
   
                                                                             
                                                                             
                                                                             
         
   
                                       
   
Expr *
canonicalize_qual(Expr *qual, bool is_check)
{
  Expr *newqual;

                                 
  if (qual == NULL)
  {
    return NULL;
  }

                                                                  
  Assert(!IsA(qual, List));

     
                                                                       
                                                                        
                                                                         
     
  newqual = find_duplicate_ors(qual, is_check);

  return newqual;
}

   
             
                                                                           
   
                                          
                                                                              
   
static List *
pull_ands(List *andlist)
{
  List *out_list = NIL;
  ListCell *arg;

  foreach (arg, andlist)
  {
    Node *subexpr = (Node *)lfirst(arg);

       
                                                                     
                                                                       
                                                                          
                              
       
    if (is_andclause(subexpr))
    {
      out_list = list_concat(out_list, pull_ands(((BoolExpr *)subexpr)->args));
    }
    else
    {
      out_list = lappend(out_list, subexpr);
    }
  }
  return out_list;
}

   
            
                                                                         
   
                                         
                                                                              
   
static List *
pull_ors(List *orlist)
{
  List *out_list = NIL;
  ListCell *arg;

  foreach (arg, orlist)
  {
    Node *subexpr = (Node *)lfirst(arg);

       
                                                                     
                                                                      
                                                                          
                              
       
    if (is_orclause(subexpr))
    {
      out_list = list_concat(out_list, pull_ors(((BoolExpr *)subexpr)->args));
    }
    else
    {
      out_list = lappend(out_list, subexpr);
    }
  }
  return out_list;
}

                       
                                                                         
                                                   
                                                                   
                                                      
   
                                                                        
                                                                            
                                                                            
                                                                        
                                                                         
                             
      
                                                      
                                          
                                               
                                                                           
                                                       
                       
   

   
                      
                                                                      
                                                                      
                                                      
   
                                                                         
                                                                            
                                                                            
                                                                          
                                                                         
                                                                              
                                                           
   
                                                                      
   
static Expr *
find_duplicate_ors(Expr *qual, bool is_check)
{
  if (is_orclause(qual))
  {
    List *orlist = NIL;
    ListCell *temp;

                 
    foreach (temp, ((BoolExpr *)qual)->args)
    {
      Expr *arg = (Expr *)lfirst(temp);

      arg = find_duplicate_ors(arg, is_check);

                                          
      if (arg && IsA(arg, Const))
      {
        Const *carg = (Const *)arg;

        if (is_check)
        {
                                                       
          if (!carg->constisnull && !DatumGetBool(carg->constvalue))
          {
            continue;
          }
                                                            
          return (Expr *)makeBoolConst(true, false);
        }
        else
        {
                                                               
          if (carg->constisnull || !DatumGetBool(carg->constvalue))
          {
            continue;
          }
                                                    
          return arg;
        }
      }

      orlist = lappend(orlist, arg);
    }

                                                      
    orlist = pull_ors(orlist);

                                           
    return process_duplicate_ors(orlist);
  }
  else if (is_andclause(qual))
  {
    List *andlist = NIL;
    ListCell *temp;

                 
    foreach (temp, ((BoolExpr *)qual)->args)
    {
      Expr *arg = (Expr *)lfirst(temp);

      arg = find_duplicate_ors(arg, is_check);

                                          
      if (arg && IsA(arg, Const))
      {
        Const *carg = (Const *)arg;

        if (is_check)
        {
                                                               
          if (carg->constisnull || DatumGetBool(carg->constvalue))
          {
            continue;
          }
                                                       
          return arg;
        }
        else
        {
                                                       
          if (!carg->constisnull && DatumGetBool(carg->constvalue))
          {
            continue;
          }
                                                               
          return (Expr *)makeBoolConst(false, false);
        }
      }

      andlist = lappend(andlist, arg);
    }

                                                     
    andlist = pull_ands(andlist);

                                          
    if (andlist == NIL)
    {
      return (Expr *)makeBoolConst(true, false);
    }

                                                               
    if (list_length(andlist) == 1)
    {
      return (Expr *)linitial(andlist);
    }

                                        
    return make_andclause(andlist);
  }
  else
  {
    return qual;
  }
}

   
                         
                                                                 
                                      
   
                                                                   
                                                  
   
static Expr *
process_duplicate_ors(List *orlist)
{
  List *reference = NIL;
  int num_subclauses = 0;
  List *winners;
  List *neworlist;
  ListCell *temp;

                                        
  if (orlist == NIL)
  {
    return (Expr *)makeBoolConst(false, false);
  }

                                                            
  if (list_length(orlist) == 1)
  {
    return (Expr *)linitial(orlist);
  }

     
                                                                             
                                                                         
                                                                            
                                         
     
  foreach (temp, orlist)
  {
    Expr *clause = (Expr *)lfirst(temp);

    if (is_andclause(clause))
    {
      List *subclauses = ((BoolExpr *)clause)->args;
      int nclauses = list_length(subclauses);

      if (reference == NIL || nclauses < num_subclauses)
      {
        reference = subclauses;
        num_subclauses = nclauses;
      }
    }
    else
    {
      reference = list_make1(clause);
      break;
    }
  }

     
                                                                   
     
  reference = list_union(NIL, reference);

     
                                                                           
                                                    
     
  winners = NIL;
  foreach (temp, reference)
  {
    Expr *refclause = (Expr *)lfirst(temp);
    bool win = true;
    ListCell *temp2;

    foreach (temp2, orlist)
    {
      Expr *clause = (Expr *)lfirst(temp2);

      if (is_andclause(clause))
      {
        if (!list_member(((BoolExpr *)clause)->args, refclause))
        {
          win = false;
          break;
        }
      }
      else
      {
        if (!equal(refclause, clause))
        {
          win = false;
          break;
        }
      }
    }

    if (win)
    {
      winners = lappend(winners, refclause);
    }
  }

     
                                              
     
  if (winners == NIL)
  {
    return make_orclause(orlist);
  }

     
                                                                   
     
                                                                          
                                                                    
                                                                   
     
                                                                             
                                                                        
     
  neworlist = NIL;
  foreach (temp, orlist)
  {
    Expr *clause = (Expr *)lfirst(temp);

    if (is_andclause(clause))
    {
      List *subclauses = ((BoolExpr *)clause)->args;

      subclauses = list_difference(subclauses, winners);
      if (subclauses != NIL)
      {
        if (list_length(subclauses) == 1)
        {
          neworlist = lappend(neworlist, linitial(subclauses));
        }
        else
        {
          neworlist = lappend(neworlist, make_andclause(subclauses));
        }
      }
      else
      {
        neworlist = NIL;                                 
        break;
      }
    }
    else
    {
      if (!list_member(winners, clause))
      {
        neworlist = lappend(neworlist, clause);
      }
      else
      {
        neworlist = NIL;                                 
        break;
      }
    }
  }

     
                                                                             
                                                                          
                                                                        
                        
     
  if (neworlist != NIL)
  {
    if (list_length(neworlist) == 1)
    {
      winners = lappend(winners, linitial(neworlist));
    }
    else
    {
      winners = lappend(winners, make_orclause(pull_ors(neworlist)));
    }
  }

     
                                                                         
                                  
     
  if (list_length(winners) == 1)
  {
    return (Expr *)linitial(winners);
  }
  else
  {
    return make_andclause(pull_ands(winners));
  }
}
