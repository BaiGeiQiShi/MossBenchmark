                                                                            
   
                   
                                     
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
   
                      
                                                         
                                                               
                                          
   
         
   
                                                                        
                                                          
                                                                          
                                                                         
   
                                                               
                                                                  
                                                                         
               
   
                                                     
                                                 
   
                                                                      
                                                                     
                                                                     
                                                                
                              
   
                                                                      
                                                                       
                                                                    
                                                                       
                                                                         
                                                                       
                                                  
   
   
                                                                   
                                                                 
                                                                 
                                                                    
                                                                   
                                                                      
                                                                  
                                             
   
   
                                                                   
   
           
                                                      
                  
                                             
                          
                                             
            
                                             
        
                                          
                   
                                
                                     
                                            
         
                                            
                                         
                                                  
            
                                             
        
       
      
   
                                                     
                                                                 
                                                                  
                                                                 
                                 
   
#include "postgres.h"

#include "access/nbtree.h"
#include "executor/execdebug.h"
#include "executor/nodeMergejoin.h"
#include "miscadmin.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

   
                                             
   
#define EXEC_MJ_INITIALIZE_OUTER 1
#define EXEC_MJ_INITIALIZE_INNER 2
#define EXEC_MJ_JOINTUPLES 3
#define EXEC_MJ_NEXTOUTER 4
#define EXEC_MJ_TESTOUTER 5
#define EXEC_MJ_NEXTINNER 6
#define EXEC_MJ_SKIP_TEST 7
#define EXEC_MJ_SKIPOUTER_ADVANCE 8
#define EXEC_MJ_SKIPINNER_ADVANCE 9
#define EXEC_MJ_ENDOUTER 10
#define EXEC_MJ_ENDINNER 11

   
                                          
   
typedef struct MergeJoinClauseData
{
                                   
  ExprState *lexpr;                                         
  ExprState *rexpr;                                          

     
                                                                       
                                               
     
  Datum ldatum;                              
  Datum rdatum;                               
  bool lisnull;                             
  bool risnull;

     
                                                                        
                  
     
  SortSupportData ssup;
} MergeJoinClauseData;

                                                             
typedef enum
{
  MJEVAL_MATCHABLE,                                             
  MJEVAL_NONMATCHABLE,                                              
  MJEVAL_ENDOFJOIN                                               
} MJEvalResult;

#define MarkInnerTuple(innerTupleSlot, mergestate) ExecCopySlot((mergestate)->mj_MarkedTupleSlot, (innerTupleSlot))

   
                  
   
                                                                           
                                                                        
                                                                          
                                                                           
                                                                             
                                                 
   
                                                                           
                                                                               
                                                                             
                                                                       
                                                                              
                                                                             
                                                                                
                                                                              
   
static MergeJoinClause
MJExamineQuals(List *mergeclauses, Oid *mergefamilies, Oid *mergecollations, int *mergestrategies, bool *mergenullsfirst, PlanState *parent)
{
  MergeJoinClause clauses;
  int nClauses = list_length(mergeclauses);
  int iClause;
  ListCell *cl;

  clauses = (MergeJoinClause)palloc0(nClauses * sizeof(MergeJoinClauseData));

  iClause = 0;
  foreach (cl, mergeclauses)
  {
    OpExpr *qual = (OpExpr *)lfirst(cl);
    MergeJoinClause clause = &clauses[iClause];
    Oid opfamily = mergefamilies[iClause];
    Oid collation = mergecollations[iClause];
    StrategyNumber opstrategy = mergestrategies[iClause];
    bool nulls_first = mergenullsfirst[iClause];
    int op_strategy;
    Oid op_lefttype;
    Oid op_righttype;
    Oid sortfunc;

    if (!IsA(qual, OpExpr))
    {
      elog(ERROR, "mergejoin clause is not an OpExpr");
    }

       
                                                    
       
    clause->lexpr = ExecInitExpr((Expr *)linitial(qual->args), parent);
    clause->rexpr = ExecInitExpr((Expr *)lsecond(qual->args), parent);

                                  
    clause->ssup.ssup_cxt = CurrentMemoryContext;
    clause->ssup.ssup_collation = collation;
    if (opstrategy == BTLessStrategyNumber)
    {
      clause->ssup.ssup_reverse = false;
    }
    else if (opstrategy == BTGreaterStrategyNumber)
    {
      clause->ssup.ssup_reverse = true;
    }
    else                         
    {
      elog(ERROR, "unsupported mergejoin strategy %d", opstrategy);
    }
    clause->ssup.ssup_nulls_first = nulls_first;

                                                              
    get_op_opfamily_properties(qual->opno, opfamily, false, &op_strategy, &op_lefttype, &op_righttype);
    if (op_strategy != BTEqualStrategyNumber)                        
    {
      elog(ERROR, "cannot merge using non-equality operator %u", qual->opno);
    }

       
                                                                     
                                                                        
                                                                
                                   
       
    clause->ssup.abbreviate = false;

                                                             
    Assert(clause->ssup.comparator == NULL);
    sortfunc = get_opfamily_proc(opfamily, op_lefttype, op_righttype, BTSORTSUPPORT_PROC);
    if (OidIsValid(sortfunc))
    {
                                                              
      OidFunctionCall1(sortfunc, PointerGetDatum(&clause->ssup));
    }
    if (clause->ssup.comparator == NULL)
    {
                                                      
      sortfunc = get_opfamily_proc(opfamily, op_lefttype, op_righttype, BTORDER_PROC);
      if (!OidIsValid(sortfunc))                        
      {
        elog(ERROR, "missing support function %d(%u,%u) in opfamily %u", BTORDER_PROC, op_lefttype, op_righttype, opfamily);
      }
                                                                   
      PrepareSortSupportComparisonShim(sortfunc, &clause->ssup);
    }

    iClause++;
  }

  return clauses;
}

   
                     
   
                                                                     
                                                                        
                                                                      
                                                                       
                                                                       
                                                                      
                                                                         
                                                                      
                                           
   
                                                                         
                                                                  
                                                                            
                                                                            
                        
   
                                                                    
                                
   
static MJEvalResult
MJEvalOuterValues(MergeJoinState *mergestate)
{
  ExprContext *econtext = mergestate->mj_OuterEContext;
  MJEvalResult result = MJEVAL_MATCHABLE;
  int i;
  MemoryContext oldContext;

                                      
  if (TupIsNull(mergestate->mj_OuterTupleSlot))
  {
    return MJEVAL_ENDOFJOIN;
  }

  ResetExprContext(econtext);

  oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

  econtext->ecxt_outertuple = mergestate->mj_OuterTupleSlot;

  for (i = 0; i < mergestate->mj_NumClauses; i++)
  {
    MergeJoinClause clause = &mergestate->mj_Clauses[i];

    clause->ldatum = ExecEvalExpr(clause->lexpr, econtext, &clause->lisnull);
    if (clause->lisnull)
    {
                                                           
      if (i == 0 && !clause->ssup.ssup_nulls_first && !mergestate->mj_FillOuter)
      {
        result = MJEVAL_ENDOFJOIN;
      }
      else if (result == MJEVAL_MATCHABLE)
      {
        result = MJEVAL_NONMATCHABLE;
      }
    }
  }

  MemoryContextSwitchTo(oldContext);

  return result;
}

   
                     
   
                                                                         
                                                                         
                                                   
   
static MJEvalResult
MJEvalInnerValues(MergeJoinState *mergestate, TupleTableSlot *innerslot)
{
  ExprContext *econtext = mergestate->mj_InnerEContext;
  MJEvalResult result = MJEVAL_MATCHABLE;
  int i;
  MemoryContext oldContext;

                                      
  if (TupIsNull(innerslot))
  {
    return MJEVAL_ENDOFJOIN;
  }

  ResetExprContext(econtext);

  oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

  econtext->ecxt_innertuple = innerslot;

  for (i = 0; i < mergestate->mj_NumClauses; i++)
  {
    MergeJoinClause clause = &mergestate->mj_Clauses[i];

    clause->rdatum = ExecEvalExpr(clause->rexpr, econtext, &clause->risnull);
    if (clause->risnull)
    {
                                                           
      if (i == 0 && !clause->ssup.ssup_nulls_first && !mergestate->mj_FillInner)
      {
        result = MJEVAL_ENDOFJOIN;
      }
      else if (result == MJEVAL_MATCHABLE)
      {
        result = MJEVAL_NONMATCHABLE;
      }
    }
  }

  MemoryContextSwitchTo(oldContext);

  return result;
}

   
             
   
                                                                    
                                                                    
                                                       
   
                                                                         
                                                         
   
static int
MJCompare(MergeJoinState *mergestate)
{
  int result = 0;
  bool nulleqnull = false;
  ExprContext *econtext = mergestate->js.ps.ps_ExprContext;
  int i;
  MemoryContext oldContext;

     
                                                                             
             
     
  ResetExprContext(econtext);

  oldContext = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

  for (i = 0; i < mergestate->mj_NumClauses; i++)
  {
    MergeJoinClause clause = &mergestate->mj_Clauses[i];

       
                                                                    
       
    if (clause->lisnull && clause->risnull)
    {
      nulleqnull = true;                    
      continue;
    }

    result = ApplySortComparator(clause->ldatum, clause->lisnull, clause->rdatum, clause->risnull, &clause->ssup);

    if (result != 0)
    {
      break;
    }
  }

     
                                                                          
                                                                             
                                                          
     
                                                                     
                                                                          
                                           
     
  if (result == 0 && (nulleqnull || mergestate->mj_ConstFalseJoin))
  {
    result = 1;
  }

  MemoryContextSwitchTo(oldContext);

  return result;
}

   
                                                              
                                                  
   
static TupleTableSlot *
MJFillOuter(MergeJoinState *node)
{
  ExprContext *econtext = node->js.ps.ps_ExprContext;
  ExprState *otherqual = node->js.ps.qual;

  ResetExprContext(econtext);

  econtext->ecxt_outertuple = node->mj_OuterTupleSlot;
  econtext->ecxt_innertuple = node->mj_NullInnerTupleSlot;

  if (ExecQual(otherqual, econtext))
  {
       
                                                                           
                                      
       
    MJ_printf("ExecMergeJoin: returning outer fill tuple\n");

    return ExecProject(node->js.ps.ps_ProjInfo);
  }
  else
  {
    InstrCountFiltered2(node, 1);
  }

  return NULL;
}

   
                                                              
                                                  
   
static TupleTableSlot *
MJFillInner(MergeJoinState *node)
{
  ExprContext *econtext = node->js.ps.ps_ExprContext;
  ExprState *otherqual = node->js.ps.qual;

  ResetExprContext(econtext);

  econtext->ecxt_outertuple = node->mj_NullOuterTupleSlot;
  econtext->ecxt_innertuple = node->mj_InnerTupleSlot;

  if (ExecQual(otherqual, econtext))
  {
       
                                                                           
                                      
       
    MJ_printf("ExecMergeJoin: returning inner fill tuple\n");

    return ExecProject(node->js.ps.ps_ProjInfo);
  }
  else
  {
    InstrCountFiltered2(node, 1);
  }

  return NULL;
}

   
                                                                   
                                                                   
   
                                                                              
                                                                              
                                                                           
   
static bool
check_constant_qual(List *qual, bool *is_const_false)
{
  ListCell *lc;

  foreach (lc, qual)
  {
    Const *con = (Const *)lfirst(lc);

    if (!con || !IsA(con, Const))
    {
      return false;
    }
    if (con->constisnull || !DatumGetBool(con->constvalue))
    {
      *is_const_false = true;
    }
  }
  return true;
}

                                                                    
                       
   
                                                        
                                        
                                                                    
   
#ifdef EXEC_MERGEJOINDEBUG

static void
ExecMergeTupleDumpOuter(MergeJoinState *mergestate)
{
  TupleTableSlot *outerSlot = mergestate->mj_OuterTupleSlot;

  printf("==== outer tuple ====\n");
  if (TupIsNull(outerSlot))
  {
    printf("(nil)\n");
  }
  else
  {
    MJ_debugtup(outerSlot);
  }
}

static void
ExecMergeTupleDumpInner(MergeJoinState *mergestate)
{
  TupleTableSlot *innerSlot = mergestate->mj_InnerTupleSlot;

  printf("==== inner tuple ====\n");
  if (TupIsNull(innerSlot))
  {
    printf("(nil)\n");
  }
  else
  {
    MJ_debugtup(innerSlot);
  }
}

static void
ExecMergeTupleDumpMarked(MergeJoinState *mergestate)
{
  TupleTableSlot *markedSlot = mergestate->mj_MarkedTupleSlot;

  printf("==== marked tuple ====\n");
  if (TupIsNull(markedSlot))
  {
    printf("(nil)\n");
  }
  else
  {
    MJ_debugtup(markedSlot);
  }
}

static void
ExecMergeTupleDump(MergeJoinState *mergestate)
{
  printf("******** ExecMergeTupleDump ********\n");

  ExecMergeTupleDumpOuter(mergestate);
  ExecMergeTupleDumpInner(mergestate);
  ExecMergeTupleDumpMarked(mergestate);

  printf("********\n");
}
#endif

                                                                    
                  
                                                                    
   
static TupleTableSlot *
ExecMergeJoin(PlanState *pstate)
{
  MergeJoinState *node = castNode(MergeJoinState, pstate);
  ExprState *joinqual;
  ExprState *otherqual;
  bool qualResult;
  int compareResult;
  PlanState *innerPlan;
  TupleTableSlot *innerTupleSlot;
  PlanState *outerPlan;
  TupleTableSlot *outerTupleSlot;
  ExprContext *econtext;
  bool doFillOuter;
  bool doFillInner;

  CHECK_FOR_INTERRUPTS();

     
                               
     
  innerPlan = innerPlanState(node);
  outerPlan = outerPlanState(node);
  econtext = node->js.ps.ps_ExprContext;
  joinqual = node->js.joinqual;
  otherqual = node->js.ps.qual;
  doFillOuter = node->mj_FillOuter;
  doFillInner = node->mj_FillInner;

     
                                                                      
                                                    
     
  ResetExprContext(econtext);

     
                                                
     
  for (;;)
  {
    MJ_dump(node);

       
                                                                    
       
    switch (node->mj_JoinState)
    {
         
                                                                    
                                                                     
                                                                     
                                                                   
                                                          
         
    case EXEC_MJ_INITIALIZE_OUTER:
      MJ_printf("ExecMergeJoin: EXEC_MJ_INITIALIZE_OUTER\n");

      outerTupleSlot = ExecProcNode(outerPlan);
      node->mj_OuterTupleSlot = outerTupleSlot;

                                                            
      switch (MJEvalOuterValues(node))
      {
      case MJEVAL_MATCHABLE:
                                                
        node->mj_JoinState = EXEC_MJ_INITIALIZE_INNER;
        break;
      case MJEVAL_NONMATCHABLE:
                                                          
        if (doFillOuter)
        {
             
                                                           
                                                         
                             
             
          TupleTableSlot *result;

          result = MJFillOuter(node);
          if (result)
          {
            return result;
          }
        }
        break;
      case MJEVAL_ENDOFJOIN:
                                  
        MJ_printf("ExecMergeJoin: nothing in outer subplan\n");
        if (doFillInner)
        {
             
                                                          
                                                         
                                                        
             
          node->mj_JoinState = EXEC_MJ_ENDOUTER;
          node->mj_MatchedInner = true;
          break;
        }
                                   
        return NULL;
      }
      break;

    case EXEC_MJ_INITIALIZE_INNER:
      MJ_printf("ExecMergeJoin: EXEC_MJ_INITIALIZE_INNER\n");

      innerTupleSlot = ExecProcNode(innerPlan);
      node->mj_InnerTupleSlot = innerTupleSlot;

                                                            
      switch (MJEvalInnerValues(node, innerTupleSlot))
      {
      case MJEVAL_MATCHABLE:

           
                                                              
                                
           
        node->mj_JoinState = EXEC_MJ_SKIP_TEST;
        break;
      case MJEVAL_NONMATCHABLE:
                                              
        if (node->mj_ExtraMarks)
        {
          ExecMarkPos(innerPlan);
        }
                                                          
        if (doFillInner)
        {
             
                                                           
                                                         
                             
             
          TupleTableSlot *result;

          result = MJFillInner(node);
          if (result)
          {
            return result;
          }
        }
        break;
      case MJEVAL_ENDOFJOIN:
                                  
        MJ_printf("ExecMergeJoin: nothing in inner subplan\n");
        if (doFillOuter)
        {
             
                                                         
                                                            
                                                            
                                                        
                    
             
          node->mj_JoinState = EXEC_MJ_ENDINNER;
          node->mj_MatchedOuter = false;
          break;
        }
                                   
        return NULL;
      }
      break;

         
                                                                     
                                                                  
                                                   
         
    case EXEC_MJ_JOINTUPLES:
      MJ_printf("ExecMergeJoin: EXEC_MJ_JOINTUPLES\n");

         
                                                                  
                                                               
                                                          
         
      node->mj_JoinState = EXEC_MJ_NEXTINNER;

         
                                                                    
                                                                     
                                                                  
                                                                     
                                                                 
                            
         
                                                              
                                                                  
                                                                   
                                                                 
              
         
      outerTupleSlot = node->mj_OuterTupleSlot;
      econtext->ecxt_outertuple = outerTupleSlot;
      innerTupleSlot = node->mj_InnerTupleSlot;
      econtext->ecxt_innertuple = innerTupleSlot;

      qualResult = (joinqual == NULL || ExecQual(joinqual, econtext));
      MJ_DEBUG_QUAL(joinqual, qualResult);

      if (qualResult)
      {
        node->mj_MatchedOuter = true;
        node->mj_MatchedInner = true;

                                                             
        if (node->js.jointype == JOIN_ANTI)
        {
          node->mj_JoinState = EXEC_MJ_NEXTOUTER;
          break;
        }

           
                                                               
                                                                   
                                           
           
        if (node->js.single_match)
        {
          node->mj_JoinState = EXEC_MJ_NEXTOUTER;
        }

        qualResult = (otherqual == NULL || ExecQual(otherqual, econtext));
        MJ_DEBUG_QUAL(otherqual, qualResult);

        if (qualResult)
        {
             
                                                            
                                                                 
             
          MJ_printf("ExecMergeJoin: returning tuple\n");

          return ExecProject(node->js.ps.ps_ProjInfo);
        }
        else
        {
          InstrCountFiltered2(node, 1);
        }
      }
      else
      {
        InstrCountFiltered1(node, 1);
      }
      break;

         
                                                                    
                                                                    
                                         
         
                                                              
                                                     
         
    case EXEC_MJ_NEXTINNER:
      MJ_printf("ExecMergeJoin: EXEC_MJ_NEXTINNER\n");

      if (doFillInner && !node->mj_MatchedInner)
      {
           
                                                               
                                                                 
           
        TupleTableSlot *result;

        node->mj_MatchedInner = true;                      

        result = MJFillInner(node);
        if (result)
        {
          return result;
        }
      }

         
                                                                    
                                                                   
                                    
         
                                                                 
                                             
         
      innerTupleSlot = ExecProcNode(innerPlan);
      node->mj_InnerTupleSlot = innerTupleSlot;
      MJ_DEBUG_PROC_NODE(innerTupleSlot);
      node->mj_MatchedInner = false;

                                                            
      switch (MJEvalInnerValues(node, innerTupleSlot))
      {
      case MJEVAL_MATCHABLE:

           
                                                         
                  
           
                                                              
                                                      
           
                                                           
                  
           
        compareResult = MJCompare(node);
        MJ_DEBUG_COMPARE(compareResult);

        if (compareResult == 0)
        {
          node->mj_JoinState = EXEC_MJ_JOINTUPLES;
        }
        else if (compareResult < 0)
        {
          node->mj_JoinState = EXEC_MJ_NEXTOUTER;
        }
        else                                          
        {
          elog(ERROR, "mergejoin input data is out of order");
        }
        break;
      case MJEVAL_NONMATCHABLE:

           
                                                              
                                                               
                                                    
           
        node->mj_JoinState = EXEC_MJ_NEXTOUTER;
        break;
      case MJEVAL_ENDOFJOIN:

           
                                                              
                                                            
                                                           
                                                              
                                                              
                                                   
           
        node->mj_InnerTupleSlot = NULL;
        node->mj_JoinState = EXEC_MJ_NEXTOUTER;
        break;
      }
      break;

                                                    
                                 
         
                        
                                             
                   
                                  
                   
         
                                         
                                                              
                                      
                                           
                                                     
                             
         
                                                              
                                                     
                                                         
         
    case EXEC_MJ_NEXTOUTER:
      MJ_printf("ExecMergeJoin: EXEC_MJ_NEXTOUTER\n");

      if (doFillOuter && !node->mj_MatchedOuter)
      {
           
                                                               
                                                                 
           
        TupleTableSlot *result;

        node->mj_MatchedOuter = true;                      

        result = MJFillOuter(node);
        if (result)
        {
          return result;
        }
      }

         
                                                 
         
      outerTupleSlot = ExecProcNode(outerPlan);
      node->mj_OuterTupleSlot = outerTupleSlot;
      MJ_DEBUG_PROC_NODE(outerTupleSlot);
      node->mj_MatchedOuter = false;

                                                            
      switch (MJEvalOuterValues(node))
      {
      case MJEVAL_MATCHABLE:
                                                            
        node->mj_JoinState = EXEC_MJ_TESTOUTER;
        break;
      case MJEVAL_NONMATCHABLE:
                                                    
        node->mj_JoinState = EXEC_MJ_NEXTOUTER;
        break;
      case MJEVAL_ENDOFJOIN:
                                  
        MJ_printf("ExecMergeJoin: end of outer subplan\n");
        innerTupleSlot = node->mj_InnerTupleSlot;
        if (doFillInner && !TupIsNull(innerTupleSlot))
        {
             
                                                          
                           
             
          node->mj_JoinState = EXEC_MJ_ENDOUTER;
          break;
        }
                                   
        return NULL;
      }
      break;

                                                                 
                                                                 
                                                             
                                                                
                                                                
                                                
         
                               
                            
                                     
                                
                                   
                                    
                     
         
                                            
         
                                                             
                                                           
                                                       
                                                           
                              
         
                                
         
                            
                                     
                                
                                                  
                     
         
                                           
         
                                                                  
         
    case EXEC_MJ_TESTOUTER:
      MJ_printf("ExecMergeJoin: EXEC_MJ_TESTOUTER\n");

         
                                                                    
                                                                 
                                                               
         
      innerTupleSlot = node->mj_MarkedTupleSlot;
      (void)MJEvalInnerValues(node, innerTupleSlot);

      compareResult = MJCompare(node);
      MJ_DEBUG_COMPARE(compareResult);

      if (compareResult == 0)
      {
           
                                                                
                                                                   
                                                      
           
                                                                 
                                                              
                                                      
           
                                                                
                                                                 
                                                              
                                                               
                                                                 
                                                                 
                                                                
                                                                 
                                                                 
                                                                
                     
           
        if (!node->mj_SkipMarkRestore)
        {
          ExecRestrPos(innerPlan);

             
                                                             
                                                              
                                                               
                                                     
             
          node->mj_InnerTupleSlot = innerTupleSlot;
                                                      
        }

        node->mj_JoinState = EXEC_MJ_JOINTUPLES;
      }
      else if (compareResult > 0)
      {
                            
                                                                
                                           
           
                          
                                    
                            
                                   
                  
           
                                                                
                                                                
                                                               
                                                              
                                                         
                            
           
        innerTupleSlot = node->mj_InnerTupleSlot;

                                                      
        switch (MJEvalInnerValues(node, innerTupleSlot))
        {
        case MJEVAL_MATCHABLE:
                                                          
          node->mj_JoinState = EXEC_MJ_SKIP_TEST;
          break;
        case MJEVAL_NONMATCHABLE:

             
                                                           
                                                       
                    
             
          node->mj_JoinState = EXEC_MJ_SKIPINNER_ADVANCE;
          break;
        case MJEVAL_ENDOFJOIN:
                                    
          if (doFillOuter)
          {
               
                                                           
                             
               
            node->mj_JoinState = EXEC_MJ_ENDINNER;
            break;
          }
                                     
          return NULL;
        }
      }
      else                                          
      {
        elog(ERROR, "mergejoin input data is out of order");
      }
      break;

                                                                   
                                                              
                                          
         
                      
         
                        
                   
                   
                                            
                      
                      
         
                                           
                                    
         
                            
         
                        
                   
                   
                                            
                      
                      
         
                                           
                                     
                                                                   
         
    case EXEC_MJ_SKIP_TEST:
      MJ_printf("ExecMergeJoin: EXEC_MJ_SKIP_TEST\n");

         
                                                                
                                                                   
                                                 
         
      compareResult = MJCompare(node);
      MJ_DEBUG_COMPARE(compareResult);

      if (compareResult == 0)
      {
        if (!node->mj_SkipMarkRestore)
        {
          ExecMarkPos(innerPlan);
        }

        MarkInnerTuple(node->mj_InnerTupleSlot, node);

        node->mj_JoinState = EXEC_MJ_JOINTUPLES;
      }
      else if (compareResult < 0)
      {
        node->mj_JoinState = EXEC_MJ_SKIPOUTER_ADVANCE;
      }
      else
      {
                               
        node->mj_JoinState = EXEC_MJ_SKIPINNER_ADVANCE;
      }
      break;

         
                                                                
                                               
         
                                                              
                                                     
         
    case EXEC_MJ_SKIPOUTER_ADVANCE:
      MJ_printf("ExecMergeJoin: EXEC_MJ_SKIPOUTER_ADVANCE\n");

      if (doFillOuter && !node->mj_MatchedOuter)
      {
           
                                                               
                                                                 
           
        TupleTableSlot *result;

        node->mj_MatchedOuter = true;                      

        result = MJFillOuter(node);
        if (result)
        {
          return result;
        }
      }

         
                                                 
         
      outerTupleSlot = ExecProcNode(outerPlan);
      node->mj_OuterTupleSlot = outerTupleSlot;
      MJ_DEBUG_PROC_NODE(outerTupleSlot);
      node->mj_MatchedOuter = false;

                                                            
      switch (MJEvalOuterValues(node))
      {
      case MJEVAL_MATCHABLE:
                                                             
        node->mj_JoinState = EXEC_MJ_SKIP_TEST;
        break;
      case MJEVAL_NONMATCHABLE:
                                                    
        node->mj_JoinState = EXEC_MJ_SKIPOUTER_ADVANCE;
        break;
      case MJEVAL_ENDOFJOIN:
                                  
        MJ_printf("ExecMergeJoin: end of outer subplan\n");
        innerTupleSlot = node->mj_InnerTupleSlot;
        if (doFillInner && !TupIsNull(innerTupleSlot))
        {
             
                                                          
                           
             
          node->mj_JoinState = EXEC_MJ_ENDOUTER;
          break;
        }
                                   
        return NULL;
      }
      break;

         
                                                                
                                               
         
                                                              
                                                     
         
    case EXEC_MJ_SKIPINNER_ADVANCE:
      MJ_printf("ExecMergeJoin: EXEC_MJ_SKIPINNER_ADVANCE\n");

      if (doFillInner && !node->mj_MatchedInner)
      {
           
                                                               
                                                                 
           
        TupleTableSlot *result;

        node->mj_MatchedInner = true;                      

        result = MJFillInner(node);
        if (result)
        {
          return result;
        }
      }

                                            
      if (node->mj_ExtraMarks)
      {
        ExecMarkPos(innerPlan);
      }

         
                                                 
         
      innerTupleSlot = ExecProcNode(innerPlan);
      node->mj_InnerTupleSlot = innerTupleSlot;
      MJ_DEBUG_PROC_NODE(innerTupleSlot);
      node->mj_MatchedInner = false;

                                                            
      switch (MJEvalInnerValues(node, innerTupleSlot))
      {
      case MJEVAL_MATCHABLE:
                                                        
        node->mj_JoinState = EXEC_MJ_SKIP_TEST;
        break;
      case MJEVAL_NONMATCHABLE:

           
                                                         
                                                            
           
        node->mj_JoinState = EXEC_MJ_SKIPINNER_ADVANCE;
        break;
      case MJEVAL_ENDOFJOIN:
                                  
        MJ_printf("ExecMergeJoin: end of inner subplan\n");
        outerTupleSlot = node->mj_OuterTupleSlot;
        if (doFillOuter && !TupIsNull(outerTupleSlot))
        {
             
                                                         
                           
             
          node->mj_JoinState = EXEC_MJ_ENDINNER;
          break;
        }
                                   
        return NULL;
      }
      break;

         
                                                                     
                                                                  
                                               
         
    case EXEC_MJ_ENDOUTER:
      MJ_printf("ExecMergeJoin: EXEC_MJ_ENDOUTER\n");

      Assert(doFillInner);

      if (!node->mj_MatchedInner)
      {
           
                                                               
                                                                 
           
        TupleTableSlot *result;

        node->mj_MatchedInner = true;                      

        result = MJFillInner(node);
        if (result)
        {
          return result;
        }
      }

                                            
      if (node->mj_ExtraMarks)
      {
        ExecMarkPos(innerPlan);
      }

         
                                                 
         
      innerTupleSlot = ExecProcNode(innerPlan);
      node->mj_InnerTupleSlot = innerTupleSlot;
      MJ_DEBUG_PROC_NODE(innerTupleSlot);
      node->mj_MatchedInner = false;

      if (TupIsNull(innerTupleSlot))
      {
        MJ_printf("ExecMergeJoin: end of inner subplan\n");
        return NULL;
      }

                                                                 
      break;

         
                                                                     
                                                                  
                                               
         
    case EXEC_MJ_ENDINNER:
      MJ_printf("ExecMergeJoin: EXEC_MJ_ENDINNER\n");

      Assert(doFillOuter);

      if (!node->mj_MatchedOuter)
      {
           
                                                               
                                                                 
           
        TupleTableSlot *result;

        node->mj_MatchedOuter = true;                      

        result = MJFillOuter(node);
        if (result)
        {
          return result;
        }
      }

         
                                                 
         
      outerTupleSlot = ExecProcNode(outerPlan);
      node->mj_OuterTupleSlot = outerTupleSlot;
      MJ_DEBUG_PROC_NODE(outerTupleSlot);
      node->mj_MatchedOuter = false;

      if (TupIsNull(outerTupleSlot))
      {
        MJ_printf("ExecMergeJoin: end of outer subplan\n");
        return NULL;
      }

                                                                 
      break;

         
                             
         
    default:
      elog(ERROR, "unrecognized mergejoin state: %d", (int)node->mj_JoinState);
    }
  }
}

                                                                    
                      
                                                                    
   
MergeJoinState *
ExecInitMergeJoin(MergeJoin *node, EState *estate, int eflags)
{
  MergeJoinState *mergestate;
  TupleDesc outerDesc, innerDesc;
  const TupleTableSlotOps *innerOps;

                                   
  Assert(!(eflags & (EXEC_FLAG_BACKWARD | EXEC_FLAG_MARK)));

  MJ1_printf("ExecInitMergeJoin: %s\n", "initializing node");

     
                            
     
  mergestate = makeNode(MergeJoinState);
  mergestate->js.ps.plan = (Plan *)node;
  mergestate->js.ps.state = estate;
  mergestate->js.ps.ExecProcNode = ExecMergeJoin;
  mergestate->js.jointype = node->join.jointype;
  mergestate->mj_ConstFalseJoin = false;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &mergestate->js.ps);

     
                                                                       
                                                                           
                                                        
     
  mergestate->mj_OuterEContext = CreateExprContext(estate);
  mergestate->mj_InnerEContext = CreateExprContext(estate);

     
                            
     
                                                                            
                                                                        
                                                                         
     
  Assert(node->join.joinqual == NIL || !node->skip_mark_restore);
  mergestate->mj_SkipMarkRestore = node->skip_mark_restore;

  outerPlanState(mergestate) = ExecInitNode(outerPlan(node), estate, eflags);
  outerDesc = ExecGetResultType(outerPlanState(mergestate));
  innerPlanState(mergestate) = ExecInitNode(innerPlan(node), estate, mergestate->mj_SkipMarkRestore ? eflags : (eflags | EXEC_FLAG_MARK));
  innerDesc = ExecGetResultType(innerPlanState(mergestate));

     
                                                                         
                                                                             
                                                                        
                                                                            
                                     
     
                                                                            
                                            
     
                                                                          
                                                                            
                                                       
     
  if (IsA(innerPlan(node), Material) && (eflags & EXEC_FLAG_REWIND) == 0 && !mergestate->mj_SkipMarkRestore)
  {
    mergestate->mj_ExtraMarks = true;
  }
  else
  {
    mergestate->mj_ExtraMarks = false;
  }

     
                                                  
     
  ExecInitResultTupleSlotTL(&mergestate->js.ps, &TTSOpsVirtual);
  ExecAssignProjectionInfo(&mergestate->js.ps, NULL);

     
                                
     
  innerOps = ExecGetResultSlotOps(innerPlanState(mergestate), NULL);
  mergestate->mj_MarkedTupleSlot = ExecInitExtraTupleSlot(estate, innerDesc, innerOps);

     
                                  
     
  mergestate->js.ps.qual = ExecInitQual(node->join.plan.qual, (PlanState *)mergestate);
  mergestate->js.joinqual = ExecInitQual(node->join.joinqual, (PlanState *)mergestate);
                                      

     
                                                                         
     
  mergestate->js.single_match = (node->join.inner_unique || node->join.jointype == JOIN_SEMI);

                                                     
  switch (node->join.jointype)
  {
  case JOIN_INNER:
  case JOIN_SEMI:
    mergestate->mj_FillOuter = false;
    mergestate->mj_FillInner = false;
    break;
  case JOIN_LEFT:
  case JOIN_ANTI:
    mergestate->mj_FillOuter = true;
    mergestate->mj_FillInner = false;
    mergestate->mj_NullInnerTupleSlot = ExecInitNullTupleSlot(estate, innerDesc, &TTSOpsVirtual);
    break;
  case JOIN_RIGHT:
    mergestate->mj_FillOuter = false;
    mergestate->mj_FillInner = true;
    mergestate->mj_NullOuterTupleSlot = ExecInitNullTupleSlot(estate, outerDesc, &TTSOpsVirtual);

       
                                                               
                                                              
       
    if (!check_constant_qual(node->join.joinqual, &mergestate->mj_ConstFalseJoin))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("RIGHT JOIN is only supported with merge-joinable join conditions")));
    }
    break;
  case JOIN_FULL:
    mergestate->mj_FillOuter = true;
    mergestate->mj_FillInner = true;
    mergestate->mj_NullOuterTupleSlot = ExecInitNullTupleSlot(estate, outerDesc, &TTSOpsVirtual);
    mergestate->mj_NullInnerTupleSlot = ExecInitNullTupleSlot(estate, innerDesc, &TTSOpsVirtual);

       
                                                               
                                                              
       
    if (!check_constant_qual(node->join.joinqual, &mergestate->mj_ConstFalseJoin))
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("FULL JOIN is only supported with merge-joinable join conditions")));
    }
    break;
  default:
    elog(ERROR, "unrecognized join type: %d", (int)node->join.jointype);
  }

     
                                  
     
  mergestate->mj_NumClauses = list_length(node->mergeclauses);
  mergestate->mj_Clauses = MJExamineQuals(node->mergeclauses, node->mergeFamilies, node->mergeCollations, node->mergeStrategies, node->mergeNullsFirst, (PlanState *)mergestate);

     
                           
     
  mergestate->mj_JoinState = EXEC_MJ_INITIALIZE_OUTER;
  mergestate->mj_MatchedOuter = false;
  mergestate->mj_MatchedInner = false;
  mergestate->mj_OuterTupleSlot = NULL;
  mergestate->mj_InnerTupleSlot = NULL;

     
                               
     
  MJ1_printf("ExecInitMergeJoin: %s\n", "node initialized");

  return mergestate;
}

                                                                    
                     
   
                
                                                
                                                                    
   
void
ExecEndMergeJoin(MergeJoinState *node)
{
  MJ1_printf("ExecEndMergeJoin: %s\n", "ending node processing");

     
                          
     
  ExecFreeExprContext(&node->js.ps);

     
                               
     
  ExecClearTuple(node->js.ps.ps_ResultTupleSlot);
  ExecClearTuple(node->mj_MarkedTupleSlot);

     
                            
     
  ExecEndNode(innerPlanState(node));
  ExecEndNode(outerPlanState(node));

  MJ1_printf("ExecEndMergeJoin: %s\n", "node processing ended");
}

void
ExecReScanMergeJoin(MergeJoinState *node)
{
  ExecClearTuple(node->mj_MarkedTupleSlot);

  node->mj_JoinState = EXEC_MJ_INITIALIZE_OUTER;
  node->mj_MatchedOuter = false;
  node->mj_MatchedInner = false;
  node->mj_OuterTupleSlot = NULL;
  node->mj_InnerTupleSlot = NULL;

     
                                                                          
                         
     
  if (node->js.ps.lefttree->chgParam == NULL)
  {
    ExecReScan(node->js.ps.lefttree);
  }
  if (node->js.ps.righttree->chgParam == NULL)
  {
    ExecReScan(node->js.ps.righttree);
  }
}
