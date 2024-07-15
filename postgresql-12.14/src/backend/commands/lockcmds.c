                                                                            
   
              
                               
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/table.h"
#include "access/xact.h"
#include "catalog/namespace.h"
#include "catalog/pg_inherits.h"
#include "commands/lockcmds.h"
#include "miscadmin.h"
#include "parser/parse_clause.h"
#include "storage/lmgr.h"
#include "utils/acl.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "rewrite/rewriteHandler.h"
#include "nodes/nodeFuncs.h"

static void
LockTableRecurse(Oid reloid, LOCKMODE lockmode, bool nowait, Oid userid);
static AclResult
LockTableAclCheck(Oid relid, LOCKMODE lockmode, Oid userid);
static void
RangeVarCallbackForLockTable(const RangeVar *rv, Oid relid, Oid oldrelid, void *arg);
static void
LockViewRecurse(Oid reloid, LOCKMODE lockmode, bool nowait, List *ancestor_views);

   
              
   
void
LockTableCommand(LockStmt *lockstmt)
{
  ListCell *p;

              
                                                      
                                         
                                      
                                          
                                                                            
              
     
  if (lockstmt->mode > RowExclusiveLock)
  {
    PreventCommandDuringRecovery("LOCK TABLE");
  }

     
                                                                         
     
  foreach (p, lockstmt->relations)
  {
    RangeVar *rv = (RangeVar *)lfirst(p);
    bool recurse = rv->inh;
    Oid reloid;

    reloid = RangeVarGetRelidExtended(rv, lockstmt->mode, lockstmt->nowait ? RVR_NOWAIT : 0, RangeVarCallbackForLockTable, (void *)&lockstmt->mode);

    if (get_rel_relkind(reloid) == RELKIND_VIEW)
    {
      LockViewRecurse(reloid, lockstmt->mode, lockstmt->nowait, NIL);
    }
    else if (recurse)
    {
      LockTableRecurse(reloid, lockstmt->mode, lockstmt->nowait, GetUserId());
    }
  }
}

   
                                                                           
                        
   
static void
RangeVarCallbackForLockTable(const RangeVar *rv, Oid relid, Oid oldrelid, void *arg)
{
  LOCKMODE lockmode = *(LOCKMODE *)arg;
  char relkind;
  char relpersistence;
  AclResult aclresult;

  if (!OidIsValid(relid))
  {
    return;                                             
  }
  relkind = get_rel_relkind(relid);
  if (!relkind)
  {
    return;                                                
                       
  }

                                                                   
  if (relkind != RELKIND_RELATION && relkind != RELKIND_PARTITIONED_TABLE && relkind != RELKIND_VIEW)
  {
    ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("\"%s\" is not a table or view", rv->relname)));
  }

     
                                                                 
                  
     
  relpersistence = get_rel_persistence(relid);
  if (relpersistence == RELPERSISTENCE_TEMP)
  {
    MyXactFlags |= XACT_FLAGS_ACCESSEDTEMPNAMESPACE;
  }

                          
  aclresult = LockTableAclCheck(relid, lockmode, GetUserId());
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, get_relkind_objtype(get_rel_relkind(relid)), rv->relname);
  }
}

   
                                                         
   
                                                                            
                                                                        
                                                                       
   
static void
LockTableRecurse(Oid reloid, LOCKMODE lockmode, bool nowait, Oid userid)
{
  List *children;
  ListCell *lc;

  children = find_inheritance_children(reloid, NoLock);

  foreach (lc, children)
  {
    Oid childreloid = lfirst_oid(lc);
    AclResult aclresult;

                                                      
    aclresult = LockTableAclCheck(childreloid, lockmode, userid);
    if (aclresult != ACLCHECK_OK)
    {
      char *relname = get_rel_name(childreloid);

      if (!relname)
      {
        continue;                                               
      }
      aclcheck_error(aclresult, get_relkind_objtype(get_rel_relkind(childreloid)), relname);
    }

                                                            
    if (!nowait)
    {
      LockRelationOid(childreloid, lockmode);
    }
    else if (!ConditionalLockRelationOid(childreloid, lockmode))
    {
                                                                    
      char *relname = get_rel_name(childreloid);

      if (!relname)
      {
        continue;                                               
      }
      ereport(ERROR, (errcode(ERRCODE_LOCK_NOT_AVAILABLE), errmsg("could not obtain lock on relation \"%s\"", relname)));
    }

       
                                                                   
                                       
       
    if (!SearchSysCacheExists1(RELOID, ObjectIdGetDatum(childreloid)))
    {
                                
      UnlockRelationOid(childreloid, lockmode);
      continue;
    }

    LockTableRecurse(childreloid, lockmode, nowait, userid);
  }
}

   
                                            
   
                                                                          
                                        
   

typedef struct
{
  LOCKMODE lockmode;                          
  bool nowait;                            
  Oid viewowner;                                                   
  Oid viewoid;                                            
  List *ancestor_views;                             
} LockViewRecurse_context;

static bool
LockViewRecurse_walker(Node *node, LockViewRecurse_context *context)
{
  if (node == NULL)
  {
    return false;
  }

  if (IsA(node, Query))
  {
    Query *query = (Query *)node;
    ListCell *rtable;

    foreach (rtable, query->rtable)
    {
      RangeTblEntry *rte = lfirst(rtable);
      AclResult aclresult;

      Oid relid = rte->relid;
      char relkind = rte->relkind;
      char *relname = get_rel_name(relid);

         
                                                                      
                  
         
      if (relid == context->viewoid && (strcmp(rte->eref->aliasname, "old") == 0 || strcmp(rte->eref->aliasname, "new") == 0))
      {
        continue;
      }

                                                                        
      if (relkind != RELKIND_RELATION && relkind != RELKIND_PARTITIONED_TABLE && relkind != RELKIND_VIEW)
      {
        continue;
      }

         
                                                                      
                                                              
         
      if (list_member_oid(context->ancestor_views, relid))
      {
        continue;
      }

                                                              
      aclresult = LockTableAclCheck(relid, context->lockmode, context->viewowner);
      if (aclresult != ACLCHECK_OK)
      {
        aclcheck_error(aclresult, get_relkind_objtype(relkind), relname);
      }

                                                              
      if (!context->nowait)
      {
        LockRelationOid(relid, context->lockmode);
      }
      else if (!ConditionalLockRelationOid(relid, context->lockmode))
      {
        ereport(ERROR, (errcode(ERRCODE_LOCK_NOT_AVAILABLE), errmsg("could not obtain lock on relation \"%s\"", relname)));
      }

      if (relkind == RELKIND_VIEW)
      {
        LockViewRecurse(relid, context->lockmode, context->nowait, context->ancestor_views);
      }
      else if (rte->inh)
      {
        LockTableRecurse(relid, context->lockmode, context->nowait, context->viewowner);
      }
    }

    return query_tree_walker(query, LockViewRecurse_walker, context, QTW_IGNORE_JOINALIASES);
  }

  return expression_tree_walker(node, LockViewRecurse_walker, context);
}

static void
LockViewRecurse(Oid reloid, LOCKMODE lockmode, bool nowait, List *ancestor_views)
{
  LockViewRecurse_context context;
  Relation view;
  Query *viewquery;

                                          
  view = table_open(reloid, NoLock);
  viewquery = get_view_query(view);

  context.lockmode = lockmode;
  context.nowait = nowait;
  context.viewowner = view->rd_rel->relowner;
  context.viewoid = reloid;
  context.ancestor_views = lcons_oid(reloid, ancestor_views);

  LockViewRecurse_walker((Node *)viewquery, &context);

  ancestor_views = list_delete_oid(ancestor_views, reloid);

  table_close(view, NoLock);
}

   
                                                                      
   
static AclResult
LockTableAclCheck(Oid reloid, LOCKMODE lockmode, Oid userid)
{
  AclResult aclresult;
  AclMode aclmask;

                                 
  if (lockmode == AccessShareLock)
  {
    aclmask = ACL_SELECT;
  }
  else if (lockmode == RowExclusiveLock)
  {
    aclmask = ACL_INSERT | ACL_UPDATE | ACL_DELETE | ACL_TRUNCATE;
  }
  else
  {
    aclmask = ACL_UPDATE | ACL_DELETE | ACL_TRUNCATE;
  }

  aclresult = pg_class_aclcheck(reloid, userid, aclmask);

  return aclresult;
}
