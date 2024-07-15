                                                                             
   
                    
                                
   
                                                                
   
                  
                                                     
   
                                                                             
   

#include "postgres.h"

#include "fmgr.h"
#include "miscadmin.h"

#include "test_rls_hooks.h"

#include "catalog/pg_type.h"
#include "nodes/makefuncs.h"
#include "nodes/makefuncs.h"
#include "parser/parse_clause.h"
#include "parser/parse_collate.h"
#include "parser/parse_node.h"
#include "parser/parse_relation.h"
#include "rewrite/rowsecurity.h"
#include "utils/acl.h"
#include "utils/rel.h"
#include "utils/relcache.h"

PG_MODULE_MAGIC;

                                         
static row_security_policy_hook_type prev_row_security_policy_hook_permissive = NULL;
static row_security_policy_hook_type prev_row_security_policy_hook_restrictive = NULL;

void
_PG_init(void);
void
_PG_fini(void);

                   
void
_PG_init(void)
{
                               
  prev_row_security_policy_hook_permissive = row_security_policy_hook_permissive;
  prev_row_security_policy_hook_restrictive = row_security_policy_hook_restrictive;

                     
  row_security_policy_hook_permissive = test_rls_hooks_permissive;
  row_security_policy_hook_restrictive = test_rls_hooks_restrictive;
}

                     
void
_PG_fini(void)
{
  row_security_policy_hook_permissive = prev_row_security_policy_hook_permissive;
  row_security_policy_hook_restrictive = prev_row_security_policy_hook_restrictive;
}

   
                                          
   
List *
test_rls_hooks_permissive(CmdType cmdtype, Relation relation)
{
  List *policies = NIL;
  RowSecurityPolicy *policy = palloc0(sizeof(RowSecurityPolicy));
  Datum role;
  FuncCall *n;
  Node *e;
  ColumnRef *c;
  ParseState *qual_pstate;
  RangeTblEntry *rte;

  if (strcmp(RelationGetRelationName(relation), "rls_test_permissive") != 0 && strcmp(RelationGetRelationName(relation), "rls_test_both") != 0)
  {
    return NIL;
  }

  qual_pstate = make_parsestate(NULL);

  rte = addRangeTableEntryForRelation(qual_pstate, relation, AccessShareLock, NULL, false, false);
  addRTEtoQuery(qual_pstate, rte, false, true, true);

  role = ObjectIdGetDatum(ACL_ID_PUBLIC);

  policy->policy_name = pstrdup("extension policy");
  policy->polcmd = '*';
  policy->roles = construct_array(&role, 1, OIDOID, sizeof(Oid), true, 'i');

     
                                                                
                                                     
     

  n = makeFuncCall(list_make2(makeString("pg_catalog"), makeString("current_user")), NIL, 0);

  c = makeNode(ColumnRef);
  c->fields = list_make1(makeString("username"));
  c->location = 0;

  e = (Node *)makeSimpleA_Expr(AEXPR_OP, "=", (Node *)n, (Node *)c, 0);

  policy->qual = (Expr *)transformWhereClause(qual_pstate, copyObject(e), EXPR_KIND_POLICY, "POLICY");
                                    
  assign_expr_collations(qual_pstate, (Node *)policy->qual);

  policy->with_check_qual = copyObject(policy->qual);
  policy->hassublinks = false;

  policies = list_make1(policy);

  return policies;
}

   
                                           
   
                                                                       
                                                                         
                                                                           
                                                            
   
List *
test_rls_hooks_restrictive(CmdType cmdtype, Relation relation)
{
  List *policies = NIL;
  RowSecurityPolicy *policy = palloc0(sizeof(RowSecurityPolicy));
  Datum role;
  FuncCall *n;
  Node *e;
  ColumnRef *c;
  ParseState *qual_pstate;
  RangeTblEntry *rte;

  if (strcmp(RelationGetRelationName(relation), "rls_test_restrictive") != 0 && strcmp(RelationGetRelationName(relation), "rls_test_both") != 0)
  {
    return NIL;
  }

  qual_pstate = make_parsestate(NULL);

  rte = addRangeTableEntryForRelation(qual_pstate, relation, AccessShareLock, NULL, false, false);
  addRTEtoQuery(qual_pstate, rte, false, true, true);

  role = ObjectIdGetDatum(ACL_ID_PUBLIC);

  policy->policy_name = pstrdup("extension policy");
  policy->polcmd = '*';
  policy->roles = construct_array(&role, 1, OIDOID, sizeof(Oid), true, 'i');

  n = makeFuncCall(list_make2(makeString("pg_catalog"), makeString("current_user")), NIL, 0);

  c = makeNode(ColumnRef);
  c->fields = list_make1(makeString("supervisor"));
  c->location = 0;

  e = (Node *)makeSimpleA_Expr(AEXPR_OP, "=", (Node *)n, (Node *)c, 0);

  policy->qual = (Expr *)transformWhereClause(qual_pstate, copyObject(e), EXPR_KIND_POLICY, "POLICY");
                                    
  assign_expr_collations(qual_pstate, (Node *)policy->qual);

  policy->with_check_qual = copyObject(policy->qual);
  policy->hassublinks = false;

  policies = list_make1(policy);

  return policies;
}
