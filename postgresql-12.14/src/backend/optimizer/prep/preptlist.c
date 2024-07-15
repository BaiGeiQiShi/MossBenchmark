                                                                            
   
               
                                                       
   
                                                                           
                                                                               
                                                                               
                                                                          
                                                                               
                                                                           
                          
   
                                                                          
                                                                          
                                                                          
                                                                            
                                                                             
                                                                              
                                                                            
                                                                        
                                           
   
                                                                               
                                                                            
                                                                            
                                                                               
                                                                               
                                                            
   
   
                                                                         
                                                                        
   
                  
                                            
   
                                                                            
   

#include "postgres.h"

#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "optimizer/optimizer.h"
#include "optimizer/prep.h"
#include "optimizer/tlist.h"
#include "parser/parsetree.h"
#include "parser/parse_coerce.h"
#include "rewrite/rewriteHandler.h"
#include "utils/rel.h"

static List *
expand_targetlist(List *tlist, int command_type, Index result_relation, Relation rel);

   
                         
                                                         
   
                                 
   
                                                                             
                                                
   
List *
preprocess_targetlist(PlannerInfo *root)
{
  Query *parse = root->parse;
  int result_relation = parse->resultRelation;
  List *range_table = parse->rtable;
  CmdType command_type = parse->commandType;
  RangeTblEntry *target_rte = NULL;
  Relation target_relation = NULL;
  List *tlist;
  ListCell *lc;

     
                                                                       
                                                                          
                                                                     
     
  if (result_relation)
  {
    target_rte = rt_fetch(result_relation, range_table);

       
                                                                          
                                          
       
    if (target_rte->rtekind != RTE_RELATION)
    {
      elog(ERROR, "result relation must be a regular relation");
    }

    target_relation = table_open(target_rte->relid, NoLock);
  }
  else
  {
    Assert(command_type == CMD_SELECT);
  }

     
                                                                            
                                                                         
                                                                         
                                                           
     
  if (command_type == CMD_UPDATE || command_type == CMD_DELETE)
  {
    rewriteTargetListUD(parse, target_rte, target_relation);
  }

     
                                                                            
                                                                            
           
     
  tlist = parse->targetList;
  if (command_type == CMD_INSERT || command_type == CMD_UPDATE)
  {
    tlist = expand_targetlist(tlist, command_type, result_relation, target_relation);
  }

     
                                                                             
                                                                           
                                                                       
                                                                           
                                                         
     
  foreach (lc, root->rowMarks)
  {
    PlanRowMark *rc = (PlanRowMark *)lfirst(lc);
    Var *var;
    char resname[32];
    TargetEntry *tle;

                                                             
    if (rc->rti != rc->prti)
    {
      continue;
    }

    if (rc->allMarkTypes & ~(1 << ROW_MARK_COPY))
    {
                             
      var = makeVar(rc->rti, SelfItemPointerAttributeNumber, TIDOID, -1, InvalidOid, 0);
      snprintf(resname, sizeof(resname), "ctid%u", rc->rowmarkId);
      tle = makeTargetEntry((Expr *)var, list_length(tlist) + 1, pstrdup(resname), true);
      tlist = lappend(tlist, tle);
    }
    if (rc->allMarkTypes & (1 << ROW_MARK_COPY))
    {
                                            
      var = makeWholeRowVar(rt_fetch(rc->rti, range_table), rc->rti, 0, false);
      snprintf(resname, sizeof(resname), "wholerow%u", rc->rowmarkId);
      tle = makeTargetEntry((Expr *)var, list_length(tlist) + 1, pstrdup(resname), true);
      tlist = lappend(tlist, tle);
    }

                                                                       
    if (rc->isParent)
    {
      var = makeVar(rc->rti, TableOidAttributeNumber, OIDOID, -1, InvalidOid, 0);
      snprintf(resname, sizeof(resname), "tableoid%u", rc->rowmarkId);
      tle = makeTargetEntry((Expr *)var, list_length(tlist) + 1, pstrdup(resname), true);
      tlist = lappend(tlist, tle);
    }
  }

     
                                                                         
                                                                           
                                                                            
                                                                           
                                             
     
  if (parse->returningList && list_length(parse->rtable) > 1)
  {
    List *vars;
    ListCell *l;

    vars = pull_var_clause((Node *)parse->returningList, PVC_RECURSE_AGGREGATES | PVC_RECURSE_WINDOWFUNCS | PVC_INCLUDE_PLACEHOLDERS);
    foreach (l, vars)
    {
      Var *var = (Var *)lfirst(l);
      TargetEntry *tle;

      if (IsA(var, Var) && var->varno == result_relation)
      {
        continue;                    
      }

      if (tlist_member((Expr *)var, tlist))
      {
        continue;                     
      }

      tle = makeTargetEntry((Expr *)var, list_length(tlist) + 1, NULL, true);

      tlist = lappend(tlist, tle);
    }
    list_free(vars);
  }

     
                                                                            
                                      
     
  if (parse->onConflict)
  {
    parse->onConflict->onConflictSet = expand_targetlist(parse->onConflict->onConflictSet, CMD_UPDATE, result_relation, target_relation);
  }

  if (target_relation)
  {
    table_close(target_relation, NoLock);
  }

  return tlist;
}

                                                                               
   
                         
   
                                                                               

   
                     
                                                                           
                                                                       
                                                       
   
static List *
expand_targetlist(List *tlist, int command_type, Index result_relation, Relation rel)
{
  List *new_tlist = NIL;
  ListCell *tlist_item;
  int attrno, numattrs;

  tlist_item = list_head(tlist);

     
                                                                           
                                                                   
     
                                                                         
                                                              
     
  numattrs = RelationGetNumberOfAttributes(rel);

  for (attrno = 1; attrno <= numattrs; attrno++)
  {
    Form_pg_attribute att_tup = TupleDescAttr(rel->rd_att, attrno - 1);
    TargetEntry *new_tle = NULL;

    if (tlist_item != NULL)
    {
      TargetEntry *old_tle = (TargetEntry *)lfirst(tlist_item);

      if (!old_tle->resjunk && old_tle->resno == attrno)
      {
        new_tle = old_tle;
        tlist_item = lnext(tlist_item);
      }
    }

    if (new_tle == NULL)
    {
         
                                                          
         
                                                                        
                                                                        
                                                                       
                                                     
         
                                                                       
                                                                     
                                                                      
                  
         
                                                                        
                                                                     
                                                                     
                                                                        
                                                               
                                                                     
                                                                
                            
         
      Oid atttype = att_tup->atttypid;
      int32 atttypmod = att_tup->atttypmod;
      Oid attcollation = att_tup->attcollation;
      Node *new_expr;

      switch (command_type)
      {
      case CMD_INSERT:
        if (!att_tup->attisdropped)
        {
          new_expr = (Node *)makeConst(atttype, -1, attcollation, att_tup->attlen, (Datum)0, true,             
              att_tup->attbyval);
          new_expr = coerce_to_domain(new_expr, InvalidOid, -1, atttype, COERCION_IMPLICIT, COERCE_IMPLICIT_CAST, -1, false);
        }
        else
        {
                                              
          new_expr = (Node *)makeConst(INT4OID, -1, InvalidOid, sizeof(int32), (Datum)0, true,             
              true            );
        }
        break;
      case CMD_UPDATE:
        if (!att_tup->attisdropped)
        {
          new_expr = (Node *)makeVar(result_relation, attrno, atttype, atttypmod, attcollation, 0);
        }
        else
        {
                                              
          new_expr = (Node *)makeConst(INT4OID, -1, InvalidOid, sizeof(int32), (Datum)0, true,             
              true            );
        }
        break;
      default:
        elog(ERROR, "unrecognized command_type: %d", (int)command_type);
        new_expr = NULL;                          
        break;
      }

      new_tle = makeTargetEntry((Expr *)new_expr, attrno, pstrdup(NameStr(att_tup->attname)), false);
    }

    new_tlist = lappend(new_tlist, new_tle);
  }

     
                                                                           
                                                                             
                                                                    
                                                                             
                                                                         
                     
     
  while (tlist_item)
  {
    TargetEntry *old_tle = (TargetEntry *)lfirst(tlist_item);

    if (!old_tle->resjunk)
    {
      elog(ERROR, "targetlist is not sorted correctly");
    }
                                                           
    if (old_tle->resno != attrno)
    {
      old_tle = flatCopyTargetEntry(old_tle);
      old_tle->resno = attrno;
    }
    new_tlist = lappend(new_tlist, old_tle);
    attrno++;
    tlist_item = lnext(tlist_item);
  }

  return new_tlist;
}

   
                                                                 
   
                                                                       
   
PlanRowMark *
get_plan_rowmark(List *rowmarks, Index rtindex)
{
  ListCell *l;

  foreach (l, rowmarks)
  {
    PlanRowMark *rc = (PlanRowMark *)lfirst(l);

    if (rc->rti == rtindex)
    {
      return rc;
    }
  }
  return NULL;
}
