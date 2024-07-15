   
                         
                                                                    
   
                                                                        
                                                                              
   
                                                                              
                                                                              
                                                                             
            
   
                                                                          
                                                                        
                                                                         
                                                                          
                                                                             
                                                                            
                                                                         
                                                                            
                                                                          
                                                                              
                                                         
   
                                                                 
                                                                           
                                                                             
                                                                      
                                                                              
                             
   
                                                                         
                                                                        
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/pg_class.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_policy.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/pg_list.h"
#include "nodes/plannodes.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteDefine.h"
#include "rewrite/rewriteHandler.h"
#include "rewrite/rewriteManip.h"
#include "rewrite/rowsecurity.h"
#include "utils/acl.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/rls.h"
#include "utils/syscache.h"
#include "tcop/utility.h"

static void
get_policies_for_relation(Relation relation, CmdType cmd, Oid user_id, List **permissive_policies, List **restrictive_policies);

static List *
sort_policies_by_name(List *policies);

static int
row_security_policy_cmp(const void *a, const void *b);

static void
add_security_quals(int rt_index, List *permissive_policies, List *restrictive_policies, List **securityQuals, bool *hasSubLinks);

static void
add_with_check_options(Relation rel, int rt_index, WCOKind kind, List *permissive_policies, List *restrictive_policies, List **withCheckOptions, bool *hasSubLinks, bool force_using);

static bool
check_role_for_policy(ArrayType *policy_roles, Oid user_id);

   
                                                                
   
                                                                         
                                                              
   
                                                                          
                                                                             
   
row_security_policy_hook_type row_security_policy_hook_permissive = NULL;
row_security_policy_hook_type row_security_policy_hook_restrictive = NULL;

   
                                                                        
                                 
   
                                                                               
                                                                              
                                                              
   
void
get_row_security_policies(Query *root, RangeTblEntry *rte, int rt_index, List **securityQuals, List **withCheckOptions, bool *hasRowSecurity, bool *hasSubLinks)
{
  Oid user_id;
  int rls_status;
  Relation rel;
  CmdType commandType;
  List *permissive_policies;
  List *restrictive_policies;

                                      
  *securityQuals = NIL;
  *withCheckOptions = NIL;
  *hasRowSecurity = false;
  *hasSubLinks = false;

                                                                 
  if (rte->relkind != RELKIND_RELATION && rte->relkind != RELKIND_PARTITIONED_TABLE)
  {
    return;
  }

                                         
  user_id = rte->checkAsUser ? rte->checkAsUser : GetUserId();

                                                                        
  rls_status = check_enable_rls(rte->relid, rte->checkAsUser, false);

                                                              
  if (rls_status == RLS_NONE)
  {
    return;
  }

     
                                                                          
                                                                         
                                                   
     
  if (rls_status == RLS_NONE_ENV)
  {
       
                                                                      
                                                                         
                             
       
    *hasRowSecurity = true;

    return;
  }

     
                                       
     
                                                                            
                                                                          
                                                                           
                                                                       
                                        
     
  rel = table_open(rte->relid, NoLock);

  commandType = rt_index == root->resultRelation ? root->commandType : CMD_SELECT;

     
                                                                       
                                                                        
                            
     
                                                                           
                                                                            
                                                                            
                     
     
                                                                          
                              
     
                                                                             
                                                                 
                                                                    
                          
     

     
                                                                       
                                                                        
            
     
                                                                          
                                                                           
                                                                       
     
  if (commandType == CMD_SELECT && rte->requiredPerms & ACL_UPDATE)
  {
    List *update_permissive_policies;
    List *update_restrictive_policies;

    get_policies_for_relation(rel, CMD_UPDATE, user_id, &update_permissive_policies, &update_restrictive_policies);

    add_security_quals(rt_index, update_permissive_policies, update_restrictive_policies, securityQuals, hasSubLinks);
  }

     
                                                                            
                                                                            
                                                                          
                                              
     

  get_policies_for_relation(rel, commandType, user_id, &permissive_policies, &restrictive_policies);

  if (commandType == CMD_SELECT || commandType == CMD_UPDATE || commandType == CMD_DELETE)
  {
    add_security_quals(rt_index, permissive_policies, restrictive_policies, securityQuals, hasSubLinks);
  }

     
                                                                             
                                                                            
                                                                             
                                                                    
     
                                                                          
                                 
     
  if ((commandType == CMD_UPDATE || commandType == CMD_DELETE) && rte->requiredPerms & ACL_SELECT)
  {
    List *select_permissive_policies;
    List *select_restrictive_policies;

    get_policies_for_relation(rel, CMD_SELECT, user_id, &select_permissive_policies, &select_restrictive_policies);

    add_security_quals(rt_index, select_permissive_policies, select_restrictive_policies, securityQuals, hasSubLinks);
  }

     
                                                                        
                                                                             
                                                                         
                                   
     
  if (commandType == CMD_INSERT || commandType == CMD_UPDATE)
  {
                                            
    Assert(rt_index == root->resultRelation);

    add_with_check_options(rel, rt_index, commandType == CMD_INSERT ? WCO_RLS_INSERT_CHECK : WCO_RLS_UPDATE_CHECK, permissive_policies, restrictive_policies, withCheckOptions, hasSubLinks, false);

       
                                                                          
                                                                           
                                                                      
                                                                           
                                  
       
    if (rte->requiredPerms & ACL_SELECT)
    {
      List *select_permissive_policies = NIL;
      List *select_restrictive_policies = NIL;

      get_policies_for_relation(rel, CMD_SELECT, user_id, &select_permissive_policies, &select_restrictive_policies);
      add_with_check_options(rel, rt_index, commandType == CMD_INSERT ? WCO_RLS_INSERT_CHECK : WCO_RLS_UPDATE_CHECK, select_permissive_policies, select_restrictive_policies, withCheckOptions, hasSubLinks, true);
    }

       
                                                                      
                                                                   
       
    if (commandType == CMD_INSERT && root->onConflict && root->onConflict->action == ONCONFLICT_UPDATE)
    {
      List *conflict_permissive_policies;
      List *conflict_restrictive_policies;
      List *conflict_select_permissive_policies = NIL;
      List *conflict_select_restrictive_policies = NIL;

                                                               
      get_policies_for_relation(rel, CMD_UPDATE, user_id, &conflict_permissive_policies, &conflict_restrictive_policies);

         
                                                                     
                                                                    
                                                                     
                                                        
         
      add_with_check_options(rel, rt_index, WCO_RLS_CONFLICT_CHECK, conflict_permissive_policies, conflict_restrictive_policies, withCheckOptions, hasSubLinks, true);

         
                                                                         
                                                                         
                                                                        
                                                                  
                                             
         
      if (rte->requiredPerms & ACL_SELECT)
      {
        get_policies_for_relation(rel, CMD_SELECT, user_id, &conflict_select_permissive_policies, &conflict_select_restrictive_policies);
        add_with_check_options(rel, rt_index, WCO_RLS_CONFLICT_CHECK, conflict_select_permissive_policies, conflict_select_restrictive_policies, withCheckOptions, hasSubLinks, true);
      }

                                                                 
      add_with_check_options(rel, rt_index, WCO_RLS_UPDATE_CHECK, conflict_permissive_policies, conflict_restrictive_policies, withCheckOptions, hasSubLinks, false);

         
                                                                         
                                                                      
                                                                      
                                         
         
      if (rte->requiredPerms & ACL_SELECT)
      {
        add_with_check_options(rel, rt_index, WCO_RLS_UPDATE_CHECK, conflict_select_permissive_policies, conflict_select_restrictive_policies, withCheckOptions, hasSubLinks, true);
      }
    }
  }

  table_close(rel, NoLock);

     
                                                                            
                                                                       
     
  setRuleCheckAsUser((Node *)*securityQuals, rte->checkAsUser);
  setRuleCheckAsUser((Node *)*withCheckOptions, rte->checkAsUser);

     
                                                                            
                                       
     
  *hasRowSecurity = true;

  return;
}

   
                             
   
                                                                             
                                                           
   
                                                   
   
static void
get_policies_for_relation(Relation relation, CmdType cmd, Oid user_id, List **permissive_policies, List **restrictive_policies)
{
  ListCell *item;

  *permissive_policies = NIL;
  *restrictive_policies = NIL;

                                                          
  foreach (item, relation->rd_rsdesc->policies)
  {
    bool cmd_matches = false;
    RowSecurityPolicy *policy = (RowSecurityPolicy *)lfirst(item);

                                                 
    if (policy->polcmd == '*')
    {
      cmd_matches = true;
    }
    else
    {
                                                                          
      switch (cmd)
      {
      case CMD_SELECT:
        if (policy->polcmd == ACL_SELECT_CHR)
        {
          cmd_matches = true;
        }
        break;
      case CMD_INSERT:
        if (policy->polcmd == ACL_INSERT_CHR)
        {
          cmd_matches = true;
        }
        break;
      case CMD_UPDATE:
        if (policy->polcmd == ACL_UPDATE_CHR)
        {
          cmd_matches = true;
        }
        break;
      case CMD_DELETE:
        if (policy->polcmd == ACL_DELETE_CHR)
        {
          cmd_matches = true;
        }
        break;
      default:
        elog(ERROR, "unrecognized policy command type %d", (int)cmd);
        break;
      }
    }

       
                                                                         
                           
       
    if (cmd_matches && check_role_for_policy(policy->roles, user_id))
    {
      if (policy->permissive)
      {
        *permissive_policies = lappend(*permissive_policies, policy);
      }
      else
      {
        *restrictive_policies = lappend(*restrictive_policies, policy);
      }
    }
  }

     
                                                                             
                                      
     
  *restrictive_policies = sort_policies_by_name(*restrictive_policies);

     
                                                                            
                                                                          
                                  
     
  if (row_security_policy_hook_restrictive)
  {
    List *hook_policies = (*row_security_policy_hook_restrictive)(cmd, relation);

       
                                                                        
                                                                           
                                                                      
                                                                           
       
    hook_policies = sort_policies_by_name(hook_policies);

    foreach (item, hook_policies)
    {
      RowSecurityPolicy *policy = (RowSecurityPolicy *)lfirst(item);

      if (check_role_for_policy(policy->roles, user_id))
      {
        *restrictive_policies = lappend(*restrictive_policies, policy);
      }
    }
  }

  if (row_security_policy_hook_permissive)
  {
    List *hook_policies = (*row_security_policy_hook_permissive)(cmd, relation);

    foreach (item, hook_policies)
    {
      RowSecurityPolicy *policy = (RowSecurityPolicy *)lfirst(item);

      if (check_role_for_policy(policy->roles, user_id))
      {
        *permissive_policies = lappend(*permissive_policies, policy);
      }
    }
  }
}

   
                         
   
                                                                 
                                                                       
                                                                              
                                                          
   
static List *
sort_policies_by_name(List *policies)
{
  int npol = list_length(policies);
  RowSecurityPolicy *pols;
  ListCell *item;
  int ii = 0;

  if (npol <= 1)
  {
    return policies;
  }

  pols = (RowSecurityPolicy *)palloc(sizeof(RowSecurityPolicy) * npol);

  foreach (item, policies)
  {
    RowSecurityPolicy *policy = (RowSecurityPolicy *)lfirst(item);

    pols[ii++] = *policy;
  }

  qsort(pols, npol, sizeof(RowSecurityPolicy), row_security_policy_cmp);

  policies = NIL;
  for (ii = 0; ii < npol; ii++)
  {
    policies = lappend(policies, &pols[ii]);
  }

  return policies;
}

   
                                                              
   
static int
row_security_policy_cmp(const void *a, const void *b)
{
  const RowSecurityPolicy *pa = (const RowSecurityPolicy *)a;
  const RowSecurityPolicy *pb = (const RowSecurityPolicy *)b;

                                                       
  if (pa->policy_name == NULL)
  {
    return pb->policy_name == NULL ? 0 : 1;
  }
  if (pb->policy_name == NULL)
  {
    return -1;
  }

  return strcmp(pa->policy_name, pb->policy_name);
}

   
                      
   
                                                                         
                                                                             
                                                                            
                                
   
                                                                            
                                                              
   
static void
add_security_quals(int rt_index, List *permissive_policies, List *restrictive_policies, List **securityQuals, bool *hasSubLinks)
{
  ListCell *item;
  List *permissive_quals = NIL;
  Expr *rowsec_expr;

     
                                                                   
                                                                           
     
  foreach (item, permissive_policies)
  {
    RowSecurityPolicy *policy = (RowSecurityPolicy *)lfirst(item);

    if (policy->qual != NULL)
    {
      permissive_quals = lappend(permissive_quals, copyObject(policy->qual));
      *hasSubLinks |= policy->hassublinks;
    }
  }

     
                                                                    
     
                                                                             
                               
     
  if (permissive_quals != NIL)
  {
       
                                                                     
                                                                      
                                                                         
                                        
       
    foreach (item, restrictive_policies)
    {
      RowSecurityPolicy *policy = (RowSecurityPolicy *)lfirst(item);
      Expr *qual;

      if (policy->qual != NULL)
      {
        qual = copyObject(policy->qual);
        ChangeVarNodes((Node *)qual, 1, rt_index, 0);

        *securityQuals = list_append_unique(*securityQuals, qual);
        *hasSubLinks |= policy->hassublinks;
      }
    }

       
                                                                    
                                                          
       
    if (list_length(permissive_quals) == 1)
    {
      rowsec_expr = (Expr *)linitial(permissive_quals);
    }
    else
    {
      rowsec_expr = makeBoolExpr(OR_EXPR, permissive_quals, -1);
    }

    ChangeVarNodes((Node *)rowsec_expr, 1, rt_index, 0);
    *securityQuals = list_append_unique(*securityQuals, rowsec_expr);
  }
  else
  {

       
                                                                     
                                                                       
                                   
       
    *securityQuals = lappend(*securityQuals, makeConst(BOOLOID, -1, InvalidOid, sizeof(bool), BoolGetDatum(false), false, true));
  }
}

   
                          
   
                                                                        
                                                                      
                                                                             
                                                                              
                                                                      
                                                                              
                                                                         
                              
   
                                                                             
                                                              
   
static void
add_with_check_options(Relation rel, int rt_index, WCOKind kind, List *permissive_policies, List *restrictive_policies, List **withCheckOptions, bool *hasSubLinks, bool force_using)
{
  ListCell *item;
  List *permissive_quals = NIL;

#define QUAL_FOR_WCO(policy) (!force_using && (policy)->with_check_qual != NULL ? (policy)->with_check_qual : (policy)->qual)

     
                                                                
                         
     
  foreach (item, permissive_policies)
  {
    RowSecurityPolicy *policy = (RowSecurityPolicy *)lfirst(item);
    Expr *qual = QUAL_FOR_WCO(policy);

    if (qual != NULL)
    {
      permissive_quals = lappend(permissive_quals, copyObject(qual));
      *hasSubLinks |= policy->hassublinks;
    }
  }

     
                                                                             
                                                              
     
                                                                        
                                                  
     
  if (permissive_quals != NIL)
  {
       
                                                                           
                                                                         
                                                                           
                                                                      
                 
       
    WithCheckOption *wco;

    wco = makeNode(WithCheckOption);
    wco->kind = kind;
    wco->relname = pstrdup(RelationGetRelationName(rel));
    wco->polname = NULL;
    wco->cascaded = false;

    if (list_length(permissive_quals) == 1)
    {
      wco->qual = (Node *)linitial(permissive_quals);
    }
    else
    {
      wco->qual = (Node *)makeBoolExpr(OR_EXPR, permissive_quals, -1);
    }

    ChangeVarNodes(wco->qual, 1, rt_index, 0);

    *withCheckOptions = list_append_unique(*withCheckOptions, wco);

       
                                                                           
                                                                       
                                                                       
                                                                       
       
    foreach (item, restrictive_policies)
    {
      RowSecurityPolicy *policy = (RowSecurityPolicy *)lfirst(item);
      Expr *qual = QUAL_FOR_WCO(policy);
      WithCheckOption *wco;

      if (qual != NULL)
      {
        qual = copyObject(qual);
        ChangeVarNodes((Node *)qual, 1, rt_index, 0);

        wco = makeNode(WithCheckOption);
        wco->kind = kind;
        wco->relname = pstrdup(RelationGetRelationName(rel));
        wco->polname = pstrdup(policy->policy_name);
        wco->qual = (Node *)qual;
        wco->cascaded = false;

        *withCheckOptions = list_append_unique(*withCheckOptions, wco);
        *hasSubLinks |= policy->hassublinks;
      }
    }
  }
  else
  {
       
                                                                       
                                                 
       
    WithCheckOption *wco;

    wco = makeNode(WithCheckOption);
    wco->kind = kind;
    wco->relname = pstrdup(RelationGetRelationName(rel));
    wco->polname = NULL;
    wco->qual = (Node *)makeConst(BOOLOID, -1, InvalidOid, sizeof(bool), BoolGetDatum(false), false, true);
    wco->cascaded = false;

    *withCheckOptions = lappend(*withCheckOptions, wco);
  }
}

   
                           
                                                                    
   
static bool
check_role_for_policy(ArrayType *policy_roles, Oid user_id)
{
  int i;
  Oid *roles = (Oid *)ARR_DATA_PTR(policy_roles);

                                                         
  if (roles[0] == ACL_ID_PUBLIC)
  {
    return true;
  }

  for (i = 0; i < ARR_DIMS(policy_roles)[0]; i++)
  {
    if (has_privs_of_role(user_id, roles[i]))
    {
      return true;
    }
  }

  return false;
}
