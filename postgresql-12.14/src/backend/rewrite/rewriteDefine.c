                                                                            
   
                   
                                          
   
                                                                         
                                                                        
   
   
                  
                                         
   
                                                                            
   
#include "postgres.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/multixact.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_rewrite.h"
#include "catalog/storage.h"
#include "commands/policy.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_utilcmd.h"
#include "rewrite/rewriteDefine.h"
#include "rewrite/rewriteManip.h"
#include "rewrite/rewriteSupport.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

static void
checkRuleResultList(List *targetList, TupleDesc resultDesc, bool isSelect, bool requireColumnNameMatch);
static bool
setRuleCheckAsUser_walker(Node *node, Oid *context);
static void
setRuleCheckAsUser_Query(Query *qry, Oid userid);

   
                
                                                                   
                           
   
static Oid
InsertRule(const char *rulname, int evtype, Oid eventrel_oid, bool evinstead, Node *event_qual, List *action, bool replace)
{
  char *evqual = nodeToString(event_qual);
  char *actiontree = nodeToString((Node *)action);
  Datum values[Natts_pg_rewrite];
  bool nulls[Natts_pg_rewrite];
  bool replaces[Natts_pg_rewrite];
  NameData rname;
  Relation pg_rewrite_desc;
  HeapTuple tup, oldtup;
  Oid rewriteObjectId;
  ObjectAddress myself, referenced;
  bool is_update = false;

     
                                      
     
  MemSet(nulls, false, sizeof(nulls));

  namestrcpy(&rname, rulname);
  values[Anum_pg_rewrite_rulename - 1] = NameGetDatum(&rname);
  values[Anum_pg_rewrite_ev_class - 1] = ObjectIdGetDatum(eventrel_oid);
  values[Anum_pg_rewrite_ev_type - 1] = CharGetDatum(evtype + '0');
  values[Anum_pg_rewrite_ev_enabled - 1] = CharGetDatum(RULE_FIRES_ON_ORIGIN);
  values[Anum_pg_rewrite_is_instead - 1] = BoolGetDatum(evinstead);
  values[Anum_pg_rewrite_ev_qual - 1] = CStringGetTextDatum(evqual);
  values[Anum_pg_rewrite_ev_action - 1] = CStringGetTextDatum(actiontree);

     
                                         
     
  pg_rewrite_desc = table_open(RewriteRelationId, RowExclusiveLock);

     
                                                        
     
  oldtup = SearchSysCache2(RULERELNAME, ObjectIdGetDatum(eventrel_oid), PointerGetDatum(rulname));

  if (HeapTupleIsValid(oldtup))
  {
    if (!replace)
    {
      ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("rule \"%s\" for relation \"%s\" already exists", rulname, get_rel_name(eventrel_oid))));
    }

       
                                                                
       
    MemSet(replaces, false, sizeof(replaces));
    replaces[Anum_pg_rewrite_ev_type - 1] = true;
    replaces[Anum_pg_rewrite_is_instead - 1] = true;
    replaces[Anum_pg_rewrite_ev_qual - 1] = true;
    replaces[Anum_pg_rewrite_ev_action - 1] = true;

    tup = heap_modify_tuple(oldtup, RelationGetDescr(pg_rewrite_desc), values, nulls, replaces);

    CatalogTupleUpdate(pg_rewrite_desc, &tup->t_self, tup);

    ReleaseSysCache(oldtup);

    rewriteObjectId = ((Form_pg_rewrite)GETSTRUCT(tup))->oid;
    is_update = true;
  }
  else
  {
    rewriteObjectId = GetNewOidWithIndex(pg_rewrite_desc, RewriteOidIndexId, Anum_pg_rewrite_oid);
    values[Anum_pg_rewrite_oid - 1] = ObjectIdGetDatum(rewriteObjectId);

    tup = heap_form_tuple(pg_rewrite_desc->rd_att, values, nulls);

    CatalogTupleInsert(pg_rewrite_desc, tup);
  }

  heap_freetuple(tup);

                                                                   
  if (is_update)
  {
    deleteDependencyRecordsFor(RewriteRelationId, rewriteObjectId, false);
  }

     
                                                                        
                                                                       
                                                                            
                           
     
  myself.classId = RewriteRelationId;
  myself.objectId = rewriteObjectId;
  myself.objectSubId = 0;

  referenced.classId = RelationRelationId;
  referenced.objectId = eventrel_oid;
  referenced.objectSubId = 0;

  recordDependencyOn(&myself, &referenced, (evtype == CMD_SELECT) ? DEPENDENCY_INTERNAL : DEPENDENCY_AUTO);

     
                                                                         
     
  recordDependencyOnExpr(&myself, (Node *)action, NIL, DEPENDENCY_NORMAL);

  if (event_qual != NULL)
  {
                                                      
    Query *qry = linitial_node(Query, action);

    qry = getInsertSelectQuery(qry, NULL);
    recordDependencyOnExpr(&myself, event_qual, qry->rtable, DEPENDENCY_NORMAL);
  }

                                       
  InvokeObjectPostCreateHook(RewriteRelationId, rewriteObjectId, 0);

  table_close(pg_rewrite_desc, RowExclusiveLock);

  return rewriteObjectId;
}

   
              
                                   
   
ObjectAddress
DefineRule(RuleStmt *stmt, const char *queryString)
{
  List *actions;
  Node *whereClause;
  Oid relId;

                       
  transformRuleStmt(stmt, queryString, &actions, &whereClause);

     
                                                          
                         
     
  relId = RangeVarGetRelid(stmt->relation, AccessExclusiveLock, false);

                       
  return DefineQueryRewrite(stmt->rulename, relId, whereClause, stmt->event, stmt->instead, stmt->replace, actions);
}

   
                      
                  
   
                                                                       
                                                                    
   
ObjectAddress
DefineQueryRewrite(const char *rulename, Oid event_relid, Node *event_qual, CmdType event_type, bool is_instead, bool replace, List *action)
{
  Relation event_relation;
  ListCell *l;
  Query *query;
  bool RelisBecomingView = false;
  Oid ruleId = InvalidOid;
  ObjectAddress address;

     
                                                                
                                                                           
                                                                         
                                                                             
                                                                            
                                                                            
     
                                                                        
     
  event_relation = table_open(event_relid, AccessExclusiveLock);

     
                                                                         
                                                                             
                                                                      
     
  if (event_relation->rd_rel->relkind != RELKIND_RELATION && event_relation->rd_rel->relkind != RELKIND_MATVIEW && event_relation->rd_rel->relkind != RELKIND_VIEW && event_relation->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table or view", RelationGetRelationName(event_relation))));
  }

  if (!allowSystemTableMods && IsSystemRelation(event_relation))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied: \"%s\" is a system catalog", RelationGetRelationName(event_relation))));
  }

     
                                                                
     
  if (!pg_class_ownercheck(event_relid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(event_relation->rd_rel->relkind), RelationGetRelationName(event_relation));
  }

     
                                            
     
  foreach (l, action)
  {
    query = lfirst_node(Query, l);
    if (query->resultRelation == 0)
    {
      continue;
    }
                                          
    if (query != getInsertSelectQuery(query, NULL))
    {
      continue;
    }
    if (query->resultRelation == PRS2_OLD_VARNO)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("rule actions on OLD are not implemented"), errhint("Use views or triggers instead.")));
    }
    if (query->resultRelation == PRS2_NEW_VARNO)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("rule actions on NEW are not implemented"), errhint("Use triggers instead.")));
    }
  }

  if (event_type == CMD_SELECT)
  {
       
                                                          
       
                                               
       
    if (list_length(action) == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("INSTEAD NOTHING rules on SELECT are not implemented"), errhint("Use views instead.")));
    }

       
                                                 
       
    if (list_length(action) > 1)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("multiple actions for rules on SELECT are not implemented")));
    }

       
                                                
       
    query = linitial_node(Query, action);
    if (!is_instead || query->commandType != CMD_SELECT)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("rules on SELECT must have action INSTEAD SELECT")));
    }

       
                                                     
       
    if (query->hasModifyingCTE)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("rules on SELECT must not contain data-modifying statements in WITH")));
    }

       
                                          
       
    if (event_qual != NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("event qualifications are not implemented for rules on SELECT")));
    }

       
                                                                      
                           
       
    checkRuleResultList(query->targetList, RelationGetDescr(event_relation), true, event_relation->rd_rel->relkind != RELKIND_MATVIEW);

       
                                                                
       
    if (!replace && event_relation->rd_rules != NULL)
    {
      int i;

      for (i = 0; i < event_relation->rd_rules->numLocks; i++)
      {
        RewriteRule *rule;

        rule = event_relation->rd_rules->rules[i];
        if (rule->event == CMD_SELECT)
        {
          ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("\"%s\" is already a view", RelationGetRelationName(event_relation))));
        }
      }
    }

       
                                                       
       
    if (strcmp(rulename, ViewSelectRuleName) != 0)
    {
         
                                                                         
                                                                      
                                                                        
                                                                      
                                                                      
                                                                   
                    
         
      if (strncmp(rulename, "_RET", 4) != 0 || strncmp(rulename + 4, RelationGetRelationName(event_relation), NAMEDATALEN - 4 - 4) != 0)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("view rule for \"%s\" must be named \"%s\"", RelationGetRelationName(event_relation), ViewSelectRuleName)));
      }
      rulename = pstrdup(ViewSelectRuleName);
    }

       
                                               
       
                                                                           
                                                                         
                                                                          
                                                                          
                                                                          
                                                                           
                                                                           
                                                                        
                      
       
    if (event_relation->rd_rel->relkind != RELKIND_VIEW && event_relation->rd_rel->relkind != RELKIND_MATVIEW)
    {
      TableScanDesc scanDesc;
      Snapshot snapshot;
      TupleTableSlot *slot;

      if (event_relation->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot convert partitioned table \"%s\" to a view", RelationGetRelationName(event_relation))));
      }

                           
      Assert(event_relation->rd_rel->relkind == RELKIND_RELATION);

      if (event_relation->rd_rel->relispartition)
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot convert partition \"%s\" to a view", RelationGetRelationName(event_relation))));
      }

      snapshot = RegisterSnapshot(GetLatestSnapshot());
      scanDesc = table_beginscan(event_relation, snapshot, 0, NULL);
      slot = table_slot_create(event_relation, NULL);
      if (table_scan_getnextslot(scanDesc, ForwardScanDirection, slot))
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not convert table \"%s\" to a view because it is not empty", RelationGetRelationName(event_relation))));
      }
      ExecDropSingleTupleTableSlot(slot);
      table_endscan(scanDesc);
      UnregisterSnapshot(snapshot);

      if (event_relation->rd_rel->relhastriggers)
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not convert table \"%s\" to a view because it has triggers", RelationGetRelationName(event_relation)), errhint("In particular, the table cannot be involved in any foreign key relationships.")));
      }

      if (event_relation->rd_rel->relhasindex)
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not convert table \"%s\" to a view because it has indexes", RelationGetRelationName(event_relation))));
      }

      if (event_relation->rd_rel->relhassubclass)
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not convert table \"%s\" to a view because it has child tables", RelationGetRelationName(event_relation))));
      }

      if (has_superclass(RelationGetRelid(event_relation)))
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not convert table \"%s\" to a view because it has parent tables", RelationGetRelationName(event_relation))));
      }

      if (event_relation->rd_rel->relrowsecurity)
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not convert table \"%s\" to a view because it has row security enabled", RelationGetRelationName(event_relation))));
      }

      if (relation_has_policies(event_relation))
      {
        ereport(ERROR, (errcode(ERRCODE_OBJECT_NOT_IN_PREREQUISITE_STATE), errmsg("could not convert table \"%s\" to a view because it has row security policies", RelationGetRelationName(event_relation))));
      }

      RelisBecomingView = true;
    }
  }
  else
  {
       
                                                                           
                                                                         
                                                                         
                                                                           
                                                                    
                                                         
       
    bool haveReturning = false;

    foreach (l, action)
    {
      query = lfirst_node(Query, l);

      if (!query->returningList)
      {
        continue;
      }
      if (haveReturning)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot have multiple RETURNING lists in a rule")));
      }
      haveReturning = true;
      if (event_qual != NULL)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("RETURNING lists are not supported in conditional rules")));
      }
      if (!is_instead)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("RETURNING lists are not supported in non-INSTEAD rules")));
      }
      checkRuleResultList(query->returningList, RelationGetDescr(event_relation), false, false);
    }

       
                                                                        
                                                                           
                                                             
       
    if (strcmp(rulename, ViewSelectRuleName) == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("non-view rule for \"%s\" must not be named \"%s\"", RelationGetRelationName(event_relation), ViewSelectRuleName)));
    }
  }

     
                                                   
     

                                                                      
  if (action != NIL || is_instead)
  {
    ruleId = InsertRule(rulename, event_type, event_relid, is_instead, event_qual, action, replace);

       
                                                                 
       
                                                                     
                                                                        
             
       
    SetRelationRuleStatus(event_relid, true);
  }

                                                                           
                                         
                                           
                                                                          
                        
                                                                        
                                                           
                                                                     
                                   
     
                                                               
                                                                           
     
  if (RelisBecomingView)
  {
    Relation relationRelation;
    Oid toastrelid;
    HeapTuple classTup;
    Form_pg_class classForm;

    relationRelation = table_open(RelationRelationId, RowExclusiveLock);
    toastrelid = event_relation->rd_rel->reltoastrelid;

                                                            
    RelationDropStorage(event_relation);
    DeleteSystemAttributeTuples(event_relid);

       
                                                                           
                                                                         
               
       
    if (OidIsValid(toastrelid))
    {
      ObjectAddress toastobject;

         
                                                                 
                                                                         
         
      deleteDependencyRecordsFor(RelationRelationId, toastrelid, false);

                                                      
      CommandCounterIncrement();

                                                     
      toastobject.classId = RelationRelationId;
      toastobject.objectId = toastrelid;
      toastobject.objectSubId = 0;
      performDeletion(&toastobject, DROP_RESTRICT, PERFORM_DELETION_INTERNAL);
    }

       
                                                                           
                                                                     
       
    CommandCounterIncrement();

       
                                                                          
                                                                           
                                     
       
    classTup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(event_relid));
    if (!HeapTupleIsValid(classTup))
    {
      elog(ERROR, "cache lookup failed for relation %u", event_relid);
    }
    classForm = (Form_pg_class)GETSTRUCT(classTup);

    classForm->relam = InvalidOid;
    classForm->reltablespace = InvalidOid;
    classForm->relpages = 0;
    classForm->reltuples = 0;
    classForm->relallvisible = 0;
    classForm->reltoastrelid = InvalidOid;
    classForm->relhasindex = false;
    classForm->relkind = RELKIND_VIEW;
    classForm->relfrozenxid = InvalidTransactionId;
    classForm->relminmxid = InvalidMultiXactId;
    classForm->relreplident = REPLICA_IDENTITY_NOTHING;

    CatalogTupleUpdate(relationRelation, &classTup->t_self, classTup);

    heap_freetuple(classTup);
    table_close(relationRelation, RowExclusiveLock);
  }

  ObjectAddressSet(address, RewriteRelationId, ruleId);

                                               
  table_close(event_relation, NoLock);

  return address;
}

   
                       
                                                                       
   
                                                                            
                                                                    
   
                                                                       
   
static void
checkRuleResultList(List *targetList, TupleDesc resultDesc, bool isSelect, bool requireColumnNameMatch)
{
  ListCell *tllist;
  int i;

                                                      
  Assert(isSelect || !requireColumnNameMatch);

  i = 0;
  foreach (tllist, targetList)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(tllist);
    Oid tletypid;
    int32 tletypmod;
    Form_pg_attribute attr;
    char *attname;

                                        
    if (tle->resjunk)
    {
      continue;
    }
    i++;
    if (i > resultDesc->natts)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), isSelect ? errmsg("SELECT rule's target list has too many entries") : errmsg("RETURNING list has too many entries")));
    }

    attr = TupleDescAttr(resultDesc, i - 1);
    attname = NameStr(attr->attname);

       
                                                                     
                                                                    
                                                                    
                                                                     
                                                                           
       
                                                                        
                                                                     
                                                                      
                                                              
                                                                      
                                                                          
                                                                           
                                                                           
                                                                           
                                                          
       
    if (attr->attisdropped)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), isSelect ? errmsg("cannot convert relation containing dropped columns to view") : errmsg("cannot create a RETURNING list for a relation containing dropped columns")));
    }

                                                                        
    if (requireColumnNameMatch && strcmp(tle->resname, attname) != 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("SELECT rule's target entry %d has different column name from column \"%s\"", i, attname), errdetail("SELECT target entry is named \"%s\".", tle->resname)));
    }

                           
    tletypid = exprType((Node *)tle->expr);
    if (attr->atttypid != tletypid)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), isSelect ? errmsg("SELECT rule's target entry %d has different type from column \"%s\"", i, attname) : errmsg("RETURNING list's entry %d has different type from column \"%s\"", i, attname), isSelect ? errdetail("SELECT target entry has type %s, but column has type %s.", format_type_be(tletypid), format_type_be(attr->atttypid)) : errdetail("RETURNING list entry has type %s, but column has type %s.", format_type_be(tletypid), format_type_be(attr->atttypid))));
    }

       
                                                                    
                                                                         
                                                                     
                                                         
       
    tletypmod = exprTypmod((Node *)tle->expr);
    if (attr->atttypmod != tletypmod && attr->atttypmod != -1 && tletypmod != -1)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), isSelect ? errmsg("SELECT rule's target entry %d has different size from column \"%s\"", i, attname) : errmsg("RETURNING list's entry %d has different size from column \"%s\"", i, attname), isSelect ? errdetail("SELECT target entry has type %s, but column has type %s.", format_type_with_typemod(tletypid, tletypmod), format_type_with_typemod(attr->atttypid, attr->atttypmod)) : errdetail("RETURNING list entry has type %s, but column has type %s.", format_type_with_typemod(tletypid, tletypmod), format_type_with_typemod(attr->atttypid, attr->atttypmod))));
    }
  }

  if (i != resultDesc->natts)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), isSelect ? errmsg("SELECT rule's target list has too few entries") : errmsg("RETURNING list has too few entries")));
  }
}

   
                      
                                                                        
                                                     
   
                                                                       
                                                                        
                                                                         
                                                                              
                                                                               
                
   
void
setRuleCheckAsUser(Node *node, Oid userid)
{
  (void)setRuleCheckAsUser_walker(node, &userid);
}

static bool
setRuleCheckAsUser_walker(Node *node, Oid *context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Query))
  {
    setRuleCheckAsUser_Query((Query *)node, *context);
    return false;
  }
  return expression_tree_walker(node, setRuleCheckAsUser_walker, (void *)context);
}

static void
setRuleCheckAsUser_Query(Query *qry, Oid userid)
{
  ListCell *l;

                                           
  foreach (l, qry->rtable)
  {
    RangeTblEntry *rte = (RangeTblEntry *)lfirst(l);

    if (rte->rtekind == RTE_SUBQUERY)
    {
                                         
      setRuleCheckAsUser_Query(rte->subquery, userid);
    }
    else
    {
      rte->checkAsUser = userid;
    }
  }

                                     
  foreach (l, qry->cteList)
  {
    CommonTableExpr *cte = (CommonTableExpr *)lfirst(l);

    setRuleCheckAsUser_Query(castNode(Query, cte->ctequery), userid);
  }

                                                                     
  if (qry->hasSubLinks)
  {
    query_tree_walker(qry, setRuleCheckAsUser_walker, (void *)&userid, QTW_IGNORE_RC_SUBQUERIES);
  }
}

   
                                                    
   
void
EnableDisableRule(Relation rel, const char *rulename, char fires_when)
{
  Relation pg_rewrite_desc;
  Oid owningRel = RelationGetRelid(rel);
  Oid eventRelationOid;
  HeapTuple ruletup;
  Form_pg_rewrite ruleform;
  bool changed = false;

     
                                    
     
  pg_rewrite_desc = table_open(RewriteRelationId, RowExclusiveLock);
  ruletup = SearchSysCacheCopy2(RULERELNAME, ObjectIdGetDatum(owningRel), PointerGetDatum(rulename));
  if (!HeapTupleIsValid(ruletup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("rule \"%s\" for relation \"%s\" does not exist", rulename, get_rel_name(owningRel))));
  }

  ruleform = (Form_pg_rewrite)GETSTRUCT(ruletup);

     
                                                       
     
  eventRelationOid = ruleform->ev_class;
  Assert(eventRelationOid == owningRel);
  if (!pg_class_ownercheck(eventRelationOid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(get_rel_relkind(eventRelationOid)), get_rel_name(eventRelationOid));
  }

     
                                                                      
     
  if (DatumGetChar(ruleform->ev_enabled) != fires_when)
  {
    ruleform->ev_enabled = CharGetDatum(fires_when);
    CatalogTupleUpdate(pg_rewrite_desc, &ruletup->t_self, ruletup);

    changed = true;
  }

  InvokeObjectPostAlterHook(RewriteRelationId, ruleform->oid, 0);

  heap_freetuple(ruletup);
  table_close(pg_rewrite_desc, RowExclusiveLock);

     
                                                                        
                                                                        
                                                            
     
  if (changed)
  {
    CacheInvalidateRelcache(rel);
  }
}

   
                                                                              
   
static void
RangeVarCallbackForRenameRule(const RangeVar *rv, Oid relid, Oid oldrelid, void *arg)
{
  HeapTuple tuple;
  Form_pg_class form;

  tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(tuple))
  {
    return;                           
  }
  form = (Form_pg_class)GETSTRUCT(tuple);

                                            
  if (form->relkind != RELKIND_RELATION && form->relkind != RELKIND_VIEW && form->relkind != RELKIND_PARTITIONED_TABLE)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table or view", rv->relname)));
  }

  if (!allowSystemTableMods && IsSystemClass(relid, form))
  {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied: \"%s\" is a system catalog", rv->relname)));
  }

                                                         
  if (!pg_class_ownercheck(relid, GetUserId()))
  {
    aclcheck_error(ACLCHECK_NOT_OWNER, get_relkind_objtype(get_rel_relkind(relid)), rv->relname);
  }

  ReleaseSysCache(tuple);
}

   
                                    
   
ObjectAddress
RenameRewriteRule(RangeVar *relation, const char *oldName, const char *newName)
{
  Oid relid;
  Relation targetrel;
  Relation pg_rewrite_desc;
  HeapTuple ruletup;
  Form_pg_rewrite ruleform;
  Oid ruleOid;
  ObjectAddress address;

     
                                                                          
                                        
     
  relid = RangeVarGetRelidExtended(relation, AccessExclusiveLock, 0, RangeVarCallbackForRenameRule, NULL);

                                                                
  targetrel = relation_open(relid, NoLock);

                                    
  pg_rewrite_desc = table_open(RewriteRelationId, RowExclusiveLock);

                                                    
  ruletup = SearchSysCacheCopy2(RULERELNAME, ObjectIdGetDatum(relid), PointerGetDatum(oldName));
  if (!HeapTupleIsValid(ruletup))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("rule \"%s\" for relation \"%s\" does not exist", oldName, RelationGetRelationName(targetrel))));
  }
  ruleform = (Form_pg_rewrite)GETSTRUCT(ruletup);
  ruleOid = ruleform->oid;

                                                       
  if (IsDefinedRewriteRule(relid, newName))
  {
    ereport(ERROR, (errcode(ERRCODE_DUPLICATE_OBJECT), errmsg("rule \"%s\" for relation \"%s\" already exists", newName, RelationGetRelationName(targetrel))));
  }

     
                                                                         
                      
     
  if (ruleform->ev_type == CMD_SELECT + '0')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("renaming an ON SELECT rule is not allowed")));
  }

                         
  namestrcpy(&(ruleform->rulename), newName);

  CatalogTupleUpdate(pg_rewrite_desc, &ruletup->t_self, ruletup);

  heap_freetuple(ruletup);
  table_close(pg_rewrite_desc, RowExclusiveLock);

     
                                                                           
                                                                          
                                                   
     
  CacheInvalidateRelcache(targetrel);

  ObjectAddressSet(address, RewriteRelationId, ruleOid);

     
                                         
     
  relation_close(targetrel, NoLock);

  return address;
}
