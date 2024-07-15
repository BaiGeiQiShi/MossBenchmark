                                                                            
   
                 
                                                              
   
                                                                          
   
                                                                               
                                                                             
                                                                           
                                                                              
                                                                              
                                                                               
   
                                                                           
                                                                              
                                                                            
                                                                           
                                                        
   
                                                                           
                                                                           
                                                                              
                                                                            
                                                                           
           
   
                                                                        
                                                                      
                                                                        
                                                                         
                                                                           
                                                                            
                                                                         
                                                                            
                                                                      
                                                                          
                                                                         
                                                                       
                                                  
   
   
                                                                         
                                                                        
   
                  
                                              
   
                                                                            
   
#include "postgres.h"

#include "nodes/nodeFuncs.h"
#include "nodes/plannodes.h"
#include "optimizer/paramassign.h"
#include "optimizer/placeholder.h"
#include "rewrite/rewriteManip.h"

   
                                                                           
                                                       
                                                                            
   
static int
assign_param_for_var(PlannerInfo *root, Var *var)
{
  ListCell *ppl;
  PlannerParamItem *pitem;
  Index levelsup;

                                               
  for (levelsup = var->varlevelsup; levelsup > 0; levelsup--)
  {
    root = root->parent_root;
  }

                                                                         
  foreach (ppl, root->plan_params)
  {
    pitem = (PlannerParamItem *)lfirst(ppl);
    if (IsA(pitem->item, Var))
    {
      Var *pvar = (Var *)pitem->item;

         
                                                                     
                                                                   
         
      if (pvar->varno == var->varno && pvar->varattno == var->varattno && pvar->vartype == var->vartype && pvar->vartypmod == var->vartypmod && pvar->varcollid == var->varcollid && pvar->varnoold == var->varnoold && pvar->varoattno == var->varoattno)
      {
        return pitem->paramId;
      }
    }
  }

                               
  var = copyObject(var);
  var->varlevelsup = 0;

  pitem = makeNode(PlannerParamItem);
  pitem->item = (Node *)var;
  pitem->paramId = list_length(root->glob->paramExecTypes);
  root->glob->paramExecTypes = lappend_oid(root->glob->paramExecTypes, var->vartype);

  root->plan_params = lappend(root->plan_params, pitem);

  return pitem->paramId;
}

   
                                                   
                                                                    
                                                                            
   
Param *
replace_outer_var(PlannerInfo *root, Var *var)
{
  Param *retval;
  int i;

  Assert(var->varlevelsup > 0 && var->varlevelsup < root->query_level);

                                                                             
  i = assign_param_for_var(root, var);

  retval = makeNode(Param);
  retval->paramkind = PARAM_EXEC;
  retval->paramid = i;
  retval->paramtype = var->vartype;
  retval->paramtypmod = var->vartypmod;
  retval->paramcollid = var->varcollid;
  retval->location = var->location;

  return retval;
}

   
                                                                        
                                                                     
                                                                            
   
                                                                       
   
static int
assign_param_for_placeholdervar(PlannerInfo *root, PlaceHolderVar *phv)
{
  ListCell *ppl;
  PlannerParamItem *pitem;
  Index levelsup;

                                               
  for (levelsup = phv->phlevelsup; levelsup > 0; levelsup--)
  {
    root = root->parent_root;
  }

                                                                         
  foreach (ppl, root->plan_params)
  {
    pitem = (PlannerParamItem *)lfirst(ppl);
    if (IsA(pitem->item, PlaceHolderVar))
    {
      PlaceHolderVar *pphv = (PlaceHolderVar *)pitem->item;

                                                       
      if (pphv->phid == phv->phid)
      {
        return pitem->paramId;
      }
    }
  }

                               
  phv = copyObject(phv);
  IncrementVarSublevelsUp((Node *)phv, -((int)phv->phlevelsup), 0);
  Assert(phv->phlevelsup == 0);

  pitem = makeNode(PlannerParamItem);
  pitem->item = (Node *)phv;
  pitem->paramId = list_length(root->glob->paramExecTypes);
  root->glob->paramExecTypes = lappend_oid(root->glob->paramExecTypes, exprType((Node *)phv->phexpr));

  root->plan_params = lappend(root->plan_params, pitem);

  return pitem->paramId;
}

   
                                                              
                                                                   
                                                                            
   
                                                                    
   
Param *
replace_outer_placeholdervar(PlannerInfo *root, PlaceHolderVar *phv)
{
  Param *retval;
  int i;

  Assert(phv->phlevelsup > 0 && phv->phlevelsup < root->query_level);

                                                                             
  i = assign_param_for_placeholdervar(root, phv);

  retval = makeNode(Param);
  retval->paramkind = PARAM_EXEC;
  retval->paramid = i;
  retval->paramtype = exprType((Node *)phv->phexpr);
  retval->paramtypmod = exprTypmod((Node *)phv->phexpr);
  retval->paramcollid = exprCollation((Node *)phv->phexpr);
  retval->location = -1;

  return retval;
}

   
                                                     
                                                                    
                                                                               
   
Param *
replace_outer_agg(PlannerInfo *root, Aggref *agg)
{
  Param *retval;
  PlannerParamItem *pitem;
  Index levelsup;

  Assert(agg->agglevelsup > 0 && agg->agglevelsup < root->query_level);

                                                  
  for (levelsup = agg->agglevelsup; levelsup > 0; levelsup--)
  {
    root = root->parent_root;
  }

     
                                                                            
                                             
     
  agg = copyObject(agg);
  IncrementVarSublevelsUp((Node *)agg, -((int)agg->agglevelsup), 0);
  Assert(agg->agglevelsup == 0);

  pitem = makeNode(PlannerParamItem);
  pitem->item = (Node *)agg;
  pitem->paramId = list_length(root->glob->paramExecTypes);
  root->glob->paramExecTypes = lappend_oid(root->glob->paramExecTypes, agg->aggtype);

  root->plan_params = lappend(root->plan_params, pitem);

  retval = makeNode(Param);
  retval->paramkind = PARAM_EXEC;
  retval->paramid = pitem->paramId;
  retval->paramtype = agg->aggtype;
  retval->paramtypmod = -1;
  retval->paramcollid = agg->aggcollid;
  retval->location = agg->location;

  return retval;
}

   
                                                                               
                                                           
                                                                  
                      
   
Param *
replace_outer_grouping(PlannerInfo *root, GroupingFunc *grp)
{
  Param *retval;
  PlannerParamItem *pitem;
  Index levelsup;
  Oid ptype = exprType((Node *)grp);

  Assert(grp->agglevelsup > 0 && grp->agglevelsup < root->query_level);

                                                        
  for (levelsup = grp->agglevelsup; levelsup > 0; levelsup--)
  {
    root = root->parent_root;
  }

     
                                                                            
                                             
     
  grp = copyObject(grp);
  IncrementVarSublevelsUp((Node *)grp, -((int)grp->agglevelsup), 0);
  Assert(grp->agglevelsup == 0);

  pitem = makeNode(PlannerParamItem);
  pitem->item = (Node *)grp;
  pitem->paramId = list_length(root->glob->paramExecTypes);
  root->glob->paramExecTypes = lappend_oid(root->glob->paramExecTypes, ptype);

  root->plan_params = lappend(root->plan_params, pitem);

  retval = makeNode(Param);
  retval->paramkind = PARAM_EXEC;
  retval->paramid = pitem->paramId;
  retval->paramtype = ptype;
  retval->paramtypmod = -1;
  retval->paramcollid = InvalidOid;
  retval->location = grp->location;

  return retval;
}

   
                                                   
                                                                 
                                                        
   
Param *
replace_nestloop_param_var(PlannerInfo *root, Var *var)
{
  Param *param;
  NestLoopParam *nlp;
  ListCell *lc;

                                                           
  foreach (lc, root->curOuterParams)
  {
    nlp = (NestLoopParam *)lfirst(lc);
    if (equal(var, nlp->paramval))
    {
                                                                 
      param = makeNode(Param);
      param->paramkind = PARAM_EXEC;
      param->paramid = nlp->paramno;
      param->paramtype = var->vartype;
      param->paramtypmod = var->vartypmod;
      param->paramcollid = var->varcollid;
      param->location = var->location;
      return param;
    }
  }

                                                     
  param = generate_new_exec_param(root, var->vartype, var->vartypmod, var->varcollid);
  param->location = var->location;

                                           
  nlp = makeNode(NestLoopParam);
  nlp->paramno = param->paramid;
  nlp->paramval = copyObject(var);
  root->curOuterParams = lappend(root->curOuterParams, nlp);

                                        
  return param;
}

   
                                                              
                                                                 
                                                        
   
                                                                             
   
Param *
replace_nestloop_param_placeholdervar(PlannerInfo *root, PlaceHolderVar *phv)
{
  Param *param;
  NestLoopParam *nlp;
  ListCell *lc;

                                                           
  foreach (lc, root->curOuterParams)
  {
    nlp = (NestLoopParam *)lfirst(lc);
    if (equal(phv, nlp->paramval))
    {
                                                                 
      param = makeNode(Param);
      param->paramkind = PARAM_EXEC;
      param->paramid = nlp->paramno;
      param->paramtype = exprType((Node *)phv->phexpr);
      param->paramtypmod = exprTypmod((Node *)phv->phexpr);
      param->paramcollid = exprCollation((Node *)phv->phexpr);
      param->location = -1;
      return param;
    }
  }

                                                     
  param = generate_new_exec_param(root, exprType((Node *)phv->phexpr), exprTypmod((Node *)phv->phexpr), exprCollation((Node *)phv->phexpr));

                                           
  nlp = makeNode(NestLoopParam);
  nlp->paramno = param->paramid;
  nlp->paramval = (Var *)copyObject(phv);
  root->curOuterParams = lappend(root->curOuterParams, nlp);

                                        
  return param;
}

   
                                    
                                                                   
                             
   
                                                                              
                                                                        
   
                                                                           
                                                                         
                                                                        
   
                                                                           
                                                                              
                                                                          
                                                                    
                                                                             
                                                                          
                                                                  
   
                                                                         
                  
   
void
process_subquery_nestloop_params(PlannerInfo *root, List *subplan_params)
{
  ListCell *lc;

  foreach (lc, subplan_params)
  {
    PlannerParamItem *pitem = castNode(PlannerParamItem, lfirst(lc));

    if (IsA(pitem->item, Var))
    {
      Var *var = (Var *)pitem->item;
      NestLoopParam *nlp;
      ListCell *lc;

                                                      
      if (!bms_is_member(var->varno, root->curOuterRels))
      {
        elog(ERROR, "non-LATERAL parameter required by subquery");
      }

                                                                 
      foreach (lc, root->curOuterParams)
      {
        nlp = (NestLoopParam *)lfirst(lc);
        if (nlp->paramno == pitem->paramId)
        {
          Assert(equal(var, nlp->paramval));
                                         
          break;
        }
      }
      if (lc == NULL)
      {
                           
        nlp = makeNode(NestLoopParam);
        nlp->paramno = pitem->paramId;
        nlp->paramval = copyObject(var);
        root->curOuterParams = lappend(root->curOuterParams, nlp);
      }
    }
    else if (IsA(pitem->item, PlaceHolderVar))
    {
      PlaceHolderVar *phv = (PlaceHolderVar *)pitem->item;
      NestLoopParam *nlp;
      ListCell *lc;

                                                      
      if (!bms_is_subset(find_placeholder_info(root, phv, false)->ph_eval_at, root->curOuterRels))
      {
        elog(ERROR, "non-LATERAL parameter required by subquery");
      }

                                                                 
      foreach (lc, root->curOuterParams)
      {
        nlp = (NestLoopParam *)lfirst(lc);
        if (nlp->paramno == pitem->paramId)
        {
          Assert(equal(phv, nlp->paramval));
                                         
          break;
        }
      }
      if (lc == NULL)
      {
                           
        nlp = makeNode(NestLoopParam);
        nlp->paramno = pitem->paramId;
        nlp->paramval = (Var *)copyObject(phv);
        root->curOuterParams = lappend(root->curOuterParams, nlp);
      }
    }
    else
    {
      elog(ERROR, "unexpected type of subquery parameter");
    }
  }
}

   
                                                                          
                                                                       
                                                                 
   
List *
identify_current_nestloop_params(PlannerInfo *root, Relids leftrelids)
{
  List *result;
  ListCell *cell;
  ListCell *prev;
  ListCell *next;

  result = NIL;
  prev = NULL;
  for (cell = list_head(root->curOuterParams); cell; cell = next)
  {
    NestLoopParam *nlp = (NestLoopParam *)lfirst(cell);

    next = lnext(cell);

       
                                                                    
                                                                         
                                                                         
       
    if (IsA(nlp->paramval, Var) && bms_is_member(nlp->paramval->varno, leftrelids))
    {
      root->curOuterParams = list_delete_cell(root->curOuterParams, cell, prev);
      result = lappend(result, nlp);
    }
    else if (IsA(nlp->paramval, PlaceHolderVar) && bms_overlap(((PlaceHolderVar *)nlp->paramval)->phrels, leftrelids) && bms_is_subset(find_placeholder_info(root, (PlaceHolderVar *)nlp->paramval, false)->ph_eval_at, leftrelids))
    {
      root->curOuterParams = list_delete_cell(root->curOuterParams, cell, prev);
      result = lappend(result, nlp);
    }
    else
    {
      prev = cell;
    }
  }
  return result;
}

   
                                                                    
   
                                                                 
                        
   
                                                                         
                                                                      
                                            
   
Param *
generate_new_exec_param(PlannerInfo *root, Oid paramtype, int32 paramtypmod, Oid paramcollation)
{
  Param *retval;

  retval = makeNode(Param);
  retval->paramkind = PARAM_EXEC;
  retval->paramid = list_length(root->glob->paramExecTypes);
  root->glob->paramExecTypes = lappend_oid(root->glob->paramExecTypes, paramtype);
  retval->paramtype = paramtype;
  retval->paramtypmod = paramtypmod;
  retval->paramcollid = paramcollation;
  retval->location = -1;

  return retval;
}

   
                                                                          
                                                                           
                                                                     
                                                                   
                                                                          
                                 
   
int
assign_special_exec_param(PlannerInfo *root)
{
  int paramId = list_length(root->glob->paramExecTypes);

  root->glob->paramExecTypes = lappend_oid(root->glob->paramExecTypes, InvalidOid);
  return paramId;
}
