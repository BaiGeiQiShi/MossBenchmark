                                                                            
   
                    
                                      
   
                                                                         
                                                                        
   
                  
                                          
   
         
                                                                            
                                                                         
                                                                              
                                                                              
                
   
                                                                            
   
#include "postgres.h"

#include "access/relation.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/dependency.h"
#include "catalog/pg_type.h"
#include "commands/trigger.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "parser/analyze.h"
#include "parser/parse_coerce.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteDefine.h"
#include "rewrite/rewriteHandler.h"
#include "rewrite/rewriteManip.h"
#include "rewrite/rowsecurity.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"

                                                                
typedef struct rewrite_event
{
  Oid relation;                                    
  CmdType event;                               
} rewrite_event;

typedef struct acquireLocksOnSubLinks_context
{
  bool for_execute;                                            
} acquireLocksOnSubLinks_context;

static bool
acquireLocksOnSubLinks(Node *node, acquireLocksOnSubLinks_context *context);
static Query *
rewriteRuleAction(Query *parsetree, Query *rule_action, Node *rule_qual, int rt_index, CmdType event, bool *returning_flag);
static List *
adjustJoinTreeList(Query *parsetree, bool removert, int rt_index);
static List *
rewriteTargetListIU(List *targetList, CmdType commandType, OverridingKind override, Relation target_relation, int result_rti);
static TargetEntry *
process_matched_tle(TargetEntry *src_tle, TargetEntry *prior_tle, const char *attrName);
static Node *
get_assignment_input(Node *node);
static bool
rewriteValuesRTE(Query *parsetree, RangeTblEntry *rte, int rti, Relation target_relation);
static void
rewriteValuesRTEToNulls(Query *parsetree, RangeTblEntry *rte);
static void
markQueryForLocking(Query *qry, Node *jtnode, LockClauseStrength strength, LockWaitPolicy waitPolicy, bool pushedDown);
static List *
matchLocks(CmdType event, RuleLock *rulelocks, int varno, Query *parsetree, bool *hasUpdate);
static Query *
fireRIRrules(Query *parsetree, List *activeRIRs);
static bool
view_has_instead_trigger(Relation view, CmdType event);
static Bitmapset *
adjust_view_column_set(Bitmapset *cols, List *targetlist);

   
                         
                                                                         
                                                                             
                                                                
   
                                                                           
                                                                            
                           
   
                                                                        
                                                                         
                                                                         
                                                                          
                                                                          
   
                                                                           
                                                                            
                                                                      
                                                                            
                                                                        
   
                                                                           
                                                                          
   
                                                                              
                                                                           
                                                                              
                                                                          
                                                                       
   
                                                                          
                                                                             
                                                                          
                                                                           
                                                                            
                                                                            
                                                                              
   
                                                                     
                                                                              
                                                                       
                                                                   
   
void
AcquireRewriteLocks(Query *parsetree, bool forExecute, bool forUpdatePushedDown)
{
  ListCell *l;
  int rt_index;
  acquireLocksOnSubLinks_context context;

  context.for_execute = forExecute;

     
                                                     
     
  rt_index = 0;
  foreach (l, parsetree->rtable)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(l);
    Relation rel;
    LOCKMODE lockmode;
    List *newaliasvars;
    Index curinputvarno;
    RangeTblEntry *curinputrte;
    ListCell *ll;

    ++rt_index;
    switch (rte->rtekind)
    {
    case RTE_RELATION:

         
                                                                     
                                                                 
                                                                
                    
         
                                                                 
                          
         
      if (!forExecute)
      {
        lockmode = AccessShareLock;
      }
      else if (forUpdatePushedDown)
      {
                                                                 
        if (rte->rellockmode == AccessShareLock)
        {
          rte->rellockmode = RowShareLock;
        }
        lockmode = rte->rellockmode;
      }
      else
      {
        lockmode = rte->rellockmode;
      }

      rel = table_open(rte->relid, lockmode);

         
                                                                    
                                                           
         
      rte->relkind = rel->rd_rel->relkind;

      table_close(rel, NoLock);
      break;

    case RTE_JOIN:

         
                                                                   
                                                              
                   
         
                                                                
                                                                  
                           
         
      newaliasvars = NIL;
      curinputvarno = 0;
      curinputrte = NULL;
      foreach (ll, rte->joinaliasvars)
      {
        Var *aliasitem = (Var *)lfirst(ll);
        Var *aliasvar = aliasitem;

                                                
        aliasvar = (Var *)strip_implicit_coercions((Node *)aliasvar);

           
                                                             
                                                                   
                                                                  
                                                                  
                                                 
           
        if (aliasvar && IsA(aliasvar, Var))
        {
             
                                                            
                                                                 
                                                                
                                                               
                                                                 
                                                               
                              
             
          Assert(aliasvar->varlevelsup == 0);
          if (aliasvar->varno != curinputvarno)
          {
            curinputvarno = aliasvar->varno;
            if (curinputvarno >= rt_index)
            {
              elog(ERROR, "unexpected varno %d in JOIN RTE %d", curinputvarno, rt_index);
            }
            curinputrte = rt_fetch(curinputvarno, parsetree->rtable);
          }
          if (get_rte_attribute_is_dropped(curinputrte, aliasvar->varattno))
          {
                                                         
            aliasitem = NULL;
          }
        }
        newaliasvars = lappend(newaliasvars, aliasitem);
      }
      rte->joinaliasvars = newaliasvars;
      break;

    case RTE_SUBQUERY:

         
                                                              
                                                      
         
      AcquireRewriteLocks(rte->subquery, forExecute, (forUpdatePushedDown || get_parse_rowmark(parsetree, rt_index) != NULL));
      break;

    default:
                                      
      break;
    }
  }

                                       
  foreach (l, parsetree->cteList)
  {
    CommonTableExpr *cte = (CommonTableExpr *)lfirst(l);

    AcquireRewriteLocks((Query *)cte->ctequery, forExecute, false);
  }

     
                                                                           
                             
     
  if (parsetree->hasSubLinks)
  {
    query_tree_walker(parsetree, acquireLocksOnSubLinks, &context, QTW_IGNORE_RC_SUBQUERIES);
  }
}

   
                                                             
   
static bool
acquireLocksOnSubLinks(Node *node, acquireLocksOnSubLinks_context *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, SubLink))
  {
    SubLink *sub = (SubLink *)node;

                             
    AcquireRewriteLocks((Query *)sub->subselect, context->for_execute, false);
                                                          
  }

     
                                                                          
                                                
     
  return expression_tree_walker(node, acquireLocksOnSubLinks, context);
}

   
                       
                                                                     
                            
   
                    
                              
                                              
                                                                 
                                                            
                              
                     
                                                                            
                                      
                 
                                 
   
static Query *
rewriteRuleAction(Query *parsetree, Query *rule_action, Node *rule_qual, int rt_index, CmdType event, bool *returning_flag)
{
  int current_varno, new_varno;
  int rt_length;
  Query *sub_action;
  Query **sub_action_ptr;
  acquireLocksOnSubLinks_context context;

  context.for_execute = true;

     
                                                                           
                                                             
     
  rule_action = copyObject(rule_action);
  rule_qual = copyObject(rule_qual);

     
                                                                   
     
  AcquireRewriteLocks(rule_action, true, false);
  (void)acquireLocksOnSubLinks(rule_qual, &context);

  current_varno = rt_index;
  rt_length = list_length(parsetree->rtable);
  new_varno = PRS2_NEW_VARNO + rt_length;

     
                                                                            
                                                  
     
                                                                          
                                                                            
                                
     
  sub_action = getInsertSelectQuery(rule_action, &sub_action_ptr);

  OffsetVarNodes((Node *)sub_action, rt_length, 0);
  OffsetVarNodes(rule_qual, rt_length, 0);
                                                               
  ChangeVarNodes((Node *)sub_action, PRS2_OLD_VARNO + rt_length, rt_index, 0);
  ChangeVarNodes(rule_qual, PRS2_OLD_VARNO + rt_length, rt_index, 0);

     
                                                                         
                                                                         
                                                                           
                                                     
     
                                                                    
                                     
     
                                                                          
                                                                           
                                           
     
                                                                          
                                                                             
                                                                          
                                                                          
                                                                        
                                                            
     
                                                                           
                                                                          
                                                                         
     
                                                                           
                                                                           
                                                  
     
                                                                           
                                                                       
                              
     
  sub_action->rtable = list_concat(copyObject(parsetree->rtable), sub_action->rtable);

     
                                                                         
                                                     
     
  if (parsetree->hasSubLinks && !sub_action->hasSubLinks)
  {
    ListCell *lc;

    foreach (lc, parsetree->rtable)
    {
      RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);

      switch (rte->rtekind)
      {
      case RTE_RELATION:
        sub_action->hasSubLinks = checkExprHasSubLink((Node *)rte->tablesample);
        break;
      case RTE_FUNCTION:
        sub_action->hasSubLinks = checkExprHasSubLink((Node *)rte->functions);
        break;
      case RTE_TABLEFUNC:
        sub_action->hasSubLinks = checkExprHasSubLink((Node *)rte->tablefunc);
        break;
      case RTE_VALUES:
        sub_action->hasSubLinks = checkExprHasSubLink((Node *)rte->values_lists);
        break;
      default:
                                                            
        break;
      }
      if (sub_action->hasSubLinks)
      {
        break;                                      
      }
    }
  }

     
                                                                         
                                                                         
                                                                          
                                                                            
                                                         
     
  sub_action->hasRowSecurity |= parsetree->hasRowSecurity;

     
                                                                         
                                                                           
                                                                           
                                                                    
                                                                          
                                                                           
                                                                             
                                                                           
                                                
     
                                                                          
                       
     
  if (sub_action->commandType != CMD_UTILITY)
  {
    bool keeporig;
    List *newjointree;

    Assert(sub_action->jointree != NULL);
    keeporig = (!rangeTableEntry_used((Node *)sub_action->jointree, rt_index, 0)) && (rangeTableEntry_used(rule_qual, rt_index, 0) || rangeTableEntry_used(parsetree->jointree->quals, rt_index, 0));
    newjointree = adjustJoinTreeList(parsetree, !keeporig, rt_index);
    if (newjointree != NIL)
    {
         
                                                                        
                                                                       
                                                                
                                   
         
      if (sub_action->setOperations != NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("conditional UNION/INTERSECT/EXCEPT statements are not implemented")));
      }

      sub_action->jointree->fromlist = list_concat(newjointree, sub_action->jointree->fromlist);

         
                                                                      
                                                         
         
      if (parsetree->hasSubLinks && !sub_action->hasSubLinks)
      {
        sub_action->hasSubLinks = checkExprHasSubLink((Node *)newjointree);
      }
    }
  }

     
                                                                             
                                              
     
  if (parsetree->cteList != NIL && sub_action->commandType != CMD_UTILITY)
  {
    ListCell *lc;

       
                                                                           
                                                                           
                                                              
       
                                                                     
                                                                       
                                                                           
                                                                          
                                                 
       
    foreach (lc, parsetree->cteList)
    {
      CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);
      ListCell *lc2;

      foreach (lc2, sub_action->cteList)
      {
        CommonTableExpr *cte2 = (CommonTableExpr *)lfirst(lc2);

        if (strcmp(cte->ctename, cte2->ctename) == 0)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("WITH query name \"%s\" appears in both a rule action and the query being rewritten", cte->ctename)));
        }
      }
    }

                                                
    sub_action->cteList = list_concat(sub_action->cteList, copyObject(parsetree->cteList));
                                                         
    sub_action->hasRecursive |= parsetree->hasRecursive;
    sub_action->hasModifyingCTE |= parsetree->hasModifyingCTE;

       
                                                                          
                                                                  
                                                                         
                                                                        
                                  
       
                                                                           
                                                                         
                                                                          
                                                                    
       
    if (sub_action->hasModifyingCTE && rule_action != sub_action)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("INSERT...SELECT rule actions are not supported for queries having data-modifying statements in WITH")));
    }
  }

     
                                                                            
                                                                            
                      
     
  AddQual(sub_action, rule_qual);

  AddQual(sub_action, parsetree->jointree->quals);

     
                                                                         
                                              
     
                                                                             
                                                                          
                                      
     
  if ((event == CMD_INSERT || event == CMD_UPDATE) && sub_action->commandType != CMD_UTILITY)
  {
    sub_action = (Query *)ReplaceVarsFromTargetList((Node *)sub_action, new_varno, 0, rt_fetch(new_varno, sub_action->rtable), parsetree->targetList, (event == CMD_UPDATE) ? REPLACEVARS_CHANGE_VARNO : REPLACEVARS_SUBSTITUTE_NULL, current_varno, NULL);
    if (sub_action_ptr)
    {
      *sub_action_ptr = sub_action;
    }
    else
    {
      rule_action = sub_action;
    }
  }

     
                                                                             
                                                                          
                                                                          
                                                
     
  if (!parsetree->returningList)
  {
    rule_action->returningList = NIL;
  }
  else if (rule_action->returningList)
  {
    if (*returning_flag)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot have RETURNING lists in multiple rules")));
    }
    *returning_flag = true;
    rule_action->returningList = (List *)ReplaceVarsFromTargetList((Node *)parsetree->returningList, parsetree->resultRelation, 0, rt_fetch(parsetree->resultRelation, parsetree->rtable), rule_action->returningList, REPLACEVARS_REPORT_ERROR, 0, &rule_action->hasSubLinks);

       
                                                                         
                                                                 
       
    if (parsetree->hasSubLinks && !rule_action->hasSubLinks)
    {
      rule_action->hasSubLinks = checkExprHasSubLink((Node *)rule_action->returningList);
    }
  }

  return rule_action;
}

   
                                                                        
                                                                             
                                                                              
                                                                             
                                                                             
                                       
   
static List *
adjustJoinTreeList(Query *parsetree, bool removert, int rt_index)
{
  List *newjointree = copyObject(parsetree->jointree->fromlist);
  ListCell *l;

  if (removert)
  {
    foreach (l, newjointree)
    {
      RangeTblRef *rtr = lfirst(l);

      if (IsA(rtr, RangeTblRef) && rtr->rtindex == rt_index)
      {
        newjointree = list_delete_ptr(newjointree, rtr);

           
                                                                     
           
        break;
      }
    }
  }
  return newjointree;
}

   
                                                                             
   
                                            
   
                                                                         
                                                                             
                                                                         
                                                                           
                                                                            
                                                                           
                
   
                                                                           
                                                                          
                                                                             
                                                                             
                                                                          
                                                                        
                                                                              
                                                                            
                                                                               
                                                                       
                                                                         
                 
   
                                                                             
                                                                        
                                                     
                                                
                                                                           
                                                          
                                                                  
   
                                                                             
                                                    
   
                                                                      
                                                                           
                                                                          
                                                                  
   
                                                                           
                                                                         
                                                                          
                                                                          
                                                                            
                    
   
static List *
rewriteTargetListIU(List *targetList, CmdType commandType, OverridingKind override, Relation target_relation, int result_rti)
{
  TargetEntry **new_tles;
  List *new_tlist = NIL;
  List *junk_tlist = NIL;
  Form_pg_attribute att_tup;
  int attrno, next_junk_attrno, numattrs;
  ListCell *temp;

     
                                                                             
                                                                          
                                                                           
                    
     
                                                                           
                                                     
     
  numattrs = RelationGetNumberOfAttributes(target_relation);
  new_tles = (TargetEntry **)palloc0(numattrs * sizeof(TargetEntry *));
  next_junk_attrno = numattrs + 1;

  foreach (temp, targetList)
  {
    TargetEntry *old_tle = (TargetEntry *)lfirst(temp);

    if (!old_tle->resjunk)
    {
                                                 
      attrno = old_tle->resno;
      if (attrno < 1 || attrno > numattrs)
      {
        elog(ERROR, "bogus resno %d in targetlist", attrno);
      }
      att_tup = TupleDescAttr(target_relation->rd_att, attrno - 1);

                                                       
      if (att_tup->attisdropped)
      {
        continue;
      }

                                                             
      new_tles[attrno - 1] = process_matched_tle(old_tle, new_tles[attrno - 1], NameStr(att_tup->attname));
    }
    else
    {
         
                                                                       
                                           
         
                                                                       
                                                                       
                                    
         

                                                             
      if (old_tle->resno != next_junk_attrno)
      {
        old_tle = flatCopyTargetEntry(old_tle);
        old_tle->resno = next_junk_attrno;
      }
      junk_tlist = lappend(junk_tlist, old_tle);
      next_junk_attrno++;
    }
  }

  for (attrno = 1; attrno <= numattrs; attrno++)
  {
    TargetEntry *new_tle = new_tles[attrno - 1];
    bool apply_default;

    att_tup = TupleDescAttr(target_relation->rd_att, attrno - 1);

                                                     
    if (att_tup->attisdropped)
    {
      continue;
    }

       
                                                                          
                                                                        
                                                  
       
    apply_default = ((new_tle == NULL && commandType == CMD_INSERT) || (new_tle && new_tle->expr && IsA(new_tle->expr, SetToDefault)));

    if (commandType == CMD_INSERT)
    {
      if (att_tup->attidentity == ATTRIBUTE_IDENTITY_ALWAYS && !apply_default)
      {
        if (override != OVERRIDING_SYSTEM_VALUE)
        {
          ereport(ERROR, (errcode(ERRCODE_GENERATED_ALWAYS), errmsg("cannot insert into column \"%s\"", NameStr(att_tup->attname)), errdetail("Column \"%s\" is an identity column defined as GENERATED ALWAYS.", NameStr(att_tup->attname)), errhint("Use OVERRIDING SYSTEM VALUE to override.")));
        }
      }

      if (att_tup->attidentity == ATTRIBUTE_IDENTITY_BY_DEFAULT && override == OVERRIDING_USER_VALUE)
      {
        apply_default = true;
      }

      if (att_tup->attgenerated && !apply_default)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("cannot insert into column \"%s\"", NameStr(att_tup->attname)), errdetail("Column \"%s\" is a generated column.", NameStr(att_tup->attname))));
      }
    }

    if (commandType == CMD_UPDATE)
    {
      if (att_tup->attidentity == ATTRIBUTE_IDENTITY_ALWAYS && new_tle && !apply_default)
      {
        ereport(ERROR, (errcode(ERRCODE_GENERATED_ALWAYS), errmsg("column \"%s\" can only be updated to DEFAULT", NameStr(att_tup->attname)), errdetail("Column \"%s\" is an identity column defined as GENERATED ALWAYS.", NameStr(att_tup->attname))));
      }

      if (att_tup->attgenerated && new_tle && !apply_default)
      {
        ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("column \"%s\" can only be updated to DEFAULT", NameStr(att_tup->attname)), errdetail("Column \"%s\" is a generated column.", NameStr(att_tup->attname))));
      }
    }

    if (att_tup->attgenerated)
    {
         
                                                           
         
      new_tle = NULL;
    }
    else if (apply_default)
    {
      Node *new_expr;

      new_expr = build_column_default(target_relation, attrno);

         
                                                                      
                                                                        
                                                                        
                                                                        
                                                         
         
      if (!new_expr)
      {
        if (commandType == CMD_INSERT)
        {
          new_tle = NULL;
        }
        else
        {
          new_expr = (Node *)makeConst(att_tup->atttypid, -1, att_tup->attcollation, att_tup->attlen, (Datum)0, true,             
              att_tup->attbyval);
                                                             
          new_expr = coerce_to_domain(new_expr, InvalidOid, -1, att_tup->atttypid, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1, false);
        }
      }

      if (new_expr)
      {
        new_tle = makeTargetEntry((Expr *)new_expr, attrno, pstrdup(NameStr(att_tup->attname)), false);
      }
    }

       
                                                                        
                                                 
       
    if (new_tle == NULL && commandType == CMD_UPDATE && target_relation->rd_rel->relkind == RELKIND_VIEW && view_has_instead_trigger(target_relation, CMD_UPDATE))
    {
      Node *new_expr;

      new_expr = (Node *)makeVar(result_rti, attrno, att_tup->atttypid, att_tup->atttypmod, att_tup->attcollation, 0);

      new_tle = makeTargetEntry((Expr *)new_expr, attrno, pstrdup(NameStr(att_tup->attname)), false);
    }

    if (new_tle)
    {
      new_tlist = lappend(new_tlist, new_tle);
    }
  }

  pfree(new_tles);

  return list_concat(new_tlist, junk_tlist);
}

   
                                                                         
   
                                                                            
                                                                       
   
static TargetEntry *
process_matched_tle(TargetEntry *src_tle, TargetEntry *prior_tle, const char *attrName)
{
  TargetEntry *result;
  CoerceToDomain *coerce_expr = NULL;
  Node *src_expr;
  Node *prior_expr;
  Node *src_input;
  Node *prior_input;
  Node *priorbottom;
  Node *newexpr;

  if (prior_tle == NULL)
  {
       
                                                                        
       
    return src_tle;
  }

               
                                                                    
                                                                         
                                                                    
                          
                                                                
                                                               
                                                                 
                                                                 
                                                                       
                                                                             
                      
                                       
                                                   
                                                      
                                                                 
                                                
     
                                                                 
                                                                
                                           
     
                                                                          
                                                                          
                                                                       
                                                                             
                                                                               
                                                                          
                                                                                 
               
     
  src_expr = (Node *)src_tle->expr;
  prior_expr = (Node *)prior_tle->expr;

  if (src_expr && IsA(src_expr, CoerceToDomain) && prior_expr && IsA(prior_expr, CoerceToDomain) && ((CoerceToDomain *)src_expr)->resulttype == ((CoerceToDomain *)prior_expr)->resulttype)
  {
                                                                         
    coerce_expr = (CoerceToDomain *)src_expr;
    src_expr = (Node *)((CoerceToDomain *)src_expr)->arg;
    prior_expr = (Node *)((CoerceToDomain *)prior_expr)->arg;
  }

  src_input = get_assignment_input(src_expr);
  prior_input = get_assignment_input(prior_expr);
  if (src_input == NULL || prior_input == NULL || exprType(src_expr) != exprType(prior_expr))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("multiple assignments to same column \"%s\"", attrName)));
  }

     
                                                                            
     
  priorbottom = prior_input;
  for (;;)
  {
    Node *newbottom = get_assignment_input(priorbottom);

    if (newbottom == NULL)
    {
      break;                                       
    }
    priorbottom = newbottom;
  }
  if (!equal(priorbottom, src_input))
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("multiple assignments to same column \"%s\"", attrName)));
  }

     
                           
     
  if (IsA(src_expr, FieldStore))
  {
    FieldStore *fstore = makeNode(FieldStore);

    if (IsA(prior_expr, FieldStore))
    {
                           
      memcpy(fstore, prior_expr, sizeof(FieldStore));
      fstore->newvals = list_concat(list_copy(((FieldStore *)prior_expr)->newvals), list_copy(((FieldStore *)src_expr)->newvals));
      fstore->fieldnums = list_concat(list_copy(((FieldStore *)prior_expr)->fieldnums), list_copy(((FieldStore *)src_expr)->fieldnums));
    }
    else
    {
                                       
      memcpy(fstore, src_expr, sizeof(FieldStore));
      fstore->arg = (Expr *)prior_expr;
    }
    newexpr = (Node *)fstore;
  }
  else if (IsA(src_expr, SubscriptingRef))
  {
    SubscriptingRef *sbsref = makeNode(SubscriptingRef);

    memcpy(sbsref, src_expr, sizeof(SubscriptingRef));
    sbsref->refexpr = (Expr *)prior_expr;
    newexpr = (Node *)sbsref;
  }
  else
  {
    elog(ERROR, "cannot happen");
    newexpr = NULL;
  }

  if (coerce_expr)
  {
                                     
    CoerceToDomain *newcoerce = makeNode(CoerceToDomain);

    memcpy(newcoerce, coerce_expr, sizeof(CoerceToDomain));
    newcoerce->arg = (Expr *)newexpr;
    newexpr = (Node *)newcoerce;
  }

  result = flatCopyTargetEntry(src_tle);
  result->expr = (Expr *)newexpr;
  return result;
}

   
                                                                     
   
static Node *
get_assignment_input(Node *node)
{
  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, FieldStore))
  {
    FieldStore *fstore = (FieldStore *)node;

    return (Node *)fstore->arg;
  }
  else if (IsA(node, SubscriptingRef))
  {
    SubscriptingRef *sbsref = (SubscriptingRef *)node;

    if (sbsref->refassgnexpr == NULL)
    {
      return NULL;
    }

    return (Node *)sbsref->refexpr;
  }

  return NULL;
}

   
                                                               
   
                                                  
   
Node *
build_column_default(Relation rel, int attrno)
{
  TupleDesc rd_att = rel->rd_att;
  Form_pg_attribute att_tup = TupleDescAttr(rd_att, attrno - 1);
  Oid atttype = att_tup->atttypid;
  int32 atttypmod = att_tup->atttypmod;
  Node *expr = NULL;
  Oid exprtype;

  if (att_tup->attidentity)
  {
    NextValueExpr *nve = makeNode(NextValueExpr);

    nve->seqid = getOwnedSequence(RelationGetRelid(rel), attrno);
    nve->typeId = att_tup->atttypid;

    return (Node *)nve;
  }

     
                                                            
     
  if (att_tup->atthasdef && rd_att->constr && rd_att->constr->num_defval > 0)
  {
    AttrDefault *defval = rd_att->constr->defval;
    int ndef = rd_att->constr->num_defval;

    while (--ndef >= 0)
    {
      if (attrno == defval[ndef].adnum)
      {
           
                                                                 
           
        expr = stringToNode(defval[ndef].adbin);
        break;
      }
    }
  }

     
                                                                            
                                
     
  if (expr == NULL && !att_tup->attgenerated)
  {
    expr = get_typdefault(atttype);
  }

  if (expr == NULL)
  {
    return NULL;                          
  }

     
                                                                         
                                                                       
                                                                             
                                                                  
                              
     
  exprtype = exprType(expr);

  expr = coerce_to_target_type(NULL,                             
      expr, exprtype, atttype, atttypmod, COERCION_ASSIGNMENT, COERCE_IMPLICIT_CAST, -1);
  if (expr == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH),
                       errmsg("column \"%s\" is of type %s"
                              " but default expression is of type %s",
                           NameStr(att_tup->attname), format_type_be(atttype), format_type_be(exprtype)),
                       errhint("You will need to rewrite or cast the expression.")));
  }

  return expr;
}

                                                     
static bool
searchForDefault(RangeTblEntry *rte)
{
  ListCell *lc;

  foreach (lc, rte->values_lists)
  {
    List *sublist = (List *)lfirst(lc);
    ListCell *lc2;

    foreach (lc2, sublist)
    {
      Node *col = (Node *)lfirst(lc2);

      if (IsA(col, SetToDefault))
      {
        return true;
      }
    }
  }
  return false;
}

   
                                                                            
                                                                         
                                                                         
                                                                    
   
                                                                       
                                                                            
                                                                        
                                                                            
                                                       
   
                                                                             
                                                                             
                                                 
   
                                                                             
                                                                              
                                                                             
                                                                           
                                                                         
                                                   
   
                                                                           
                   
   
static bool
rewriteValuesRTE(Query *parsetree, RangeTblEntry *rte, int rti, Relation target_relation)
{
  List *newValues;
  ListCell *lc;
  bool isAutoUpdatableView;
  bool allReplaced;
  int numattrs;
  int *attrnos;

                                                           
  Assert(parsetree->commandType == CMD_INSERT);
  Assert(rte->rtekind == RTE_VALUES);

     
                                                                         
                                                                       
                                                           
     
  if (!searchForDefault(rte))
  {
    return true;                    
  }

     
                                                                           
                                                                           
                                                                           
                                                                        
                       
     
  numattrs = list_length(linitial(rte->values_lists));
  attrnos = (int *)palloc0(numattrs * sizeof(int));

  foreach (lc, parsetree->targetList)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(lc);

    if (IsA(tle->expr, Var))
    {
      Var *var = (Var *)tle->expr;

      if (var->varno == rti)
      {
        int attrno = var->varattno;

        Assert(attrno >= 1 && attrno <= numattrs);
        attrnos[attrno - 1] = tle->resno;
      }
    }
  }

     
                                                                           
                                                                         
           
     
  isAutoUpdatableView = false;
  if (target_relation->rd_rel->relkind == RELKIND_VIEW && !view_has_instead_trigger(target_relation, CMD_INSERT))
  {
    List *locks;
    bool hasUpdate;
    bool found;
    ListCell *l;

                                                   
    locks = matchLocks(CMD_INSERT, target_relation->rd_rules, parsetree->resultRelation, parsetree, &hasUpdate);

    found = false;
    foreach (l, locks)
    {
      RewriteRule *rule_lock = (RewriteRule *)lfirst(l);

      if (rule_lock->isInstead && rule_lock->qual == NULL)
      {
        found = true;
        break;
      }
    }

       
                                                                           
                                                                      
                       
       
    if (!found)
    {
      isAutoUpdatableView = true;
    }
  }

  newValues = NIL;
  allReplaced = true;
  foreach (lc, rte->values_lists)
  {
    List *sublist = (List *)lfirst(lc);
    List *newList = NIL;
    ListCell *lc2;
    int i;

    Assert(list_length(sublist) == numattrs);

    i = 0;
    foreach (lc2, sublist)
    {
      Node *col = (Node *)lfirst(lc2);
      int attrno = attrnos[i++];

      if (IsA(col, SetToDefault))
      {
        Form_pg_attribute att_tup;
        Node *new_expr;

        if (attrno == 0)
        {
          elog(ERROR, "cannot set value in column %d to DEFAULT", i);
        }
        Assert(attrno > 0 && attrno <= target_relation->rd_att->natts);
        att_tup = TupleDescAttr(target_relation->rd_att, attrno - 1);

        if (!att_tup->attisdropped)
        {
          new_expr = build_column_default(target_relation, attrno);
        }
        else
        {
          new_expr = NULL;                              
        }

           
                                                                     
                                                                      
                                                      
           
        if (!new_expr)
        {
          if (isAutoUpdatableView)
          {
                                           
            newList = lappend(newList, col);
            allReplaced = false;
            continue;
          }

          new_expr = (Node *)makeConst(att_tup->atttypid, -1, att_tup->attcollation, att_tup->attlen, (Datum)0, true,             
              att_tup->attbyval);
                                                             
          new_expr = coerce_to_domain(new_expr, InvalidOid, -1, att_tup->atttypid, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1, false);
        }
        newList = lappend(newList, new_expr);
      }
      else
      {
        newList = lappend(newList, col);
      }
    }
    newValues = lappend(newValues, newList);
  }
  rte->values_lists = newValues;

  pfree(attrnos);

  return allReplaced;
}

   
                                                                 
                                       
   
                                                                               
                                                                             
                                                                         
                                                                               
         
   
static void
rewriteValuesRTEToNulls(Query *parsetree, RangeTblEntry *rte)
{
  List *newValues;
  ListCell *lc;

  Assert(rte->rtekind == RTE_VALUES);
  newValues = NIL;
  foreach (lc, rte->values_lists)
  {
    List *sublist = (List *)lfirst(lc);
    List *newList = NIL;
    ListCell *lc2;

    foreach (lc2, sublist)
    {
      Node *col = (Node *)lfirst(lc2);

      if (IsA(col, SetToDefault))
      {
        SetToDefault *def = (SetToDefault *)col;

        newList = lappend(newList, makeNullConst(def->typeId, def->typeMod, def->collation));
      }
      else
      {
        newList = lappend(newList, col);
      }
    }
    newValues = lappend(newValues, newList);
  }
  rte->values_lists = newValues;
}

   
                                                                    
   
                                                                           
                                                                             
                                                                             
                                                                            
                
   
                                                                            
                                                                             
                                                                           
                                                
   
void
rewriteTargetListUD(Query *parsetree, RangeTblEntry *target_rte, Relation target_relation)
{
  Var *var = NULL;
  const char *attrname;
  TargetEntry *tle;

  if (target_relation->rd_rel->relkind == RELKIND_RELATION || target_relation->rd_rel->relkind == RELKIND_MATVIEW || target_relation->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
       
                                                                        
       
    var = makeVar(parsetree->resultRelation, SelfItemPointerAttributeNumber, TIDOID, -1, InvalidOid, 0);

    attrname = "ctid";
  }
  else if (target_relation->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
  {
       
                                                                    
       
    FdwRoutine *fdwroutine;

    fdwroutine = GetFdwRoutineForRelation(target_relation, false);

    if (fdwroutine->AddForeignUpdateTargets != NULL)
    {
      fdwroutine->AddForeignUpdateTargets(parsetree, target_rte, target_relation);
    }

       
                                                                           
                                                                           
                                                       
       
    if (target_relation->trigdesc && ((parsetree->commandType == CMD_UPDATE && (target_relation->trigdesc->trig_update_after_row || target_relation->trigdesc->trig_update_before_row)) || (parsetree->commandType == CMD_DELETE && (target_relation->trigdesc->trig_delete_after_row || target_relation->trigdesc->trig_delete_before_row))))
    {
      var = makeWholeRowVar(target_rte, parsetree->resultRelation, 0, false);

      attrname = "wholerow";
    }
  }

  if (var != NULL)
  {
    tle = makeTargetEntry((Expr *)var, list_length(parsetree->targetList) + 1, pstrdup(attrname), true);

    parsetree->targetList = lappend(parsetree->targetList, tle);
  }
}

   
                                                                               
                                                                    
   
void
fill_extraUpdatedCols(RangeTblEntry *target_rte, Relation target_relation)
{
  TupleDesc tupdesc = RelationGetDescr(target_relation);
  TupleConstr *constr = tupdesc->constr;

  target_rte->extraUpdatedCols = NULL;

  if (constr && constr->has_generated_stored)
  {
    for (int i = 0; i < constr->num_defval; i++)
    {
      AttrDefault *defval = &constr->defval[i];
      Node *expr;
      Bitmapset *attrs_used = NULL;

                                        
      if (!TupleDescAttr(tupdesc, defval->adnum - 1)->attgenerated)
      {
        continue;
      }

                                                             
      expr = stringToNode(defval->adbin);
      pull_varattnos(expr, 1, &attrs_used);

      if (bms_overlap(target_rte->updatedCols, attrs_used))
      {
        target_rte->extraUpdatedCols = bms_add_member(target_rte->extraUpdatedCols, defval->adnum - FirstLowInvalidHeapAttributeNumber);
      }
    }
  }
}

   
                
                                                            
   
static List *
matchLocks(CmdType event, RuleLock *rulelocks, int varno, Query *parsetree, bool *hasUpdate)
{
  List *matching_locks = NIL;
  int nlocks;
  int i;

  if (rulelocks == NULL)
  {
    return NIL;
  }

  if (parsetree->commandType != CMD_SELECT)
  {
    if (parsetree->resultRelation != varno)
    {
      return NIL;
    }
  }

  nlocks = rulelocks->numLocks;

  for (i = 0; i < nlocks; i++)
  {
    RewriteRule *oneLock = rulelocks->rules[i];

    if (oneLock->event == CMD_UPDATE)
    {
      *hasUpdate = true;
    }

       
                                                                   
                                                                      
                                                                           
                                              
       
    if (oneLock->event != CMD_SELECT)
    {
      if (SessionReplicationRole == SESSION_REPLICATION_ROLE_REPLICA)
      {
        if (oneLock->enabled == RULE_FIRES_ON_ORIGIN || oneLock->enabled == RULE_DISABLED)
        {
          continue;
        }
      }
      else                           
      {
        if (oneLock->enabled == RULE_FIRES_ON_REPLICA || oneLock->enabled == RULE_DISABLED)
        {
          continue;
        }
      }
    }

    if (oneLock->event == event)
    {
      if (parsetree->commandType != CMD_SELECT || rangeTableEntry_used((Node *)parsetree, varno, 0))
      {
        matching_locks = lappend(matching_locks, oneLock);
      }
    }
  }

  return matching_locks;
}

   
                                                
   
static Query *
ApplyRetrieveRule(Query *parsetree, RewriteRule *rule, int rt_index, Relation relation, List *activeRIRs)
{
  Query *rule_action;
  RangeTblEntry *rte, *subrte;
  RowMarkClause *rc;

  if (list_length(rule->actions) != 1)
  {
    elog(ERROR, "expected just one rule action");
  }
  if (rule->qual != NULL)
  {
    elog(ERROR, "cannot handle qualified ON SELECT rule");
  }

  if (rt_index == parsetree->resultRelation)
  {
       
                                                                         
                                                                     
                                                                          
                                                                          
                                  
       
                                                                          
                                    
       
                                                                          
                                                                      
                                                                      
                                                                           
                                                                           
                                                         
       
    if (parsetree->commandType == CMD_INSERT)
    {
      return parsetree;
    }
    else if (parsetree->commandType == CMD_UPDATE || parsetree->commandType == CMD_DELETE)
    {
      RangeTblEntry *newrte;
      Var *var;
      TargetEntry *tle;

      rte = rt_fetch(rt_index, parsetree->rtable);
      newrte = copyObject(rte);
      parsetree->rtable = lappend(parsetree->rtable, newrte);
      parsetree->resultRelation = list_length(parsetree->rtable);

         
                                                                         
                                                                      
                                      
         
      rte->requiredPerms = 0;
      rte->checkAsUser = InvalidOid;
      rte->selectedCols = NULL;
      rte->insertedCols = NULL;
      rte->updatedCols = NULL;
      rte->extraUpdatedCols = NULL;

         
                                                                       
                                                                      
                                                                
                                                                        
         
                                                                       
                                          
         
      parsetree->returningList = copyObject(parsetree->returningList);
      ChangeVarNodes((Node *)parsetree->returningList, rt_index, parsetree->resultRelation, 0);

         
                                                                        
                                                                   
                                                                     
                                                                      
         
      var = makeWholeRowVar(rte, rt_index, 0, false);
      tle = makeTargetEntry((Expr *)var, list_length(parsetree->targetList) + 1, pstrdup("wholerow"), true);

      parsetree->targetList = lappend(parsetree->targetList, tle);

                                                              
    }
    else
    {
      elog(ERROR, "unrecognized commandType: %d", (int)parsetree->commandType);
    }
  }

     
                                                                             
     
                                                                        
                                                                           
                                                                 
     
  rc = get_parse_rowmark(parsetree, rt_index);

     
                                                                           
                                                                          
                                                                          
     
  rule_action = copyObject(linitial(rule->actions));

  AcquireRewriteLocks(rule_action, true, (rc != NULL));

     
                                                                         
                                                                             
                                                             
     
  if (rc != NULL)
  {
    markQueryForLocking(rule_action, (Node *)rule_action->jointree, rc->strength, rc->waitPolicy, true);
  }

     
                                                             
     
                                                                             
                                                                         
                                                                             
                                                                             
               
     
  rule_action = fireRIRrules(rule_action, activeRIRs);

     
                                                                           
                                     
     
  rte = rt_fetch(rt_index, parsetree->rtable);

  rte->rtekind = RTE_SUBQUERY;
  rte->subquery = rule_action;
  rte->security_barrier = RelationIsSecurityView(relation);
                                                             
  rte->relid = InvalidOid;
  rte->relkind = 0;
  rte->rellockmode = 0;
  rte->tablesample = NULL;
  rte->inh = false;                                     

     
                                                                          
                                                                 
     
  subrte = rt_fetch(PRS2_OLD_VARNO, rule_action->rtable);
  Assert(subrte->relid == relation->rd_id);
  subrte->requiredPerms = rte->requiredPerms;
  subrte->checkAsUser = rte->checkAsUser;
  subrte->selectedCols = rte->selectedCols;
  subrte->insertedCols = rte->insertedCols;
  subrte->updatedCols = rte->updatedCols;
  subrte->extraUpdatedCols = rte->extraUpdatedCols;

  rte->requiredPerms = 0;                                             
  rte->checkAsUser = InvalidOid;
  rte->selectedCols = NULL;
  rte->insertedCols = NULL;
  rte->updatedCols = NULL;
  rte->extraUpdatedCols = NULL;

  return parsetree;
}

   
                                                                            
   
                                                                    
                                                          
   
                                                                           
                                                                         
                                                                           
                                                          
   
static void
markQueryForLocking(Query *qry, Node *jtnode, LockClauseStrength strength, LockWaitPolicy waitPolicy, bool pushedDown)
{
  if (jtnode == NULL)
  {
    return;
  }
  if (IsA(jtnode, RangeTblRef))
  {
    int rti = ((RangeTblRef *)jtnode)->rtindex;
    RangeTblEntry *rte = rt_fetch(rti, qry->rtable);

    if (rte->rtekind == RTE_RELATION)
    {
      applyLockingClause(qry, rti, strength, waitPolicy, pushedDown);
      rte->requiredPerms |= ACL_SELECT_FOR_UPDATE;
    }
    else if (rte->rtekind == RTE_SUBQUERY)
    {
      applyLockingClause(qry, rti, strength, waitPolicy, pushedDown);
                                                                         
      markQueryForLocking(rte->subquery, (Node *)rte->subquery->jointree, strength, waitPolicy, true);
    }
                                                      
  }
  else if (IsA(jtnode, FromExpr))
  {
    FromExpr *f = (FromExpr *)jtnode;
    ListCell *l;

    foreach (l, f->fromlist)
    {
      markQueryForLocking(qry, lfirst(l), strength, waitPolicy, pushedDown);
    }
  }
  else if (IsA(jtnode, JoinExpr))
  {
    JoinExpr *j = (JoinExpr *)jtnode;

    markQueryForLocking(qry, j->larg, strength, waitPolicy, pushedDown);
    markQueryForLocking(qry, j->rarg, strength, waitPolicy, pushedDown);
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", (int)nodeTag(jtnode));
  }
}

   
                      
                                                                        
                      
   
                                                                         
                                                                         
                                   
   
                                                                           
                                                                        
                                                                      
   
static bool
fireRIRonSubLink(Node *node, List *activeRIRs)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, SubLink))
  {
    SubLink *sub = (SubLink *)node;

                             
    sub->subselect = (Node *)fireRIRrules((Query *)sub->subselect, activeRIRs);
                                                          
  }

     
                                                                             
                                      
     
  return expression_tree_walker(node, fireRIRonSubLink, (void *)activeRIRs);
}

   
                  
                                                                   
   
                                                                          
                                               
   
static Query *
fireRIRrules(Query *parsetree, List *activeRIRs)
{
  int origResultRelation = parsetree->resultRelation;
  int rt_index;
  ListCell *lc;

     
                                                                            
                                      
     
  rt_index = 0;
  while (rt_index < list_length(parsetree->rtable))
  {
    RangeTblEntry *rte;
    Relation rel;
    List *locks;
    RuleLock *rules;
    RewriteRule *rule;
    int i;

    ++rt_index;

    rte = rt_fetch(rt_index, parsetree->rtable);

       
                                                                         
                                                                   
                                                     
       
    if (rte->rtekind == RTE_SUBQUERY)
    {
      rte->subquery = fireRIRrules(rte->subquery, activeRIRs);
      continue;
    }

       
                                                                    
       
    if (rte->rtekind != RTE_RELATION)
    {
      continue;
    }

       
                                                                    
                                                                          
                                                   
       
                                                                          
                                                                          
                                                                          
                                                         
       
    if (rte->relkind == RELKIND_MATVIEW)
    {
      continue;
    }

       
                                                                       
                                                                         
                                                                           
                                                              
                                       
       
    if (parsetree->onConflict && rt_index == parsetree->onConflict->exclRelIndex)
    {
      continue;
    }

       
                                                                       
                                                                       
                                                                       
                                                                          
                                      
       
    if (rt_index != parsetree->resultRelation && !rangeTableEntry_used((Node *)parsetree, rt_index, 0))
    {
      continue;
    }

       
                                                            
                                                                     
       
    if (rt_index == parsetree->resultRelation && rt_index != origResultRelation)
    {
      continue;
    }

       
                                                         
                                                               
       
    rel = table_open(rte->relid, NoLock);

       
                                                
       
    rules = rel->rd_rules;
    if (rules != NULL)
    {
      locks = NIL;
      for (i = 0; i < rules->numLocks; i++)
      {
        rule = rules->rules[i];
        if (rule->event != CMD_SELECT)
        {
          continue;
        }

        locks = lappend(locks, rule);
      }

         
                                                                        
         
      if (locks != NIL)
      {
        ListCell *l;

        if (list_member_oid(activeRIRs, RelationGetRelid(rel)))
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("infinite recursion detected in rules for relation \"%s\"", RelationGetRelationName(rel))));
        }
        activeRIRs = lcons_oid(RelationGetRelid(rel), activeRIRs);

        foreach (l, locks)
        {
          rule = lfirst(l);

          parsetree = ApplyRetrieveRule(parsetree, rule, rt_index, rel, activeRIRs);
        }

        activeRIRs = list_delete_first(activeRIRs);
      }
    }

    table_close(rel, NoLock);
  }

                                       
  foreach (lc, parsetree->cteList)
  {
    CommonTableExpr *cte = (CommonTableExpr *)lfirst(lc);

    cte->ctequery = (Node *)fireRIRrules((Query *)cte->ctequery, activeRIRs);
  }

     
                                                                           
                             
     
  if (parsetree->hasSubLinks)
  {
    query_tree_walker(parsetree, fireRIRonSubLink, (void *)activeRIRs, QTW_IGNORE_RC_SUBQUERIES);
  }

     
                                                                        
                                                                        
                                                                            
                                                  
     
  rt_index = 0;
  foreach (lc, parsetree->rtable)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc);
    Relation rel;
    List *securityQuals;
    List *withCheckOptions;
    bool hasRowSecurity;
    bool hasSubLinks;

    ++rt_index;

                                                     
    if (rte->rtekind != RTE_RELATION || (rte->relkind != RELKIND_RELATION && rte->relkind != RELKIND_PARTITIONED_TABLE))
    {
      continue;
    }

    rel = table_open(rte->relid, NoLock);

       
                                                                      
       
    get_row_security_policies(parsetree, rte, rt_index, &securityQuals, &withCheckOptions, &hasRowSecurity, &hasSubLinks);

    if (securityQuals != NIL || withCheckOptions != NIL)
    {
      if (hasSubLinks)
      {
        acquireLocksOnSubLinks_context context;

           
                                                                    
                      
           
        if (list_member_oid(activeRIRs, RelationGetRelid(rel)))
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("infinite recursion detected in policy for relation \"%s\"", RelationGetRelationName(rel))));
        }

        activeRIRs = lcons_oid(RelationGetRelid(rel), activeRIRs);

           
                                                                    
                                                                       
                                                       
           
                                                                     
                                                                      
           
        context.for_execute = true;
        (void)acquireLocksOnSubLinks((Node *)securityQuals, &context);
        (void)acquireLocksOnSubLinks((Node *)withCheckOptions, &context);

           
                                                           
                                                                   
           
        expression_tree_walker((Node *)securityQuals, fireRIRonSubLink, (void *)activeRIRs);

        expression_tree_walker((Node *)withCheckOptions, fireRIRonSubLink, (void *)activeRIRs);

        activeRIRs = list_delete_first(activeRIRs);
      }

         
                                                                      
                                                                         
                                                                         
                                                                      
         
      rte->securityQuals = list_concat(securityQuals, rte->securityQuals);

      parsetree->withCheckOptions = list_concat(withCheckOptions, parsetree->withCheckOptions);
    }

       
                                                                     
                                                  
       
    if (hasRowSecurity)
    {
      parsetree->hasRowSecurity = true;
    }
    if (hasSubLinks)
    {
      parsetree->hasSubLinks = true;
    }

    table_close(rel, NoLock);
  }

  return parsetree;
}

   
                                                                       
                                                                        
                                                                           
                                                                          
                                                        
   
                                                                           
                                                                           
                                                                             
                                                                            
                                                         
   
static Query *
CopyAndAddInvertedQual(Query *parsetree, Node *rule_qual, int rt_index, CmdType event)
{
                                                                 
  Node *new_qual = copyObject(rule_qual);
  acquireLocksOnSubLinks_context context;

  context.for_execute = true;

     
                                                                           
                                                                         
                                                                            
                                                      
     
  (void)acquireLocksOnSubLinks(new_qual, &context);

                             
  ChangeVarNodes(new_qual, PRS2_OLD_VARNO, rt_index, 0);
                             
  if (event == CMD_INSERT || event == CMD_UPDATE)
  {
    new_qual = ReplaceVarsFromTargetList(new_qual, PRS2_NEW_VARNO, 0, rt_fetch(rt_index, parsetree->rtable), parsetree->targetList, (event == CMD_UPDATE) ? REPLACEVARS_CHANGE_VARNO : REPLACEVARS_SUBSTITUTE_NULL, rt_index, &parsetree->hasSubLinks);
  }
                                 
  AddInvertedQual(parsetree, new_qual);

  return parsetree;
}

   
               
                                                 
   
                    
                              
                                                            
                              
                                 
                     
                                                                     
                                      
                                                                         
                                      
                                                                        
                                                           
                 
                                                         
   
                                                                        
                                                                           
                                                                            
                                                                          
                                                                       
                                                                        
                                                                       
                                                               
   
static List *
fireRules(Query *parsetree, int rt_index, CmdType event, List *locks, bool *instead_flag, bool *returning_flag, Query **qual_product)
{
  List *results = NIL;
  ListCell *l;

  foreach (l, locks)
  {
    RewriteRule *rule_lock = (RewriteRule *)lfirst(l);
    Node *event_qual = rule_lock->qual;
    List *actions = rule_lock->actions;
    QuerySource qsrc;
    ListCell *r;

                                                         
    if (rule_lock->isInstead)
    {
      if (event_qual != NULL)
      {
        qsrc = QSRC_QUAL_INSTEAD_RULE;
      }
      else
      {
        qsrc = QSRC_INSTEAD_RULE;
        *instead_flag = true;                                 
      }
    }
    else
    {
      qsrc = QSRC_NON_INSTEAD_RULE;
    }

    if (qsrc == QSRC_QUAL_INSTEAD_RULE)
    {
         
                                                                      
                                                            
                                                                      
                                                                         
                                                                         
                                                                         
                                             
         
                                                                    
                                                                   
         
      if (!*instead_flag)
      {
        if (*qual_product == NULL)
        {
          *qual_product = copyObject(parsetree);
        }
        *qual_product = CopyAndAddInvertedQual(*qual_product, event_qual, rt_index, event);
      }
    }

                                                                        
    foreach (r, actions)
    {
      Query *rule_action = lfirst(r);

      if (rule_action->commandType == CMD_NOTHING)
      {
        continue;
      }

      rule_action = rewriteRuleAction(parsetree, rule_action, event_qual, rt_index, event, returning_flag);

      rule_action->querySource = qsrc;
      rule_action->canSetTag = false;                         

      results = lappend(results, rule_action);
    }
  }

  return results;
}

   
                                                              
   
                                                                          
                                       
   
                                                                          
                                                                           
   
Query *
get_view_query(Relation view)
{
  int i;

  Assert(view->rd_rel->relkind == RELKIND_VIEW);

  for (i = 0; i < view->rd_rules->numLocks; i++)
  {
    RewriteRule *rule = view->rd_rules->rules[i];

    if (rule->event == CMD_SELECT)
    {
                                                      
      if (list_length(rule->actions) != 1)
      {
        elog(ERROR, "invalid _RETURN rule action specification");
      }

      return (Query *)linitial(rule->actions);
    }
  }

  elog(ERROR, "failed to find _RETURN rule for view");
  return NULL;                          
}

   
                                                                              
   
                                                                             
                                                                         
              
   
static bool
view_has_instead_trigger(Relation view, CmdType event)
{
  TriggerDesc *trigDesc = view->trigdesc;

  switch (event)
  {
  case CMD_INSERT:
    if (trigDesc && trigDesc->trig_insert_instead_row)
    {
      return true;
    }
    break;
  case CMD_UPDATE:
    if (trigDesc && trigDesc->trig_update_instead_row)
    {
      return true;
    }
    break;
  case CMD_DELETE:
    if (trigDesc && trigDesc->trig_delete_instead_row)
    {
      return true;
    }
    break;
  default:
    elog(ERROR, "unrecognized CmdType: %d", (int)event);
    break;
  }
  return false;
}

   
                                                                            
                                                                               
                                               
   
                                                                           
                                                         
   
                                                                               
                                                                               
   
static const char *
view_col_is_auto_updatable(RangeTblRef *rtr, TargetEntry *tle)
{
  Var *var = (Var *)tle->expr;

     
                                                                            
                                                                
     
                                                                           
                                                                             
                                                                          
                         
     
  if (tle->resjunk)
  {
    return gettext_noop("Junk view columns are not updatable.");
  }

  if (!IsA(var, Var) || var->varno != rtr->rtindex || var->varlevelsup != 0)
  {
    return gettext_noop("View columns that are not columns of their base relation are not updatable.");
  }

  if (var->varattno < 0)
  {
    return gettext_noop("View columns that refer to system columns are not updatable.");
  }

  if (var->varattno == 0)
  {
    return gettext_noop("View columns that return whole-row references are not updatable.");
  }

  return NULL;                                   
}

   
                                                                             
                                                                                
                                                            
 
                                                                           
                                                         
   
                                                                              
                                                                              
                                                                    
   
                                                                              
                                                                          
              
   
const char *
view_query_is_auto_updatable(Query *viewquery, bool check_cols)
{
  RangeTblRef *rtr;
  RangeTblEntry *base_rte;

               
                                                                             
                           
                                                                             
                                                
                                      
                                                       
                                                                           
     
                                                                          
                                                                          
                                                                           
                                                                          
                                                                       
     
                                                                         
                                                                         
                                                                             
                             
     
                                                                          
                         
                               
                                                                         
                                                                        
                                         
                                                
     
                                                                          
                                                                           
               
     
  if (viewquery->distinctClause != NIL)
  {
    return gettext_noop("Views containing DISTINCT are not automatically updatable.");
  }

  if (viewquery->groupClause != NIL || viewquery->groupingSets)
  {
    return gettext_noop("Views containing GROUP BY are not automatically updatable.");
  }

  if (viewquery->havingQual != NULL)
  {
    return gettext_noop("Views containing HAVING are not automatically updatable.");
  }

  if (viewquery->setOperations != NULL)
  {
    return gettext_noop("Views containing UNION, INTERSECT, or EXCEPT are not automatically updatable.");
  }

  if (viewquery->cteList != NIL)
  {
    return gettext_noop("Views containing WITH are not automatically updatable.");
  }

  if (viewquery->limitOffset != NULL || viewquery->limitCount != NULL)
  {
    return gettext_noop("Views containing LIMIT or OFFSET are not automatically updatable.");
  }

     
                                                                          
                                                                            
                                                                            
                                          
     
                                                                          
                                                 
     
  if (viewquery->hasAggs)
  {
    return gettext_noop("Views that return aggregate functions are not automatically updatable.");
  }

  if (viewquery->hasWindowFuncs)
  {
    return gettext_noop("Views that return window functions are not automatically updatable.");
  }

  if (viewquery->hasTargetSRFs)
  {
    return gettext_noop("Views that return set-returning functions are not automatically updatable.");
  }

     
                                                                             
                              
     
  if (list_length(viewquery->jointree->fromlist) != 1)
  {
    return gettext_noop("Views that do not select from a single table or view are not automatically updatable.");
  }

  rtr = (RangeTblRef *)linitial(viewquery->jointree->fromlist);
  if (!IsA(rtr, RangeTblRef))
  {
    return gettext_noop("Views that do not select from a single table or view are not automatically updatable.");
  }

  base_rte = rt_fetch(rtr->rtindex, viewquery->rtable);
  if (base_rte->rtekind != RTE_RELATION || (base_rte->relkind != RELKIND_RELATION && base_rte->relkind != RELKIND_FOREIGN_TABLE && base_rte->relkind != RELKIND_VIEW && base_rte->relkind != RELKIND_PARTITIONED_TABLE))
  {
    return gettext_noop("Views that do not select from a single table or view are not automatically updatable.");
  }

  if (base_rte->tablesample)
  {
    return gettext_noop("Views containing TABLESAMPLE are not automatically updatable.");
  }

     
                                                                             
                                           
     
  if (check_cols)
  {
    ListCell *cell;
    bool found;

    found = false;
    foreach (cell, viewquery->targetList)
    {
      TargetEntry *tle = (TargetEntry *)lfirst(cell);

      if (view_col_is_auto_updatable(rtr, tle) == NULL)
      {
        found = true;
        break;
      }
    }

    if (!found)
    {
      return gettext_noop("Views that have no updatable columns are not automatically updatable.");
    }
  }

  return NULL;                            
}

   
                                                                              
                                                                           
                                                                               
                   
   
                                                                           
                                                         
   
                                                                            
                                        
   
                                                                               
                                                                              
                                                
   
                                                                             
                                       
   
                                                                              
                                                                           
              
   
static const char *
view_cols_are_auto_updatable(Query *viewquery, Bitmapset *required_cols, Bitmapset **updatable_cols, char **non_updatable_col)
{
  RangeTblRef *rtr;
  AttrNumber col;
  ListCell *cell;

     
                                                                             
                                             
     
  Assert(list_length(viewquery->jointree->fromlist) == 1);
  rtr = linitial_node(RangeTblRef, viewquery->jointree->fromlist);

                                             
  if (updatable_cols != NULL)
  {
    *updatable_cols = NULL;
  }
  if (non_updatable_col != NULL)
  {
    *non_updatable_col = NULL;
  }

                                              
  col = -FirstLowInvalidHeapAttributeNumber;
  foreach (cell, viewquery->targetList)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(cell);
    const char *col_update_detail;

    col++;
    col_update_detail = view_col_is_auto_updatable(rtr, tle);

    if (col_update_detail == NULL)
    {
                                   
      if (updatable_cols != NULL)
      {
        *updatable_cols = bms_add_member(*updatable_cols, col);
      }
    }
    else if (bms_is_member(col, required_cols))
    {
                                                
      if (non_updatable_col != NULL)
      {
        *non_updatable_col = tle->resname;
      }
      return col_update_detail;
    }
  }

  return NULL;                                                  
}

   
                                                                       
                      
   
                                                                             
                                                                           
                                                                           
                                                                             
                 
   
                                                                         
                                                                         
                                                                          
                  
   
                                                                               
                                                                            
                                                                         
                                                                               
                                                                              
                                                                          
                                                                         
                                                                  
                                                                             
                                                                          
                                                        
   
                                                                           
                                                                               
                                                                         
   
int
relation_is_updatable(Oid reloid, List *outer_reloids, bool include_triggers, Bitmapset *include_cols)
{
  int events = 0;
  Relation rel;
  RuleLock *rulelocks;

#define ALL_EVENTS ((1 << CMD_INSERT) | (1 << CMD_UPDATE) | (1 << CMD_DELETE))

                                                                          
  check_stack_depth();

  rel = try_relation_open(reloid, AccessShareLock);

     
                                                                        
                                                                             
                                                                       
                      
     
  if (rel == NULL)
  {
    return 0;
  }

                                                                      
  if (list_member_oid(outer_reloids, RelationGetRelid(rel)))
  {
    relation_close(rel, AccessShareLock);
    return 0;
  }

                                                          
  if (rel->rd_rel->relkind == RELKIND_RELATION || rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    relation_close(rel, AccessShareLock);
    return ALL_EVENTS;
  }

                                                                          
  rulelocks = rel->rd_rules;
  if (rulelocks != NULL)
  {
    int i;

    for (i = 0; i < rulelocks->numLocks; i++)
    {
      if (rulelocks->rules[i]->isInstead && rulelocks->rules[i]->qual == NULL)
      {
        events |= ((1 << rulelocks->rules[i]->event) & ALL_EVENTS);
      }
    }

                                                     
    if (events == ALL_EVENTS)
    {
      relation_close(rel, AccessShareLock);
      return events;
    }
  }

                                                                          
  if (include_triggers)
  {
    TriggerDesc *trigDesc = rel->trigdesc;

    if (trigDesc)
    {
      if (trigDesc->trig_insert_instead_row)
      {
        events |= (1 << CMD_INSERT);
      }
      if (trigDesc->trig_update_instead_row)
      {
        events |= (1 << CMD_UPDATE);
      }
      if (trigDesc->trig_delete_instead_row)
      {
        events |= (1 << CMD_DELETE);
      }

                                                          
      if (events == ALL_EVENTS)
      {
        relation_close(rel, AccessShareLock);
        return events;
      }
    }
  }

                                                                         
  if (rel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
  {
    FdwRoutine *fdwroutine = GetFdwRoutineForRelation(rel, false);

    if (fdwroutine->IsForeignRelUpdatable != NULL)
    {
      events |= fdwroutine->IsForeignRelUpdatable(rel);
    }
    else
    {
                                                               
      if (fdwroutine->ExecForeignInsert != NULL)
      {
        events |= (1 << CMD_INSERT);
      }
      if (fdwroutine->ExecForeignUpdate != NULL)
      {
        events |= (1 << CMD_UPDATE);
      }
      if (fdwroutine->ExecForeignDelete != NULL)
      {
        events |= (1 << CMD_DELETE);
      }
    }

    relation_close(rel, AccessShareLock);
    return events;
  }

                                                        
  if (rel->rd_rel->relkind == RELKIND_VIEW)
  {
    Query *viewquery = get_view_query(rel);

    if (view_query_is_auto_updatable(viewquery, false) == NULL)
    {
      Bitmapset *updatable_cols;
      int auto_events;
      RangeTblRef *rtr;
      RangeTblEntry *base_rte;
      Oid baseoid;

         
                                                                       
                                                                        
                                                                      
                 
         
      view_cols_are_auto_updatable(viewquery, NULL, &updatable_cols, NULL);

      if (include_cols != NULL)
      {
        updatable_cols = bms_int_members(updatable_cols, include_cols);
      }

      if (bms_is_empty(updatable_cols))
      {
        auto_events = (1 << CMD_DELETE);                         
      }
      else
      {
        auto_events = ALL_EVENTS;                             
      }

         
                                                                    
                                                                     
                                                                      
                                                                   
         
      rtr = (RangeTblRef *)linitial(viewquery->jointree->fromlist);
      base_rte = rt_fetch(rtr->rtindex, viewquery->rtable);
      Assert(base_rte->rtekind == RTE_RELATION);

      if (base_rte->relkind != RELKIND_RELATION && base_rte->relkind != RELKIND_PARTITIONED_TABLE)
      {
        baseoid = base_rte->relid;
        outer_reloids = lcons_oid(RelationGetRelid(rel), outer_reloids);
        include_cols = adjust_view_column_set(updatable_cols, viewquery->targetList);
        auto_events &= relation_is_updatable(baseoid, outer_reloids, include_triggers, include_cols);
        outer_reloids = list_delete_first(outer_reloids);
      }
      events |= auto_events;
    }
  }

                                                                       
  relation_close(rel, AccessShareLock);
  return events;
}

   
                                                                                
   
                                                                               
                                                                               
                                                                           
                                                                       
   
static Bitmapset *
adjust_view_column_set(Bitmapset *cols, List *targetlist)
{
  Bitmapset *result = NULL;
  int col;

  col = -1;
  while ((col = bms_next_member(cols, col)) >= 0)
  {
                                                                      
    AttrNumber attno = col + FirstLowInvalidHeapAttributeNumber;

    if (attno == InvalidAttrNumber)
    {
         
                                                                     
                                                                         
                                                                 
                                                                      
                                            
         
      ListCell *lc;

      foreach (lc, targetlist)
      {
        TargetEntry *tle = lfirst_node(TargetEntry, lc);
        Var *var;

        if (tle->resjunk)
        {
          continue;
        }
        var = castNode(Var, tle->expr);
        result = bms_add_member(result, var->varattno - FirstLowInvalidHeapAttributeNumber);
      }
    }
    else
    {
         
                                                                      
                                                                     
                          
         
      TargetEntry *tle = get_tle_by_resno(targetlist, attno);

      if (tle != NULL && !tle->resjunk && IsA(tle->expr, Var))
      {
        Var *var = (Var *)tle->expr;

        result = bms_add_member(result, var->varattno - FirstLowInvalidHeapAttributeNumber);
      }
      else
      {
        elog(ERROR, "attribute number %d not found in view targetlist", attno);
      }
    }
  }

  return result;
}

   
                       
                                                                             
                                                           
   
                                                                               
                                                                               
                                  
   
static Query *
rewriteTargetView(Query *parsetree, Relation view)
{
  Query *viewquery;
  const char *auto_update_detail;
  RangeTblRef *rtr;
  int base_rt_index;
  int new_rt_index;
  RangeTblEntry *base_rte;
  RangeTblEntry *view_rte;
  RangeTblEntry *new_rte;
  Relation base_rel;
  List *view_targetlist;
  ListCell *lc;

     
                                                                             
                                                                        
                                                                          
                                                                             
                                     
     
  viewquery = copyObject(get_view_query(view));

                                             
  auto_update_detail = view_query_is_auto_updatable(viewquery, parsetree->commandType != CMD_DELETE);

  if (auto_update_detail)
  {
                                                                     
    switch (parsetree->commandType)
    {
    case CMD_INSERT:
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot insert into view \"%s\"", RelationGetRelationName(view)), errdetail_internal("%s", _(auto_update_detail)), errhint("To enable inserting into the view, provide an INSTEAD OF INSERT trigger or an unconditional ON INSERT DO INSTEAD rule.")));
      break;
    case CMD_UPDATE:
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot update view \"%s\"", RelationGetRelationName(view)), errdetail_internal("%s", _(auto_update_detail)), errhint("To enable updating the view, provide an INSTEAD OF UPDATE trigger or an unconditional ON UPDATE DO INSTEAD rule.")));
      break;
    case CMD_DELETE:
      ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot delete from view \"%s\"", RelationGetRelationName(view)), errdetail_internal("%s", _(auto_update_detail)), errhint("To enable deleting from the view, provide an INSTEAD OF DELETE trigger or an unconditional ON DELETE DO INSTEAD rule.")));
      break;
    default:
      elog(ERROR, "unrecognized CmdType: %d", (int)parsetree->commandType);
      break;
    }
  }

     
                                                                             
                                                                           
                                                             
                                                                          
                                                      
     
  if (parsetree->commandType != CMD_DELETE)
  {
    Bitmapset *modified_cols = NULL;
    char *non_updatable_col;

    foreach (lc, parsetree->targetList)
    {
      TargetEntry *tle = (TargetEntry *)lfirst(lc);

      if (!tle->resjunk)
      {
        modified_cols = bms_add_member(modified_cols, tle->resno - FirstLowInvalidHeapAttributeNumber);
      }
    }

    if (parsetree->onConflict)
    {
      foreach (lc, parsetree->onConflict->onConflictSet)
      {
        TargetEntry *tle = (TargetEntry *)lfirst(lc);

        if (!tle->resjunk)
        {
          modified_cols = bms_add_member(modified_cols, tle->resno - FirstLowInvalidHeapAttributeNumber);
        }
      }
    }

    auto_update_detail = view_cols_are_auto_updatable(viewquery, modified_cols, NULL, &non_updatable_col);
    if (auto_update_detail)
    {
         
                                                                     
                                                              
         
      switch (parsetree->commandType)
      {
      case CMD_INSERT:
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot insert into column \"%s\" of view \"%s\"", non_updatable_col, RelationGetRelationName(view)), errdetail_internal("%s", _(auto_update_detail))));
        break;
      case CMD_UPDATE:
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot update column \"%s\" of view \"%s\"", non_updatable_col, RelationGetRelationName(view)), errdetail_internal("%s", _(auto_update_detail))));
        break;
      default:
        elog(ERROR, "unrecognized CmdType: %d", (int)parsetree->commandType);
        break;
      }
    }
  }

                                                         
  view_rte = rt_fetch(parsetree->resultRelation, parsetree->rtable);

     
                                                                          
                                           
     
  Assert(list_length(viewquery->jointree->fromlist) == 1);
  rtr = linitial_node(RangeTblRef, viewquery->jointree->fromlist);

  base_rt_index = rtr->rtindex;
  base_rte = rt_fetch(base_rt_index, viewquery->rtable);
  Assert(base_rte->rtekind == RTE_RELATION);

     
                                                                           
                                                                         
                                                                         
                                                                          
                                                           
     
  base_rel = table_open(base_rte->relid, RowExclusiveLock);

     
                                                                             
                                                                    
     
  base_rte->relkind = base_rel->rd_rel->relkind;

     
                                                                            
                                                                             
                                                                            
                             
     
  if (viewquery->hasSubLinks)
  {
    acquireLocksOnSubLinks_context context;

    context.for_execute = true;
    query_tree_walker(viewquery, acquireLocksOnSubLinks, &context, QTW_IGNORE_RC_SUBQUERIES);
  }

     
                                                                             
                                                                           
                                                                             
                                                                            
                                          
     
                                                                           
                                                                        
                                     
     
  new_rte = base_rte;
  new_rte->rellockmode = RowExclusiveLock;

  parsetree->rtable = lappend(parsetree->rtable, new_rte);
  new_rt_index = list_length(parsetree->rtable);

     
                                                                        
                                             
     
  if (parsetree->commandType == CMD_INSERT)
  {
    new_rte->inh = false;
  }

     
                                                                           
                                                                            
                                                                           
                                                                          
                                                                           
            
     
  view_targetlist = viewquery->targetList;

  ChangeVarNodes((Node *)view_targetlist, base_rt_index, new_rt_index, 0);

     
                                                                        
                                                                            
                                                                           
                                                                           
                                                                 
     
                                                                         
                                                                            
                                                                        
                                     
     
  new_rte->checkAsUser = view->rd_rel->relowner;
  new_rte->requiredPerms = view_rte->requiredPerms;

     
                                              
     
                                                                            
                                                                             
                                                                           
                                                                         
                                                                   
                                                                             
                                                                            
                                                                             
                                                                         
                                                                             
                                                                            
                                                                           
                                                                         
                                                                       
                                                                          
                                                                
     
                                                                           
                    
     
  Assert(bms_is_empty(new_rte->insertedCols) && bms_is_empty(new_rte->updatedCols));

  new_rte->insertedCols = adjust_view_column_set(view_rte->insertedCols, view_targetlist);

  new_rte->updatedCols = adjust_view_column_set(view_rte->updatedCols, view_targetlist);

     
                                                                           
                                                                          
                                                                 
     
  new_rte->securityQuals = view_rte->securityQuals;
  view_rte->securityQuals = NIL;

     
                                                                       
                                                                    
     
  parsetree = (Query *)ReplaceVarsFromTargetList((Node *)parsetree, parsetree->resultRelation, 0, view_rte, view_targetlist, REPLACEVARS_REPORT_ERROR, 0, &parsetree->hasSubLinks);

     
                                                                         
                                                                         
                                                                          
                                                     
     
  ChangeVarNodes((Node *)parsetree, parsetree->resultRelation, new_rt_index, 0);
  Assert(parsetree->resultRelation == new_rt_index);

     
                                                                             
                                                                      
                             
     
                                                                            
                                                                           
                                                          
     
  if (parsetree->commandType != CMD_DELETE)
  {
    foreach (lc, parsetree->targetList)
    {
      TargetEntry *tle = (TargetEntry *)lfirst(lc);
      TargetEntry *view_tle;

      if (tle->resjunk)
      {
        continue;
      }

      view_tle = get_tle_by_resno(view_targetlist, tle->resno);
      if (view_tle != NULL && !view_tle->resjunk && IsA(view_tle->expr, Var))
      {
        tle->resno = ((Var *)view_tle->expr)->varattno;
      }
      else
      {
        elog(ERROR, "attribute number %d not found in view targetlist", tle->resno);
      }
    }
  }

     
                                                                          
                                             
     
  if (parsetree->onConflict && parsetree->onConflict->action == ONCONFLICT_UPDATE)
  {
    Index old_exclRelIndex, new_exclRelIndex;
    RangeTblEntry *new_exclRte;
    List *tmp_tlist;

       
                                                                   
                                                                   
                 
       
    foreach (lc, parsetree->onConflict->onConflictSet)
    {
      TargetEntry *tle = (TargetEntry *)lfirst(lc);
      TargetEntry *view_tle;

      if (tle->resjunk)
      {
        continue;
      }

      view_tle = get_tle_by_resno(view_targetlist, tle->resno);
      if (view_tle != NULL && !view_tle->resjunk && IsA(view_tle->expr, Var))
      {
        tle->resno = ((Var *)view_tle->expr)->varattno;
      }
      else
      {
        elog(ERROR, "attribute number %d not found in view targetlist", tle->resno);
      }
    }

       
                                                                          
                                                                         
                                                                           
                                                                      
                                                                         
                                                                 
       
    old_exclRelIndex = parsetree->onConflict->exclRelIndex;

    new_exclRte = addRangeTableEntryForRelation(make_parsestate(NULL), base_rel, RowExclusiveLock, makeAlias("excluded", NIL), false, false);
    new_exclRte->relkind = RELKIND_COMPOSITE_TYPE;
    new_exclRte->requiredPerms = 0;
                                                                   

    parsetree->rtable = lappend(parsetree->rtable, new_exclRte);
    new_exclRelIndex = parsetree->onConflict->exclRelIndex = list_length(parsetree->rtable);

       
                                                                          
                                                                 
       
    parsetree->onConflict->exclRelTlist = BuildOnConflictExcludedTargetlist(base_rel, new_exclRelIndex);

       
                                                                       
                                                                     
                                                                           
                                                                        
                                                                       
                                     
       
    tmp_tlist = copyObject(view_targetlist);

    ChangeVarNodes((Node *)tmp_tlist, new_rt_index, new_exclRelIndex, 0);

    parsetree->onConflict = (OnConflictExpr *)ReplaceVarsFromTargetList((Node *)parsetree->onConflict, old_exclRelIndex, 0, view_rte, tmp_tlist, REPLACEVARS_REPORT_ERROR, 0, &parsetree->hasSubLinks);
  }

     
                                                                             
                                                                            
                                                                            
                                       
     
                                                                             
                                                                          
                                                                     
     
                                                                    
     
  if (parsetree->commandType != CMD_INSERT && viewquery->jointree->quals != NULL)
  {
    Node *viewqual = (Node *)viewquery->jointree->quals;

       
                                                                  
                                                                           
                                                                       
       
    viewqual = copyObject(viewqual);

    ChangeVarNodes(viewqual, base_rt_index, new_rt_index, 0);

    if (RelationIsSecurityView(view))
    {
         
                                                                       
                                                                       
                                          
         
                                                                         
                                            
         
      new_rte = rt_fetch(new_rt_index, parsetree->rtable);
      new_rte->securityQuals = lcons(viewqual, new_rte->securityQuals);

         
                                                                        
                                                                      
         

         
                                                                        
                       
         
      if (!parsetree->hasSubLinks)
      {
        parsetree->hasSubLinks = checkExprHasSubLink(viewqual);
      }
    }
    else
    {
      AddQual(parsetree, (Node *)viewqual);
    }
  }

     
                                                                             
                                                                            
                                           
     
  if (parsetree->commandType != CMD_DELETE)
  {
    bool has_wco = RelationHasCheckOption(view);
    bool cascaded = RelationHasCascadedCheckOption(view);

       
                                                                          
                                               
       
                                                                      
                                                                          
             
       
    if (parsetree->withCheckOptions != NIL)
    {
      WithCheckOption *parent_wco = (WithCheckOption *)linitial(parsetree->withCheckOptions);

      if (parent_wco->cascaded)
      {
        has_wco = true;
        cascaded = true;
      }
    }

       
                                                                     
                                                                      
                                     
       
                                                                         
                                                                       
                                                       
       
    if (has_wco && (cascaded || viewquery->jointree->quals != NULL))
    {
      WithCheckOption *wco;

      wco = makeNode(WithCheckOption);
      wco->kind = WCO_VIEW_CHECK;
      wco->relname = pstrdup(RelationGetRelationName(view));
      wco->polname = NULL;
      wco->qual = NULL;
      wco->cascaded = cascaded;

      parsetree->withCheckOptions = lcons(wco, parsetree->withCheckOptions);

      if (viewquery->jointree->quals != NULL)
      {
        wco->qual = (Node *)viewquery->jointree->quals;
        ChangeVarNodes(wco->qual, base_rt_index, new_rt_index, 0);

           
                                                                     
                                                                      
                                                                    
                                                                     
                                              
           
        if (!parsetree->hasSubLinks && parsetree->commandType != CMD_UPDATE)
        {
          parsetree->hasSubLinks = checkExprHasSubLink(wco->qual);
        }
      }
    }
  }

  table_close(base_rel, NoLock);

  return parsetree;
}

   
                  
                                                                           
   
                                                                            
                       
   
                                                                               
                                                                              
                                                          
   
static List *
RewriteQuery(Query *parsetree, List *rewrite_events, int orig_rt_length)
{
  CmdType event = parsetree->commandType;
  bool instead = false;
  bool returning = false;
  bool updatableview = false;
  Query *qual_product = NULL;
  List *rewritten = NIL;
  ListCell *lc1;

     
                                                                            
                                                                          
                                      
     
  foreach (lc1, parsetree->cteList)
  {
    CommonTableExpr *cte = lfirst_node(CommonTableExpr, lc1);
    Query *ctequery = castNode(Query, cte->ctequery);
    List *newstuff;

    if (ctequery->commandType == CMD_SELECT)
    {
      continue;
    }

    newstuff = RewriteQuery(ctequery, rewrite_events, 0);

       
                                                                       
                                                                       
                                                                           
       
    if (list_length(newstuff) == 1)
    {
                                                 
      ctequery = linitial_node(Query, newstuff);
      if (!(ctequery->commandType == CMD_SELECT || ctequery->commandType == CMD_UPDATE || ctequery->commandType == CMD_INSERT || ctequery->commandType == CMD_DELETE))
      {
           
                                                                      
                                                                       
           
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("DO INSTEAD NOTIFY rules are not supported for data-modifying statements in WITH")));
      }
                                                  
      Assert(!ctequery->canSetTag);
                                                        
      cte->ctequery = (Node *)ctequery;
    }
    else if (newstuff == NIL)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("DO INSTEAD NOTHING rules are not supported for data-modifying statements in WITH")));
    }
    else
    {
      ListCell *lc2;

                                                                     
      foreach (lc2, newstuff)
      {
        Query *q = (Query *)lfirst(lc2);

        if (q->querySource == QSRC_QUAL_INSTEAD_RULE)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("conditional DO INSTEAD rules are not supported for data-modifying statements in WITH")));
        }
        if (q->querySource == QSRC_NON_INSTEAD_RULE)
        {
          ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("DO ALSO rules are not supported for data-modifying statements in WITH")));
        }
      }

      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("multi-statement DO INSTEAD rules are not supported for data-modifying statements in WITH")));
    }
  }

     
                                                                             
                                                                
     
                                                                             
                                                                         
                       
     
  if (event != CMD_SELECT && event != CMD_UTILITY)
  {
    int result_relation;
    RangeTblEntry *rt_entry;
    Relation rt_entry_relation;
    List *locks;
    int product_orig_rt_length;
    List *product_queries;
    bool hasUpdate = false;
    int values_rte_index = 0;
    bool defaults_remaining = false;

    result_relation = parsetree->resultRelation;
    Assert(result_relation != 0);
    rt_entry = rt_fetch(result_relation, parsetree->rtable);
    Assert(rt_entry->rtekind == RTE_RELATION);

       
                                                         
                                                               
       
    rt_entry_relation = table_open(rt_entry->relid, NoLock);

       
                                                              
       
    if (event == CMD_INSERT)
    {
      ListCell *lc2;
      RangeTblEntry *values_rte = NULL;

         
                                                                         
                                                                         
                                                                   
                                                                       
         
      foreach (lc2, parsetree->jointree->fromlist)
      {
        RangeTblRef *rtr = (RangeTblRef *)lfirst(lc2);

        if (IsA(rtr, RangeTblRef) && rtr->rtindex > orig_rt_length)
        {
          RangeTblEntry *rte = rt_fetch(rtr->rtindex, parsetree->rtable);

          if (rte->rtekind == RTE_VALUES)
          {
                                                          
            if (values_rte != NULL)
            {
              elog(ERROR, "more than one VALUES RTE found");
            }

            values_rte = rte;
            values_rte_index = rtr->rtindex;
          }
        }
      }

      if (values_rte)
      {
                                             
        parsetree->targetList = rewriteTargetListIU(parsetree->targetList, parsetree->commandType, parsetree->override, rt_entry_relation, parsetree->resultRelation);
                                                 
        if (!rewriteValuesRTE(parsetree, values_rte, values_rte_index, rt_entry_relation))
        {
          defaults_remaining = true;
        }
      }
      else
      {
                                              
        parsetree->targetList = rewriteTargetListIU(parsetree->targetList, parsetree->commandType, parsetree->override, rt_entry_relation, parsetree->resultRelation);
      }

      if (parsetree->onConflict && parsetree->onConflict->action == ONCONFLICT_UPDATE)
      {
        parsetree->onConflict->onConflictSet = rewriteTargetListIU(parsetree->onConflict->onConflictSet, CMD_UPDATE, parsetree->override, rt_entry_relation, parsetree->resultRelation);
      }
    }
    else if (event == CMD_UPDATE)
    {
      parsetree->targetList = rewriteTargetListIU(parsetree->targetList, parsetree->commandType, parsetree->override, rt_entry_relation, parsetree->resultRelation);

                                                                  
      fill_extraUpdatedCols(rt_entry, rt_entry_relation);
    }
    else if (event == CMD_DELETE)
    {
                              
    }
    else
    {
      elog(ERROR, "unrecognized commandType: %d", (int)event);
    }

       
                                                
       
    locks = matchLocks(event, rt_entry_relation->rd_rules, result_relation, parsetree, &hasUpdate);

    product_orig_rt_length = list_length(parsetree->rtable);
    product_queries = fireRules(parsetree, result_relation, event, locks, &instead, &returning, &qual_product);

       
                                                                           
                                                                        
                                                                         
                                                                        
                                                                    
                                                            
       
    if (defaults_remaining && product_queries != NIL)
    {
      ListCell *n;

         
                                                                      
                                                                     
         
      foreach (n, product_queries)
      {
        Query *pt = (Query *)lfirst(n);
        RangeTblEntry *values_rte = rt_fetch(values_rte_index, pt->rtable);

        rewriteValuesRTEToNulls(pt, values_rte);
      }
    }

       
                                                                         
                                                                         
                                                                     
                                                              
                                                                      
                  
       
                                                                           
                                                                           
                                                                        
                                                                    
                  
       
    if (!instead && rt_entry_relation->rd_rel->relkind == RELKIND_VIEW && !view_has_instead_trigger(rt_entry_relation, event))
    {
         
                                                                         
                                                                     
                                          
         
                                                                         
                                                                         
                                    
         
      if (qual_product != NULL)
      {
        switch (parsetree->commandType)
        {
        case CMD_INSERT:
          ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot insert into view \"%s\"", RelationGetRelationName(rt_entry_relation)), errdetail("Views with conditional DO INSTEAD rules are not automatically updatable."), errhint("To enable inserting into the view, provide an INSTEAD OF INSERT trigger or an unconditional ON INSERT DO INSTEAD rule.")));
          break;
        case CMD_UPDATE:
          ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot update view \"%s\"", RelationGetRelationName(rt_entry_relation)), errdetail("Views with conditional DO INSTEAD rules are not automatically updatable."), errhint("To enable updating the view, provide an INSTEAD OF UPDATE trigger or an unconditional ON UPDATE DO INSTEAD rule.")));
          break;
        case CMD_DELETE:
          ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("cannot delete from view \"%s\"", RelationGetRelationName(rt_entry_relation)), errdetail("Views with conditional DO INSTEAD rules are not automatically updatable."), errhint("To enable deleting from the view, provide an INSTEAD OF DELETE trigger or an unconditional ON DELETE DO INSTEAD rule.")));
          break;
        default:
          elog(ERROR, "unrecognized CmdType: %d", (int)parsetree->commandType);
          break;
        }
      }

         
                                                                        
                                                                 
                  
         
      parsetree = rewriteTargetView(parsetree, rt_entry_relation);

         
                                                                 
                                                                       
                                                                      
                                                       
         
      if (parsetree->commandType == CMD_INSERT)
      {
        product_queries = lcons(parsetree, product_queries);
      }
      else
      {
        product_queries = lappend(product_queries, parsetree);
      }

         
                                                                     
                                                                      
                                                                        
                                                                     
                                  
         
      instead = true;
      returning = true;
      updatableview = true;
    }

       
                                                                       
                                  
       
    if (product_queries != NIL)
    {
      ListCell *n;
      rewrite_event *rev;

      foreach (n, rewrite_events)
      {
        rev = (rewrite_event *)lfirst(n);
        if (rev->relation == RelationGetRelid(rt_entry_relation) && rev->event == event)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("infinite recursion detected in rules for relation \"%s\"", RelationGetRelationName(rt_entry_relation))));
        }
      }

      rev = (rewrite_event *)palloc(sizeof(rewrite_event));
      rev->relation = RelationGetRelid(rt_entry_relation);
      rev->event = event;
      rewrite_events = lcons(rev, rewrite_events);

      foreach (n, product_queries)
      {
        Query *pt = (Query *)lfirst(n);
        List *newstuff;

           
                                                                       
                                                                       
                                                             
           
                                                                      
                                                                   
                                                           
           
        newstuff = RewriteQuery(pt, rewrite_events, pt == parsetree ? orig_rt_length : product_orig_rt_length);
        rewritten = list_concat(rewritten, newstuff);
      }

      rewrite_events = list_delete_first(rewrite_events);
    }

       
                                                                          
                                                                          
                                                                         
                                                                         
                                                  
       
    if ((instead || qual_product != NULL) && parsetree->returningList && !returning)
    {
      switch (event)
      {
      case CMD_INSERT:
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot perform INSERT RETURNING on relation \"%s\"", RelationGetRelationName(rt_entry_relation)), errhint("You need an unconditional ON INSERT DO INSTEAD rule with a RETURNING clause.")));
        break;
      case CMD_UPDATE:
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot perform UPDATE RETURNING on relation \"%s\"", RelationGetRelationName(rt_entry_relation)), errhint("You need an unconditional ON UPDATE DO INSTEAD rule with a RETURNING clause.")));
        break;
      case CMD_DELETE:
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot perform DELETE RETURNING on relation \"%s\"", RelationGetRelationName(rt_entry_relation)), errhint("You need an unconditional ON DELETE DO INSTEAD rule with a RETURNING clause.")));
        break;
      default:
        elog(ERROR, "unrecognized commandType: %d", (int)event);
        break;
      }
    }

       
                                                                           
                            
       
    if (parsetree->onConflict && (product_queries != NIL || hasUpdate) && !updatableview)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("INSERT with ON CONFLICT clause cannot be used with table that has INSERT or UPDATE rules")));
    }

    table_close(rt_entry_relation, NoLock);
  }

     
                                                                             
                                                                             
                                                                       
                                                                           
                                                                             
                                                    
     
                                                                            
                                                                         
                                                    
     
  if (!instead)
  {
    if (parsetree->commandType == CMD_INSERT)
    {
      if (qual_product != NULL)
      {
        rewritten = lcons(qual_product, rewritten);
      }
      else
      {
        rewritten = lcons(parsetree, rewritten);
      }
    }
    else
    {
      if (qual_product != NULL)
      {
        rewritten = lappend(rewritten, qual_product);
      }
      else
      {
        rewritten = lappend(rewritten, parsetree);
      }
    }
  }

     
                                                                          
                                                                             
                                                                           
                                                                 
                                                                          
                                   
     
  if (parsetree->cteList != NIL)
  {
    int qcount = 0;

    foreach (lc1, rewritten)
    {
      Query *q = (Query *)lfirst(lc1);

      if (q->commandType != CMD_UTILITY)
      {
        qcount++;
      }
    }
    if (qcount > 1)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("WITH cannot be used in a query that is rewritten by rules into multiple queries")));
    }
  }

  return rewritten;
}

   
                  
                                                
                                                                      
                      
   
                                                                       
                                                                          
   
List *
QueryRewrite(Query *parsetree)
{
  uint64 input_query_id = parsetree->queryId;
  List *querylist;
  List *results;
  ListCell *l;
  CmdType origCmdType;
  bool foundOriginalQuery;
  Query *lastInstead;

     
                                                                 
     
  Assert(parsetree->querySource == QSRC_ORIGINAL);
  Assert(parsetree->canSetTag);

     
            
     
                                                                   
     
  querylist = RewriteQuery(parsetree, NIL, 0);

     
            
     
                                           
     
                                                                             
     
  results = NIL;
  foreach (l, querylist)
  {
    Query *query = (Query *)lfirst(l);

    query = fireRIRrules(query, NIL);

    query->queryId = input_query_id;

    results = lappend(results, query);
  }

     
            
     
                                                                          
                                                                          
     
                                                                          
                                                                           
                                                                           
                                                                           
                                                            
     
                                                                            
                                                                            
                                            
     
  origCmdType = parsetree->commandType;
  foundOriginalQuery = false;
  lastInstead = NULL;

  foreach (l, results)
  {
    Query *query = (Query *)lfirst(l);

    if (query->querySource == QSRC_ORIGINAL)
    {
      Assert(query->canSetTag);
      Assert(!foundOriginalQuery);
      foundOriginalQuery = true;
#ifndef USE_ASSERT_CHECKING
      break;
#endif
    }
    else
    {
      Assert(!query->canSetTag);
      if (query->commandType == origCmdType && (query->querySource == QSRC_INSTEAD_RULE || query->querySource == QSRC_QUAL_INSTEAD_RULE))
      {
        lastInstead = query;
      }
    }
  }

  if (!foundOriginalQuery && lastInstead != NULL)
  {
    lastInstead->canSetTag = true;
  }

  return results;
}
