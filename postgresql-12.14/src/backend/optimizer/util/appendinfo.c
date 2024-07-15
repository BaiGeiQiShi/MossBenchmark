                                                                            
   
                
                                                                
   
                                                                         
                                                                        
   
   
                  
                                             
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/appendinfo.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

typedef struct
{
  PlannerInfo *root;
  int nappinfos;
  AppendRelInfo **appinfos;
} adjust_appendrel_attrs_context;

static void
make_inh_translation_list(Relation oldrelation, Relation newrelation, Index newvarno, List **translated_vars);
static Node *
adjust_appendrel_attrs_mutator(Node *node, adjust_appendrel_attrs_context *context);
static List *
adjust_inherited_tlist(List *tlist, AppendRelInfo *context);

   
                        
                                                      
   
AppendRelInfo *
make_append_rel_info(Relation parentrel, Relation childrel, Index parentRTindex, Index childRTindex)
{
  AppendRelInfo *appinfo = makeNode(AppendRelInfo);

  appinfo->parent_relid = parentRTindex;
  appinfo->child_relid = childRTindex;
  appinfo->parent_reltype = parentrel->rd_rel->reltype;
  appinfo->child_reltype = childrel->rd_rel->reltype;
  make_inh_translation_list(parentrel, childrel, childRTindex, &appinfo->translated_vars);
  appinfo->parent_reloid = RelationGetRelid(parentrel);

  return appinfo;
}

   
                             
                                                                       
                           
   
                                                                           
   
static void
make_inh_translation_list(Relation oldrelation, Relation newrelation, Index newvarno, List **translated_vars)
{
  List *vars = NIL;
  TupleDesc old_tupdesc = RelationGetDescr(oldrelation);
  TupleDesc new_tupdesc = RelationGetDescr(newrelation);
  Oid new_relid = RelationGetRelid(newrelation);
  int oldnatts = old_tupdesc->natts;
  int newnatts = new_tupdesc->natts;
  int old_attno;
  int new_attno = 0;

  for (old_attno = 0; old_attno < oldnatts; old_attno++)
  {
    Form_pg_attribute att;
    char *attname;
    Oid atttypid;
    int32 atttypmod;
    Oid attcollation;

    att = TupleDescAttr(old_tupdesc, old_attno);
    if (att->attisdropped)
    {
                                              
      vars = lappend(vars, NULL);
      continue;
    }
    attname = NameStr(att->attname);
    atttypid = att->atttypid;
    atttypmod = att->atttypmod;
    attcollation = att->attcollation;

       
                                                                          
                                                             
       
    if (oldrelation == newrelation)
    {
      vars = lappend(vars, makeVar(newvarno, (AttrNumber)(old_attno + 1), atttypid, atttypmod, attcollation, 0));
      continue;
    }

       
                                                                    
                                                                         
                                                                      
                                                                         
                                                                         
                                                                         
                                      
       
    if (new_attno >= newnatts || (att = TupleDescAttr(new_tupdesc, new_attno))->attisdropped || strcmp(attname, NameStr(att->attname)) != 0)
    {
      HeapTuple newtup;

      newtup = SearchSysCacheAttName(new_relid, attname);
      if (!HeapTupleIsValid(newtup))
      {
        elog(ERROR, "could not find inherited attribute \"%s\" of relation \"%s\"", attname, RelationGetRelationName(newrelation));
      }
      new_attno = ((Form_pg_attribute)GETSTRUCT(newtup))->attnum - 1;
      ReleaseSysCache(newtup);

      att = TupleDescAttr(new_tupdesc, new_attno);
    }

                                                  
    if (atttypid != att->atttypid || atttypmod != att->atttypmod)
    {
      elog(ERROR, "attribute \"%s\" of relation \"%s\" does not match parent's type", attname, RelationGetRelationName(newrelation));
    }
    if (attcollation != att->attcollation)
    {
      elog(ERROR, "attribute \"%s\" of relation \"%s\" does not match parent's collation", attname, RelationGetRelationName(newrelation));
    }

    vars = lappend(vars, makeVar(newvarno, (AttrNumber)(new_attno + 1), atttypid, atttypmod, attcollation, 0));
    new_attno++;
  }

  *translated_vars = vars;
}

   
                          
                                                                              
                                                                          
                                                                         
                      
   
                                                                        
                                                             
   
                                                                            
                                                          
   
Node *
adjust_appendrel_attrs(PlannerInfo *root, Node *node, int nappinfos, AppendRelInfo **appinfos)
{
  Node *result;
  adjust_appendrel_attrs_context context;

  context.root = root;
  context.nappinfos = nappinfos;
  context.appinfos = appinfos;

                                                               
  Assert(nappinfos >= 1 && appinfos != NULL);

     
                                                                       
     
  if (node && IsA(node, Query))
  {
    Query *newnode;
    int cnt;

    newnode = query_tree_mutator((Query *)node, adjust_appendrel_attrs_mutator, (void *)&context, QTW_IGNORE_RC_SUBQUERIES);
    for (cnt = 0; cnt < nappinfos; cnt++)
    {
      AppendRelInfo *appinfo = appinfos[cnt];

      if (newnode->resultRelation == appinfo->parent_relid)
      {
        newnode->resultRelation = appinfo->child_relid;
                                                            
        if (newnode->commandType == CMD_UPDATE)
        {
          newnode->targetList = adjust_inherited_tlist(newnode->targetList, appinfo);
        }
        break;
      }
    }

    result = (Node *)newnode;
  }
  else
  {
    result = adjust_appendrel_attrs_mutator(node, &context);
  }

  return result;
}

static Node *
adjust_appendrel_attrs_mutator(Node *node, adjust_appendrel_attrs_context *context)
{
  AppendRelInfo **appinfos = context->appinfos;
  int nappinfos = context->nappinfos;
  int cnt;

  if (node == NULL)
  {
    return NULL;
  }
  if (IsA(node, Var))
  {
    Var *var = (Var *)copyObject(node);
    AppendRelInfo *appinfo = NULL;

    for (cnt = 0; cnt < nappinfos; cnt++)
    {
      if (var->varno == appinfos[cnt]->parent_relid)
      {
        appinfo = appinfos[cnt];
        break;
      }
    }

    if (var->varlevelsup == 0 && appinfo)
    {
      var->varno = appinfo->child_relid;
      var->varnoold = appinfo->child_relid;
      if (var->varattno > 0)
      {
        Node *newnode;

        if (var->varattno > list_length(appinfo->translated_vars))
        {
          elog(ERROR, "attribute %d of relation \"%s\" does not exist", var->varattno, get_rel_name(appinfo->parent_reloid));
        }
        newnode = copyObject(list_nth(appinfo->translated_vars, var->varattno - 1));
        if (newnode == NULL)
        {
          elog(ERROR, "attribute %d of relation \"%s\" does not exist", var->varattno, get_rel_name(appinfo->parent_reloid));
        }
        return newnode;
      }
      else if (var->varattno == 0)
      {
           
                                                                    
                                                                       
                                                                     
                                                    
           
        if (OidIsValid(appinfo->child_reltype))
        {
          Assert(var->vartype == appinfo->parent_reltype);
          if (appinfo->parent_reltype != appinfo->child_reltype)
          {
            ConvertRowtypeExpr *r = makeNode(ConvertRowtypeExpr);

            r->arg = (Expr *)var;
            r->resulttype = appinfo->parent_reltype;
            r->convertformat = COERCE_IMPLICIT_CAST;
            r->location = -1;
                                                                   
            var->vartype = appinfo->child_reltype;
            return (Node *)r;
          }
        }
        else
        {
             
                                                                  
             
                                                                     
                                                                    
                                                   
             
                                                                     
                                                                    
                               
             
          RowExpr *rowexpr;
          List *fields;
          RangeTblEntry *rte;

          rte = rt_fetch(appinfo->parent_relid, context->root->parse->rtable);
          fields = copyObject(appinfo->translated_vars);
          rowexpr = makeNode(RowExpr);
          rowexpr->args = fields;
          rowexpr->row_typeid = var->vartype;
          rowexpr->row_format = COERCE_IMPLICIT_CAST;
          rowexpr->colnames = copyObject(rte->eref->colnames);
          rowexpr->location = -1;

          return (Node *)rowexpr;
        }
      }
                                                              
    }
    return (Node *)var;
  }
  if (IsA(node, CurrentOfExpr))
  {
    CurrentOfExpr *cexpr = (CurrentOfExpr *)copyObject(node);

    for (cnt = 0; cnt < nappinfos; cnt++)
    {
      AppendRelInfo *appinfo = appinfos[cnt];

      if (cexpr->cvarno == appinfo->parent_relid)
      {
        cexpr->cvarno = appinfo->child_relid;
        break;
      }
    }
    return (Node *)cexpr;
  }
  if (IsA(node, RangeTblRef))
  {
    RangeTblRef *rtr = (RangeTblRef *)copyObject(node);

    for (cnt = 0; cnt < nappinfos; cnt++)
    {
      AppendRelInfo *appinfo = appinfos[cnt];

      if (rtr->rtindex == appinfo->parent_relid)
      {
        rtr->rtindex = appinfo->child_relid;
        break;
      }
    }
    return (Node *)rtr;
  }
  if (IsA(node, JoinExpr))
  {
                                                                  
    JoinExpr *j;
    AppendRelInfo *appinfo;

    j = (JoinExpr *)expression_tree_mutator(node, adjust_appendrel_attrs_mutator, (void *)context);
                                                             
    for (cnt = 0; cnt < nappinfos; cnt++)
    {
      appinfo = appinfos[cnt];

      if (j->rtindex == appinfo->parent_relid)
      {
        j->rtindex = appinfo->child_relid;
        break;
      }
    }
    return (Node *)j;
  }
  if (IsA(node, PlaceHolderVar))
  {
                                                                        
    PlaceHolderVar *phv;

    phv = (PlaceHolderVar *)expression_tree_mutator(node, adjust_appendrel_attrs_mutator, (void *)context);
                                             
    if (phv->phlevelsup == 0)
    {
      phv->phrels = adjust_child_relids(phv->phrels, context->nappinfos, context->appinfos);
    }
    return (Node *)phv;
  }
                                                             
  Assert(!IsA(node, SpecialJoinInfo));
  Assert(!IsA(node, AppendRelInfo));
  Assert(!IsA(node, PlaceHolderInfo));
  Assert(!IsA(node, MinMaxAggInfo));

     
                                                                       
                                                                     
                                                                        
     
  if (IsA(node, RestrictInfo))
  {
    RestrictInfo *oldinfo = (RestrictInfo *)node;
    RestrictInfo *newinfo = makeNode(RestrictInfo);

                                       
    memcpy(newinfo, oldinfo, sizeof(RestrictInfo));

                                           
    newinfo->clause = (Expr *)adjust_appendrel_attrs_mutator((Node *)oldinfo->clause, context);

                                                   
    newinfo->orclause = (Expr *)adjust_appendrel_attrs_mutator((Node *)oldinfo->orclause, context);

                               
    newinfo->clause_relids = adjust_child_relids(oldinfo->clause_relids, context->nappinfos, context->appinfos);
    newinfo->required_relids = adjust_child_relids(oldinfo->required_relids, context->nappinfos, context->appinfos);
    newinfo->outer_relids = adjust_child_relids(oldinfo->outer_relids, context->nappinfos, context->appinfos);
    newinfo->nullable_relids = adjust_child_relids(oldinfo->nullable_relids, context->nappinfos, context->appinfos);
    newinfo->left_relids = adjust_child_relids(oldinfo->left_relids, context->nappinfos, context->appinfos);
    newinfo->right_relids = adjust_child_relids(oldinfo->right_relids, context->nappinfos, context->appinfos);

       
                                                                      
                                                                      
                                                                       
                                                                          
       
    newinfo->eval_cost.startup = -1;
    newinfo->norm_selec = -1;
    newinfo->outer_selec = -1;
    newinfo->left_em = NULL;
    newinfo->right_em = NULL;
    newinfo->scansel_cache = NIL;
    newinfo->left_bucketsize = -1;
    newinfo->right_bucketsize = -1;
    newinfo->left_mcvfreq = -1;
    newinfo->right_mcvfreq = -1;

    return (Node *)newinfo;
  }

     
                                                                        
                                                                 
     
  Assert(!IsA(node, SubLink));
  Assert(!IsA(node, Query));

  return expression_tree_mutator(node, adjust_appendrel_attrs_mutator, (void *)context);
}

   
                                     
                                                                              
   
                                                                                
                                                                           
   
Node *
adjust_appendrel_attrs_multilevel(PlannerInfo *root, Node *node, Relids child_relids, Relids top_parent_relids)
{
  AppendRelInfo **appinfos;
  Bitmapset *parent_relids = NULL;
  int nappinfos;
  int cnt;

  Assert(bms_num_members(child_relids) == bms_num_members(top_parent_relids));

  appinfos = find_appinfos_by_relids(root, child_relids, &nappinfos);

                                                                     
  for (cnt = 0; cnt < nappinfos; cnt++)
  {
    AppendRelInfo *appinfo = appinfos[cnt];

    parent_relids = bms_add_member(parent_relids, appinfo->parent_relid);
  }

                                                          
  if (!bms_equal(parent_relids, top_parent_relids))
  {
    node = adjust_appendrel_attrs_multilevel(root, node, parent_relids, top_parent_relids);
  }

                                    
  node = adjust_appendrel_attrs(root, node, nappinfos, appinfos);

  pfree(appinfos);

  return node;
}

   
                                                                           
                                                         
   
Relids
adjust_child_relids(Relids relids, int nappinfos, AppendRelInfo **appinfos)
{
  Bitmapset *result = NULL;
  int cnt;

  for (cnt = 0; cnt < nappinfos; cnt++)
  {
    AppendRelInfo *appinfo = appinfos[cnt];

                                  
    if (bms_is_member(appinfo->parent_relid, relids))
    {
                                                   
      if (!result)
      {
        result = bms_copy(relids);
      }

      result = bms_del_member(result, appinfo->parent_relid);
      result = bms_add_member(result, appinfo->child_relid);
    }
  }

                                                         
  if (result)
  {
    return result;
  }

                                                                
  return relids;
}

   
                                                                    
                                                                          
                                      
   
Relids
adjust_child_relids_multilevel(PlannerInfo *root, Relids relids, Relids child_relids, Relids top_parent_relids)
{
  AppendRelInfo **appinfos;
  int nappinfos;
  Relids parent_relids = NULL;
  Relids result;
  Relids tmp_result = NULL;
  int cnt;

     
                                                                           
                               
     
  if (!bms_overlap(relids, top_parent_relids))
  {
    return relids;
  }

  appinfos = find_appinfos_by_relids(root, child_relids, &nappinfos);

                                                                         
  for (cnt = 0; cnt < nappinfos; cnt++)
  {
    AppendRelInfo *appinfo = appinfos[cnt];

    parent_relids = bms_add_member(parent_relids, appinfo->parent_relid);
  }

                                                          
  if (!bms_equal(parent_relids, top_parent_relids))
  {
    tmp_result = adjust_child_relids_multilevel(root, relids, parent_relids, top_parent_relids);
    relids = tmp_result;
  }

  result = adjust_child_relids(relids, nappinfos, appinfos);

                                                        
  if (tmp_result)
  {
    bms_free(tmp_result);
  }
  bms_free(parent_relids);
  pfree(appinfos);

  return result;
}

   
                                                                  
   
                                                                          
                                                                         
                                                                          
                                                                       
                                                                         
                      
   
                                                                     
                                                                      
                
   
                                                                             
   
static List *
adjust_inherited_tlist(List *tlist, AppendRelInfo *context)
{
  bool changed_it = false;
  ListCell *tl;
  List *new_tlist;
  bool more;
  int attrno;

                                                                      
  Assert(OidIsValid(context->parent_reloid));

                                                                  
  foreach (tl, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(tl);
    Var *childvar;

    if (tle->resjunk)
    {
      continue;                        
    }

                                                                  
    if (tle->resno <= 0 || tle->resno > list_length(context->translated_vars))
    {
      elog(ERROR, "attribute %d of relation \"%s\" does not exist", tle->resno, get_rel_name(context->parent_reloid));
    }
    childvar = (Var *)list_nth(context->translated_vars, tle->resno - 1);
    if (childvar == NULL || !IsA(childvar, Var))
    {
      elog(ERROR, "attribute %d of relation \"%s\" does not exist", tle->resno, get_rel_name(context->parent_reloid));
    }

    if (tle->resno != childvar->varattno)
    {
      tle->resno = childvar->varattno;
      changed_it = true;
    }
  }

     
                                                                       
                                                                      
                                                                           
                                
     
  if (!changed_it)
  {
    return tlist;
  }

  new_tlist = NIL;
  more = true;
  for (attrno = 1; more; attrno++)
  {
    more = false;
    foreach (tl, tlist)
    {
      TargetEntry *tle = (TargetEntry *)lfirst(tl);

      if (tle->resjunk)
      {
        continue;                        
      }

      if (tle->resno == attrno)
      {
        new_tlist = lappend(new_tlist, tle);
      }
      else if (tle->resno > attrno)
      {
        more = true;
      }
    }
  }

  foreach (tl, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(tl);

    if (!tle->resjunk)
    {
      continue;                                  
    }

    tle->resno = attrno;
    new_tlist = lappend(new_tlist, tle);
    attrno++;
  }

  return new_tlist;
}

   
                           
                                                                          
   
                                                                            
                                                                    
   
AppendRelInfo **
find_appinfos_by_relids(PlannerInfo *root, Relids relids, int *nappinfos)
{
  AppendRelInfo **appinfos;
  int cnt = 0;
  int i;

  *nappinfos = bms_num_members(relids);
  appinfos = (AppendRelInfo **)palloc(sizeof(AppendRelInfo *) * *nappinfos);

  i = -1;
  while ((i = bms_next_member(relids, i)) >= 0)
  {
    AppendRelInfo *appinfo = root->append_rel_array[i];

    if (!appinfo)
    {
      elog(ERROR, "child rel %d not found in append_rel_array", i);
    }

    appinfos[cnt++] = appinfo;
  }
  return appinfos;
}
