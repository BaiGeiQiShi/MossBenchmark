                                                                            
   
            
                                         
   
                                                                         
                                                                        
   
                                 
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup.h"
#include "access/htup_details.h"
#include "access/relation.h"
#include "access/table.h"
#include "access/sysattr.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_policy.h"
#include "catalog/pg_type.h"
#include "commands/policy.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/pg_list.h"
#include "parser/parse_clause.h"
#include "parser/parse_collate.h"
#include "parser/parse_node.h"
#include "parser/parse_relation.h"
#include "rewrite/rewriteManip.h"
#include "rewrite/rowsecurity.h"
#include "storage/lock.h"
#include "utils/acl.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static void
RangeVarCallbackForPolicy(const RangeVar *rv, Oid relid, Oid oldrelid, void *arg);
static char
parse_policy_command(const char *cmd_name);
static Datum *
policy_role_list_to_array(List *roles, int *num_roles);

   
                                           
   
                         
                                        
                                  
                                      
   
                                                         
   
static void
RangeVarCallbackForPolicy(const RangeVar *rv, Oid relid, Oid oldrelid, void *arg)
{
  HeapTuple tuple;
  Form_pg_class classform;
  char relkind;

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tuple))
  {
    return;
  }

  classform = (Form_pg_class)GETSTRUCT(tuple);
  relkind = classform->relkind;

                          
  if (!pg_class_ownercheck(relid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(get_rel_relkind(relid)), rv->relname);
  }

                                                                
  if (!allowSystemTableMods && IsSystemClass(relid, classform))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied: \"%s\" is a system catalog", rv->relname)));
  }

                                      
  if (relkind != RELKIND_RELATION && relkind != RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table", rv->relname)));
  }

  ReleaseSysCache(tuple);
}

   
                          
                                                                  
                    
   
                                                                          
                                        
   
   
static char
parse_policy_command(const char *cmd_name)
{
  char polcmd;

  if (!cmd_name)
  {
    elog(ERROR, "unrecognized policy command");
  }

  if (strcmp(cmd_name, "all") == 0)
  {
    polcmd = '*';
  }
  else if (strcmp(cmd_name, "select") == 0)
  {
    polcmd = ACL_SELECT_CHR;
  }
  else if (strcmp(cmd_name, "insert") == 0)
  {
    polcmd = ACL_INSERT_CHR;
  }
  else if (strcmp(cmd_name, "update") == 0)
  {
    polcmd = ACL_UPDATE_CHR;
  }
  else if (strcmp(cmd_name, "delete") == 0)
  {
    polcmd = ACL_DELETE_CHR;
  }
  else
  {
    elog(ERROR, "unrecognized policy command");
  }

  return polcmd;
}

   
                             
                                                                  
                    
   
static Datum *
policy_role_list_to_array(List *roles, int *num_roles)
{
  Datum *role_oids;
  ListCell *cell;
  int i = 0;

                                                           
  if (roles == NIL)
  {
    *num_roles = 1;
    role_oids = (Datum *)palloc(*num_roles * sizeof(Datum));
    role_oids[0] = ObjectIdGetDatum(ACL_ID_PUBLIC);

    return role_oids;
  }

  *num_roles = list_length(roles);
  role_oids = (Datum *)palloc(*num_roles * sizeof(Datum));

  foreach (cell, roles)
  {
    RoleSpec *spec = lfirst(cell);

       
                                                              
       
    if (spec->roletype == ROLESPEC_PUBLIC)
    {
      if (*num_roles != 1)
      {
        ereport(WARNING, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("ignoring specified roles other than PUBLIC"), errhint("All roles are members of the PUBLIC role.")));
        *num_roles = 1;
      }
      role_oids[0] = ObjectIdGetDatum(ACL_ID_PUBLIC);

      return role_oids;
    }
    else
    {
      role_oids[i++] = ObjectIdGetDatum(get_rolespec_oid(spec, false));
    }
  }

  return role_oids;
}

   
                                                              
                                  
   
                                                                      
                              
   
void
RelationBuildRowSecurity(Relation relation)
{
  MemoryContext rscxt;
  MemoryContext oldcxt = CurrentMemoryContext;
  RowSecurityDesc *rsdesc;
  Relation catalog;
  ScanKeyData skey;
  SysScanDesc sscan;
  HeapTuple tuple;

     
                                                                     
                                                                            
                                                                      
                                                                            
     
  rscxt = AllocSetContextCreate(CurrentMemoryContext, "row security descriptor", ALLOCSET_SMALL_SIZES);
  MemoryContextCopyAndSetIdentifier(rscxt, RelationGetRelationName(relation));

  rsdesc = MemoryContextAllocZero(rscxt, sizeof(RowSecurityDesc));
  rsdesc->rscxt = rscxt;

     
                                                                        
                                                                             
                                                                          
                                                      
     
  catalog = table_open(PolicyRelationId, AccessShareLock);

  ScanKeyInit(&skey, Anum_pg_policy_polrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(relation)));

  sscan = systable_beginscan(catalog, PolicyPolrelidPolnameIndexId, true, NULL, 1, &skey);

  while (HeapTupleIsValid(tuple = systable_getnext(sscan)))
  {
    Form_pg_policy policy_form = (Form_pg_policy)GETSTRUCT(tuple);
    RowSecurityPolicy *policy;
    Datum datum;
    bool isnull;
    char *str_value;

    policy = MemoryContextAllocZero(rscxt, sizeof(RowSecurityPolicy));

       
                                                                          
                                                                          
                           
       

                            
    policy->polcmd = policy_form->polcmd;

                                               
    policy->permissive = policy_form->polpermissive;

                         
    policy->policy_name = MemoryContextStrdup(rscxt, NameStr(policy_form->polname));

                          
    datum = heap_getattr(tuple, Anum_pg_policy_polroles, RelationGetDescr(catalog), &isnull);
                                                     
    if (isnull)
    {
      elog(ERROR, "unexpected null value in pg_policy.polroles");
    }
    MemoryContextSwitchTo(rscxt);
    policy->roles = DatumGetArrayTypePCopy(datum);
    MemoryContextSwitchTo(oldcxt);

                         
    datum = heap_getattr(tuple, Anum_pg_policy_polqual, RelationGetDescr(catalog), &isnull);
    if (!isnull)
    {
      str_value = TextDatumGetCString(datum);
      MemoryContextSwitchTo(rscxt);
      policy->qual = (Expr *)stringToNode(str_value);
      MemoryContextSwitchTo(oldcxt);
      pfree(str_value);
    }
    else
    {
      policy->qual = NULL;
    }

                             
    datum = heap_getattr(tuple, Anum_pg_policy_polwithcheck, RelationGetDescr(catalog), &isnull);
    if (!isnull)
    {
      str_value = TextDatumGetCString(datum);
      MemoryContextSwitchTo(rscxt);
      policy->with_check_qual = (Expr *)stringToNode(str_value);
      MemoryContextSwitchTo(oldcxt);
      pfree(str_value);
    }
    else
    {
      policy->with_check_qual = NULL;
    }

                                                                          
    policy->hassublinks = checkExprHasSubLink((Node *)policy->qual) || checkExprHasSubLink((Node *)policy->with_check_qual);

       
                                                                           
                         
       
    MemoryContextSwitchTo(rscxt);
    rsdesc->policies = lcons(policy, rsdesc->policies);
    MemoryContextSwitchTo(oldcxt);
  }

  systable_endscan(sscan);
  table_close(catalog, AccessShareLock);

     
                                                              
                                                                           
                                              
     
  MemoryContextSetParent(rscxt, CacheMemoryContext);

  relation->rd_rsdesc = rsdesc;
}

   
                      
                                                                              
                                  
   
                                      
   
void
RemovePolicyById(Oid policy_id)
{
  Relation pg_policy_rel;
  SysScanDesc sscan;
  ScanKeyData skey[1];
  HeapTuple tuple;
  Oid relid;
  Relation rel;

  pg_policy_rel = table_open(PolicyRelationId, RowExclusiveLock);

     
                                
     
  ScanKeyInit(&skey[0], Anum_pg_policy_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(policy_id));

  sscan = systable_beginscan(pg_policy_rel, PolicyOidIndexId, true, NULL, 1, skey);

  tuple = systable_getnext(sscan);

                                                                       
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "could not find tuple for policy %u", policy_id);
  }

     
                                                                           
                                                                           
                                                                         
                   
     
  relid = ((Form_pg_policy)GETSTRUCT(tuple))->polrelid;

  rel = table_open(relid, AccessExclusiveLock);
  if (rel->rd_rel->relkind != RELKIND_RELATION && rel->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table", RelationGetRelationName(rel))));
  }

  if (!allowSystemTableMods && IsSystemRelation(rel))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied: \"%s\" is a system catalog", RelationGetRelationName(rel))));
  }

  CatalogTupleDelete(pg_policy_rel, &tuple->t_self);

  systable_endscan(sscan);

     
                                                                           
                                                                             
                                                                      
                                                                           
                                                                             
                 
     
  CacheInvalidateRelcache(rel);

  table_close(rel, NoLock);

                
  table_close(pg_policy_rel, RowExclusiveLock);
}

   
                                
                                                         
   
                                                                      
                                                                        
                                                                          
                                                                          
   
                                          
                                               
                                      
   
bool
RemoveRoleFromObjectPolicy(Oid roleid, Oid classid, Oid policy_id)
{
  Relation pg_policy_rel;
  SysScanDesc sscan;
  ScanKeyData skey[1];
  HeapTuple tuple;
  Oid relid;
  ArrayType *policy_roles;
  Datum roles_datum;
  Oid *roles;
  int num_roles;
  Datum *role_oids;
  bool attr_isnull;
  bool keep_policy = true;
  int i, j;

  Assert(classid == PolicyRelationId);

  pg_policy_rel = table_open(PolicyRelationId, RowExclusiveLock);

     
                                
     
  ScanKeyInit(&skey[0], Anum_pg_policy_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(policy_id));

  sscan = systable_beginscan(pg_policy_rel, PolicyOidIndexId, true, NULL, 1, skey);

  tuple = systable_getnext(sscan);

                                                   
  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "could not find tuple for policy %u", policy_id);
  }

                                          
  relid = ((Form_pg_policy)GETSTRUCT(tuple))->polrelid;

                                    
  roles_datum = heap_getattr(tuple, Anum_pg_policy_polroles, RelationGetDescr(pg_policy_rel), &attr_isnull);

  Assert(!attr_isnull);

  policy_roles = DatumGetArrayTypePCopy(roles_datum);
  roles = (Oid *)ARR_DATA_PTR(policy_roles);
  num_roles = ARR_DIMS(policy_roles)[0];

     
                                                                          
                                                                        
                                                                         
     
  role_oids = (Datum *)palloc(num_roles * sizeof(Datum));
  for (i = 0, j = 0; i < num_roles; i++)
  {
    if (roles[i] != roleid)
    {
      role_oids[j++] = ObjectIdGetDatum(roles[i]);
    }
  }
  num_roles = j;

                                                     
  if (num_roles > 0)
  {
    ArrayType *role_ids;
    Datum values[Natts_pg_policy];
    bool isnull[Natts_pg_policy];
    bool replaces[Natts_pg_policy];
    HeapTuple new_tuple;
    HeapTuple reltup;
    ObjectAddress target;
    ObjectAddress myself;

                    
    memset(values, 0, sizeof(values));
    memset(replaces, 0, sizeof(replaces));
    memset(isnull, 0, sizeof(isnull));

                                             
    role_ids = construct_array(role_oids, num_roles, OIDOID, sizeof(Oid), true, 'i');

    replaces[Anum_pg_policy_polroles - 1] = true;
    values[Anum_pg_policy_polroles - 1] = PointerGetDatum(role_ids);

    new_tuple = heap_modify_tuple(tuple, RelationGetDescr(pg_policy_rel), values, isnull, replaces);
    CatalogTupleUpdate(pg_policy_rel, &new_tuple->t_self, new_tuple);

                                                        
    deleteSharedDependencyRecordsFor(PolicyRelationId, policy_id, 0);

                                                    
    myself.classId = PolicyRelationId;
    myself.objectId = policy_id;
    myself.objectSubId = 0;

    target.classId = AuthIdRelationId;
    target.objectSubId = 0;
    for (i = 0; i < num_roles; i++)
    {
      target.objectId = DatumGetObjectId(role_oids[i]);
                                                     
      if (target.objectId != ACL_ID_PUBLIC)
      {
        recordSharedDependencyOn(&myself, &target, SHARED_DEPENDENCY_POLICY);
      }
    }

    InvokeObjectPostAlterHook(PolicyRelationId, policy_id, 0);

    heap_freetuple(new_tuple);

                              
    CommandCounterIncrement();

       
                                                                         
                                                                           
                                                 
       
    reltup = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
    if (HeapTupleIsValid(reltup))
    {
      CacheInvalidateRelcacheByTuple(reltup);
      ReleaseSysCache(reltup);
    }
  }
  else
  {
                                                            
    keep_policy = false;
  }

                 
  systable_endscan(sscan);

  table_close(pg_policy_rel, RowExclusiveLock);

  return keep_policy;
}

   
                  
                                                        
   
                                                                    
   
ObjectAddress
CreatePolicy(CreatePolicyStmt *stmt)
{
  Relation pg_policy_rel;
  Oid policy_id;
  Relation target_table;
  Oid table_id;
  char polcmd;
  Datum *role_oids;
  int nitems = 0;
  ArrayType *role_ids;
  ParseState *qual_pstate;
  ParseState *with_check_pstate;
  RangeTblEntry *rte;
  Node *qual;
  Node *with_check_qual;
  ScanKeyData skey[2];
  SysScanDesc sscan;
  HeapTuple policy_tuple;
  Datum values[Natts_pg_policy];
  bool isnull[Natts_pg_policy];
  ObjectAddress target;
  ObjectAddress myself;
  int i;

                     
  polcmd = parse_policy_command(stmt->cmd_name);

     
                                                                        
     
  if ((polcmd == ACL_SELECT_CHR || polcmd == ACL_DELETE_CHR) && stmt->with_check != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("WITH CHECK cannot be applied to SELECT or DELETE")));
  }

     
                                                                            
               
     
  if (polcmd == ACL_INSERT_CHR && stmt->qual != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("only WITH CHECK expression allowed for INSERT")));
  }

                        
  role_oids = policy_role_list_to_array(stmt->roles, &nitems);
  role_ids = construct_array(role_oids, nitems, OIDOID, sizeof(Oid), true, 'i');

                                 
  qual_pstate = make_parsestate(NULL);
  with_check_pstate = make_parsestate(NULL);

                  
  memset(values, 0, sizeof(values));
  memset(isnull, 0, sizeof(isnull));

                                                          
  table_id = RangeVarGetRelidExtended(stmt->table, AccessExclusiveLock, 0, RangeVarCallbackForPolicy, (void *)stmt);

                                                                          
  target_table = relation_open(table_id, NoLock);

                                          
  rte = addRangeTableEntryForRelation(qual_pstate, target_table, AccessShareLock, NULL, false, false);
  addRTEtoQuery(qual_pstate, rte, false, true, true);

                                    
  rte = addRangeTableEntryForRelation(with_check_pstate, target_table, AccessShareLock, NULL, false, false);
  addRTEtoQuery(with_check_pstate, rte, false, true, true);

  qual = transformWhereClause(qual_pstate, copyObject(stmt->qual), EXPR_KIND_POLICY, "POLICY");

  with_check_qual = transformWhereClause(with_check_pstate, copyObject(stmt->with_check), EXPR_KIND_POLICY, "POLICY");

                                    
  assign_expr_collations(qual_pstate, qual);
  assign_expr_collations(with_check_pstate, with_check_qual);

                              
  pg_policy_rel = table_open(PolicyRelationId, RowExclusiveLock);

                                       
  ScanKeyInit(&skey[0], Anum_pg_policy_polrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(table_id));

                                
  ScanKeyInit(&skey[1], Anum_pg_policy_polname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(stmt->policy_name));

  sscan = systable_beginscan(pg_policy_rel, PolicyPolrelidPolnameIndexId, true, NULL, 2, skey);

  policy_tuple = systable_getnext(sscan);

                                                                
  if (HeapTupleIsValid(policy_tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("policy \"%s\" for table \"%s\" already exists", stmt->policy_name, RelationGetRelationName(target_table))));
  }

  policy_id = GetNewOidWithIndex(pg_policy_rel, PolicyOidIndexId, Anum_pg_policy_oid);
  values[Anum_pg_policy_oid - 1] = ObjectIdGetDatum(policy_id);
  values[Anum_pg_policy_polrelid - 1] = ObjectIdGetDatum(table_id);
  values[Anum_pg_policy_polname - 1] = DirectFunctionCall1(namein, CStringGetDatum(stmt->policy_name));
  values[Anum_pg_policy_polcmd - 1] = CharGetDatum(polcmd);
  values[Anum_pg_policy_polpermissive - 1] = BoolGetDatum(stmt->permissive);
  values[Anum_pg_policy_polroles - 1] = PointerGetDatum(role_ids);

                            
  if (qual)
  {
    values[Anum_pg_policy_polqual - 1] = CStringGetTextDatum(nodeToString(qual));
  }
  else
  {
    isnull[Anum_pg_policy_polqual - 1] = true;
  }

                                      
  if (with_check_qual)
  {
    values[Anum_pg_policy_polwithcheck - 1] = CStringGetTextDatum(nodeToString(with_check_qual));
  }
  else
  {
    isnull[Anum_pg_policy_polwithcheck - 1] = true;
  }

  policy_tuple = heap_form_tuple(RelationGetDescr(pg_policy_rel), values, isnull);

  CatalogTupleInsert(pg_policy_rel, policy_tuple);

                           
  target.classId = RelationRelationId;
  target.objectId = table_id;
  target.objectSubId = 0;

  myself.classId = PolicyRelationId;
  myself.objectId = policy_id;
  myself.objectSubId = 0;

  recordDependencyOn(&myself, &target, DEPENDENCY_AUTO);

  recordDependencyOnExpr(&myself, qual, qual_pstate->p_rtable, DEPENDENCY_NORMAL);

  recordDependencyOnExpr(&myself, with_check_qual, with_check_pstate->p_rtable, DEPENDENCY_NORMAL);

                                  
  target.classId = AuthIdRelationId;
  target.objectSubId = 0;
  for (i = 0; i < nitems; i++)
  {
    target.objectId = DatumGetObjectId(role_oids[i]);
                                 
    if (target.objectId != ACL_ID_PUBLIC)
    {
      recordSharedDependencyOn(&myself, &target, SHARED_DEPENDENCY_POLICY);
    }
  }

  InvokeObjectPostCreateHook(PolicyRelationId, policy_id, 0);

                                 
  CacheInvalidateRelcache(target_table);

                 
  heap_freetuple(policy_tuple);
  free_parsestate(qual_pstate);
  free_parsestate(with_check_pstate);
  systable_endscan(sscan);
  relation_close(target_table, NoLock);
  table_close(pg_policy_rel, RowExclusiveLock);

  return myself;
}

   
                 
                                                       
   
                                                                             
   
ObjectAddress
AlterPolicy(AlterPolicyStmt *stmt)
{
  Relation pg_policy_rel;
  Oid policy_id;
  Relation target_table;
  Oid table_id;
  Datum *role_oids = NULL;
  int nitems = 0;
  ArrayType *role_ids = NULL;
  List *qual_parse_rtable = NIL;
  List *with_check_parse_rtable = NIL;
  Node *qual = NULL;
  Node *with_check_qual = NULL;
  ScanKeyData skey[2];
  SysScanDesc sscan;
  HeapTuple policy_tuple;
  HeapTuple new_tuple;
  Datum values[Natts_pg_policy];
  bool isnull[Natts_pg_policy];
  bool replaces[Natts_pg_policy];
  ObjectAddress target;
  ObjectAddress myself;
  Datum polcmd_datum;
  char polcmd;
  bool polcmd_isnull;
  int i;

                      
  if (stmt->roles != NULL)
  {
    role_oids = policy_role_list_to_array(stmt->roles, &nitems);
    role_ids = construct_array(role_oids, nitems, OIDOID, sizeof(Oid), true, 'i');
  }

                                                          
  table_id = RangeVarGetRelidExtended(stmt->table, AccessExclusiveLock, 0, RangeVarCallbackForPolicy, (void *)stmt);

  target_table = relation_open(table_id, NoLock);

                                     
  if (stmt->qual)
  {
    RangeTblEntry *rte;
    ParseState *qual_pstate = make_parsestate(NULL);

    rte = addRangeTableEntryForRelation(qual_pstate, target_table, AccessShareLock, NULL, false, false);

    addRTEtoQuery(qual_pstate, rte, false, true, true);

    qual = transformWhereClause(qual_pstate, copyObject(stmt->qual), EXPR_KIND_POLICY, "POLICY");

                                      
    assign_expr_collations(qual_pstate, qual);

    qual_parse_rtable = qual_pstate->p_rtable;
    free_parsestate(qual_pstate);
  }

                                          
  if (stmt->with_check)
  {
    RangeTblEntry *rte;
    ParseState *with_check_pstate = make_parsestate(NULL);

    rte = addRangeTableEntryForRelation(with_check_pstate, target_table, AccessShareLock, NULL, false, false);

    addRTEtoQuery(with_check_pstate, rte, false, true, true);

    with_check_qual = transformWhereClause(with_check_pstate, copyObject(stmt->with_check), EXPR_KIND_POLICY, "POLICY");

                                      
    assign_expr_collations(with_check_pstate, with_check_qual);

    with_check_parse_rtable = with_check_pstate->p_rtable;
    free_parsestate(with_check_pstate);
  }

                  
  memset(values, 0, sizeof(values));
  memset(replaces, 0, sizeof(replaces));
  memset(isnull, 0, sizeof(isnull));

                              
  pg_policy_rel = table_open(PolicyRelationId, RowExclusiveLock);

                                       
  ScanKeyInit(&skey[0], Anum_pg_policy_polrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(table_id));

                                
  ScanKeyInit(&skey[1], Anum_pg_policy_polname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(stmt->policy_name));

  sscan = systable_beginscan(pg_policy_rel, PolicyPolrelidPolnameIndexId, true, NULL, 2, skey);

  policy_tuple = systable_getnext(sscan);

                                                              
  if (!HeapTupleIsValid(policy_tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("policy \"%s\" for table \"%s\" does not exist", stmt->policy_name, RelationGetRelationName(target_table))));
  }

                          
  polcmd_datum = heap_getattr(policy_tuple, Anum_pg_policy_polcmd, RelationGetDescr(pg_policy_rel), &polcmd_isnull);
  Assert(!polcmd_isnull);
  polcmd = DatumGetChar(polcmd_datum);

     
                                                                        
     
  if ((polcmd == ACL_SELECT_CHR || polcmd == ACL_DELETE_CHR) && stmt->with_check != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("only USING expression allowed for SELECT, DELETE")));
  }

     
                                                                            
               
     
  if ((polcmd == ACL_INSERT_CHR) && stmt->qual != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("only WITH CHECK expression allowed for INSERT")));
  }

  policy_id = ((Form_pg_policy)GETSTRUCT(policy_tuple))->oid;

  if (role_ids != NULL)
  {
    replaces[Anum_pg_policy_polroles - 1] = true;
    values[Anum_pg_policy_polroles - 1] = PointerGetDatum(role_ids);
  }
  else
  {
    Oid *roles;
    Datum roles_datum;
    bool attr_isnull;
    ArrayType *policy_roles;

       
                                                                           
                                                                          
                       
       

    roles_datum = heap_getattr(policy_tuple, Anum_pg_policy_polroles, RelationGetDescr(pg_policy_rel), &attr_isnull);
    Assert(!attr_isnull);

    policy_roles = DatumGetArrayTypePCopy(roles_datum);

    roles = (Oid *)ARR_DATA_PTR(policy_roles);

    nitems = ARR_DIMS(policy_roles)[0];

    role_oids = (Datum *)palloc(nitems * sizeof(Datum));

    for (i = 0; i < nitems; i++)
    {
      role_oids[i] = ObjectIdGetDatum(roles[i]);
    }
  }

  if (qual != NULL)
  {
    replaces[Anum_pg_policy_polqual - 1] = true;
    values[Anum_pg_policy_polqual - 1] = CStringGetTextDatum(nodeToString(qual));
  }
  else
  {
    Datum value_datum;
    bool attr_isnull;

       
                                                                          
                                                                          
                                              
       

                                              
    value_datum = heap_getattr(policy_tuple, Anum_pg_policy_polqual, RelationGetDescr(pg_policy_rel), &attr_isnull);
    if (!attr_isnull)
    {
      char *qual_value;
      ParseState *qual_pstate;

                                                             
      qual_pstate = make_parsestate(NULL);

      qual_value = TextDatumGetCString(value_datum);
      qual = stringToNode(qual_value);

                                                                         
      addRangeTableEntryForRelation(qual_pstate, target_table, AccessShareLock, NULL, false, false);

      qual_parse_rtable = qual_pstate->p_rtable;
      free_parsestate(qual_pstate);
    }
  }

  if (with_check_qual != NULL)
  {
    replaces[Anum_pg_policy_polwithcheck - 1] = true;
    values[Anum_pg_policy_polwithcheck - 1] = CStringGetTextDatum(nodeToString(with_check_qual));
  }
  else
  {
    Datum value_datum;
    bool attr_isnull;

       
                                                                           
                                                                          
                                                  
       

                                                   
    value_datum = heap_getattr(policy_tuple, Anum_pg_policy_polwithcheck, RelationGetDescr(pg_policy_rel), &attr_isnull);
    if (!attr_isnull)
    {
      char *with_check_value;
      ParseState *with_check_pstate;

                                                             
      with_check_pstate = make_parsestate(NULL);

      with_check_value = TextDatumGetCString(value_datum);
      with_check_qual = stringToNode(with_check_value);

                                                                         
      addRangeTableEntryForRelation(with_check_pstate, target_table, AccessShareLock, NULL, false, false);

      with_check_parse_rtable = with_check_pstate->p_rtable;
      free_parsestate(with_check_pstate);
    }
  }

  new_tuple = heap_modify_tuple(policy_tuple, RelationGetDescr(pg_policy_rel), values, isnull, replaces);
  CatalogTupleUpdate(pg_policy_rel, &new_tuple->t_self, new_tuple);

                            
  deleteDependencyRecordsFor(PolicyRelationId, policy_id, false);

                           
  target.classId = RelationRelationId;
  target.objectId = table_id;
  target.objectSubId = 0;

  myself.classId = PolicyRelationId;
  myself.objectId = policy_id;
  myself.objectSubId = 0;

  recordDependencyOn(&myself, &target, DEPENDENCY_AUTO);

  recordDependencyOnExpr(&myself, qual, qual_parse_rtable, DEPENDENCY_NORMAL);

  recordDependencyOnExpr(&myself, with_check_qual, with_check_parse_rtable, DEPENDENCY_NORMAL);

                                  
  deleteSharedDependencyRecordsFor(PolicyRelationId, policy_id, 0);
  target.classId = AuthIdRelationId;
  target.objectSubId = 0;
  for (i = 0; i < nitems; i++)
  {
    target.objectId = DatumGetObjectId(role_oids[i]);
                                 
    if (target.objectId != ACL_ID_PUBLIC)
    {
      recordSharedDependencyOn(&myself, &target, SHARED_DEPENDENCY_POLICY);
    }
  }

  InvokeObjectPostAlterHook(PolicyRelationId, policy_id, 0);

  heap_freetuple(new_tuple);

                                 
  CacheInvalidateRelcache(target_table);

                 
  systable_endscan(sscan);
  relation_close(target_table, NoLock);
  table_close(pg_policy_rel, RowExclusiveLock);

  return myself;
}

   
                   
                                              
   
ObjectAddress
rename_policy(RenameStmt *stmt)
{
  Relation pg_policy_rel;
  Relation target_table;
  Oid table_id;
  Oid opoloid;
  ScanKeyData skey[2];
  SysScanDesc sscan;
  HeapTuple policy_tuple;
  ObjectAddress address;

                                                          
  table_id = RangeVarGetRelidExtended(stmt->relation, AccessExclusiveLock, 0, RangeVarCallbackForPolicy, (void *)stmt);

  target_table = relation_open(table_id, NoLock);

  pg_policy_rel = table_open(PolicyRelationId, RowExclusiveLock);

                                        

                                       
  ScanKeyInit(&skey[0], Anum_pg_policy_polrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(table_id));

                                
  ScanKeyInit(&skey[1], Anum_pg_policy_polname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(stmt->newname));

  sscan = systable_beginscan(pg_policy_rel, PolicyPolrelidPolnameIndexId, true, NULL, 2, skey);

  if (HeapTupleIsValid(systable_getnext(sscan)))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("policy \"%s\" for table \"%s\" already exists", stmt->newname, RelationGetRelationName(target_table))));
  }

  systable_endscan(sscan);

                                                      
                                       
  ScanKeyInit(&skey[0], Anum_pg_policy_polrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(table_id));

                                
  ScanKeyInit(&skey[1], Anum_pg_policy_polname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(stmt->subname));

  sscan = systable_beginscan(pg_policy_rel, PolicyPolrelidPolnameIndexId, true, NULL, 2, skey);

  policy_tuple = systable_getnext(sscan);

                                              
  if (!HeapTupleIsValid(policy_tuple))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("policy \"%s\" for table \"%s\" does not exist", stmt->subname, RelationGetRelationName(target_table))));
  }

  opoloid = ((Form_pg_policy)GETSTRUCT(policy_tuple))->oid;

  policy_tuple = heap_copytuple(policy_tuple);

  namestrcpy(&((Form_pg_policy)GETSTRUCT(policy_tuple))->polname, stmt->newname);

  CatalogTupleUpdate(pg_policy_rel, &policy_tuple->t_self, policy_tuple);

  InvokeObjectPostAlterHook(PolicyRelationId, opoloid, 0);

  ObjectAddressSet(address, PolicyRelationId, opoloid);

     
                                                                           
                                                                          
                                                   
     
  CacheInvalidateRelcache(target_table);

                 
  systable_endscan(sscan);
  table_close(pg_policy_rel, RowExclusiveLock);
  relation_close(target_table, NoLock);

  return address;
}

   
                                                                      
   
                                                                   
                                 
   
Oid
get_relation_policy_oid(Oid relid, const char *policy_name, bool missing_ok)
{
  Relation pg_policy_rel;
  ScanKeyData skey[2];
  SysScanDesc sscan;
  HeapTuple policy_tuple;
  Oid policy_oid;

  pg_policy_rel = table_open(PolicyRelationId, AccessShareLock);

                                       
  ScanKeyInit(&skey[0], Anum_pg_policy_polrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));

                                
  ScanKeyInit(&skey[1], Anum_pg_policy_polname, BTEqualStrategyNumber, F_NAMEEQ, CStringGetDatum(policy_name));

  sscan = systable_beginscan(pg_policy_rel, PolicyPolrelidPolnameIndexId, true, NULL, 2, skey);

  policy_tuple = systable_getnext(sscan);

  if (!HeapTupleIsValid(policy_tuple))
  {
    if (!missing_ok)
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("policy \"%s\" for table \"%s\" does not exist", policy_name, get_rel_name(relid))));
    }

    policy_oid = InvalidOid;
  }
  else
  {
    policy_oid = ((Form_pg_policy)GETSTRUCT(policy_tuple))->oid;
  }

                 
  systable_endscan(sscan);
  table_close(pg_policy_rel, AccessShareLock);

  return policy_oid;
}

   
                                                                  
   
bool
relation_has_policies(Relation rel)
{
  Relation catalog;
  ScanKeyData skey;
  SysScanDesc sscan;
  HeapTuple policy_tuple;
  bool ret = false;

  catalog = table_open(PolicyRelationId, AccessShareLock);
  ScanKeyInit(&skey, Anum_pg_policy_polrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(RelationGetRelid(rel)));
  sscan = systable_beginscan(catalog, PolicyPolrelidPolnameIndexId, true, NULL, 1, &skey);
  policy_tuple = systable_getnext(sscan);
  if (HeapTupleIsValid(policy_tuple))
  {
    ret = true;
  }

  systable_endscan(sscan);
  table_close(catalog, AccessShareLock);

  return ret;
}
