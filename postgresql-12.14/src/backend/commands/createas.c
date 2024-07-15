                                                                            
   
              
                                                          
                                                                      
                                  
   
                                                                 
                                  
   
                                                                    
                                                                            
                                                                         
                                                       
   
                                                                         
                                                                        
   
   
                  
                                     
   
                                                                            
   
#include "postgres.h"

#include "access/heapam.h"
#include "access/reloptions.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "access/xlog.h"
#include "catalog/namespace.h"
#include "catalog/toasting.h"
#include "commands/createas.h"
#include "commands/matview.h"
#include "commands/prepare.h"
#include "commands/tablecmds.h"
#include "commands/view.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_clause.h"
#include "rewrite/rewriteHandler.h"
#include "storage/smgr.h"
#include "tcop/tcopprot.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/rls.h"
#include "utils/snapmgr.h"

typedef struct
{
  DestReceiver pub;                                       
  IntoClause *into;                                    
                                                   
  Relation rel;                                      
  ObjectAddress reladdr;                                              
  CommandId output_cid;                                         
  int ti_options;                                                      
  BulkInsertState bistate;                        
} DR_intorel;

                                                    
static ObjectAddress
create_ctas_internal(List *attrList, IntoClause *into);
static ObjectAddress
create_ctas_nodata(List *tlist, IntoClause *into);

                                               
static void
intorel_startup(DestReceiver *self, int operation, TupleDesc typeinfo);
static bool
intorel_receive(TupleTableSlot *slot, DestReceiver *self);
static void
intorel_shutdown(DestReceiver *self);
static void
intorel_destroy(DestReceiver *self);

   
                        
   
                                                                          
                                                                        
                                                   
   
static ObjectAddress
create_ctas_internal(List *attrList, IntoClause *into)
{
  CreateStmt *create = makeNode(CreateStmt);
  bool is_matview;
  char relkind;
  Datum toast_options;
  static char *validnsps[] = HEAP_RELOPT_NAMESPACES;
  ObjectAddress intoRelationAddr;

                                                                            
  is_matview = (into->viewQuery != NULL);
  relkind = is_matview ? RELKIND_MATVIEW : RELKIND_RELATION;

     
                                                                          
                                   
     
  create->relation = into->rel;
  create->tableElts = attrList;
  create->inhRelations = NIL;
  create->ofTypename = NULL;
  create->constraints = NIL;
  create->options = into->options;
  create->oncommit = into->onCommit;
  create->tablespacename = into->tableSpaceName;
  create->if_not_exists = false;
  create->accessMethod = into->accessMethod;

     
                                                                             
                                                                    
     
  intoRelationAddr = DefineRelation(create, relkind, InvalidOid, NULL, NULL);

     
                                                                         
                                                                         
                                                         
     
  CommandCounterIncrement();

                                                         
  toast_options = transformRelOptions((Datum)0, create->options, "toast", validnsps, true, false);

  (void)heap_reloptions(RELKIND_TOASTVALUE, toast_options, true);

  NewRelationCreateToastTable(intoRelationAddr.objectId, toast_options);

                                                      
  if (is_matview)
  {
                                                          
    Query *query = (Query *)copyObject(into->viewQuery);

    StoreViewQuery(intoRelationAddr.objectId, query, false);
    CommandCounterIncrement();
  }

  return intoRelationAddr;
}

   
                      
   
                                                                             
                                                    
   
static ObjectAddress
create_ctas_nodata(List *tlist, IntoClause *into)
{
  List *attrList;
  ListCell *t, *lc;

     
                                                                         
                                                                            
                                                                           
     
  attrList = NIL;
  lc = list_head(into->colNames);
  foreach (t, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(t);

    if (!tle->resjunk)
    {
      ColumnDef *col;
      char *colname;

      if (lc)
      {
        colname = strVal(lfirst(lc));
        lc = lnext(lc);
      }
      else
      {
        colname = tle->resname;
      }

      col = makeColumnDef(colname, exprType((Node *)tle->expr), exprTypmod((Node *)tle->expr), exprCollation((Node *)tle->expr));

         
                                                                       
                                                                     
                                                                       
                                                     
         
      if (!OidIsValid(col->collOid) && type_is_collatable(col->typeName->typeOid))
      {
        ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("no collation was derived for column \"%s\" with collatable type %s", col->colname, format_type_be(col->typeName->typeOid)), errhint("Use the COLLATE clause to set the collation explicitly.")));
      }

      attrList = lappend(attrList, col);
    }
  }

  if (lc != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("too many column names were specified")));
  }

                                                               
  return create_ctas_internal(attrList, into);
}

   
                                                          
   
ObjectAddress
ExecCreateTableAs(CreateTableAsStmt *stmt, const char *queryString, ParamListInfo params, QueryEnvironment *queryEnv, char *completionTag)
{
  Query *query = castNode(Query, stmt->query);
  IntoClause *into = stmt->into;
  bool is_matview = (into->viewQuery != NULL);
  DestReceiver *dest;
  Oid save_userid = InvalidOid;
  int save_sec_context = 0;
  int save_nestlevel = 0;
  ObjectAddress address;
  List *rewritten;
  PlannedStmt *plan;
  QueryDesc *queryDesc;

  if (stmt->if_not_exists)
  {
    Oid nspid;
    Oid oldrelid;

    nspid = RangeVarGetCreationNamespace(into->rel);

    oldrelid = get_relname_relid(into->rel->relname, nspid);
    if (OidIsValid(oldrelid))
    {
         
                                                                   
         
                                                                        
                                                                       
         
      ObjectAddressSet(address, RelationRelationId, oldrelid);
      checkMembershipInCurrentExtension(&address);

                      
      ereport(NOTICE, (errcode(ERRCODE_DUPLICATE_TABLE), errmsg("relation \"%s\" already exists, skipping", into->rel->relname)));
      return InvalidObjectAddress;
    }
  }

     
                                                                   
     
  dest = CreateIntoRelDestReceiver(into);

     
                                                                           
                                                         
     
  if (query->commandType == CMD_UTILITY && IsA(query->utilityStmt, ExecuteStmt))
  {
    ExecuteStmt *estmt = castNode(ExecuteStmt, query->utilityStmt);

    Assert(!is_matview);                         
    ExecuteQuery(estmt, into, queryString, params, dest, completionTag);

                                                              
    address = ((DR_intorel *)dest)->reladdr;

    return address;
  }
  Assert(query->commandType == CMD_SELECT);

     
                                                                          
                                                                          
                                                                        
                                                                            
                                   
     
  if (is_matview)
  {
    GetUserIdAndSecContext(&save_userid, &save_sec_context);
    SetUserIdAndSecContext(save_userid, save_sec_context | SECURITY_RESTRICTED_OPERATION);
    save_nestlevel = NewGUCNestLevel();
  }

  if (into->skipData)
  {
       
                                                                      
                                                                         
                                                                           
                                                                    
       
    address = create_ctas_nodata(query->targetList, into);
  }
  else
  {
       
                                                                          
                                                                        
                                                                    
                                
       
                                                                          
                                                                       
                                                                         
                                                                           
                 
       
    rewritten = QueryRewrite(copyObject(query));

                                                                           
    if (list_length(rewritten) != 1)
    {
      elog(ERROR, "unexpected rewrite result for %s", is_matview ? "CREATE MATERIALIZED VIEW" : "CREATE TABLE AS SELECT");
    }
    query = linitial_node(Query, rewritten);
    Assert(query->commandType == CMD_SELECT);

                        
    plan = pg_plan_query(query, CURSOR_OPT_PARALLEL_OK, params);

       
                                                                           
                                                                     
                                                                        
                                                                   
                                           
       
    PushCopiedSnapshot(GetActiveSnapshot());
    UpdateActiveSnapshotCommandId();

                                                                      
    queryDesc = CreateQueryDesc(plan, queryString, GetActiveSnapshot(), InvalidSnapshot, dest, params, queryEnv, 0);

                                                              
    ExecutorStart(queryDesc, GetIntoRelEFlags(into));

                                    
    ExecutorRun(queryDesc, ForwardScanDirection, 0L, true);

                                                                  
    if (completionTag)
    {
      snprintf(completionTag, COMPLETION_TAG_BUFSIZE, "SELECT " UINT64_FORMAT, queryDesc->estate->es_processed);
    }

                                                              
    address = ((DR_intorel *)dest)->reladdr;

                      
    ExecutorFinish(queryDesc);
    ExecutorEnd(queryDesc);

    FreeQueryDesc(queryDesc);

    PopActiveSnapshot();
  }

  if (is_matview)
  {
                                   
    AtEOXact_GUC(false, save_nestlevel);

                                             
    SetUserIdAndSecContext(save_userid, save_sec_context);
  }

  return address;
}

   
                                                                          
   
                                                                           
                                                                            
                                                                          
                                     
   
int
GetIntoRelEFlags(IntoClause *intoClause)
{
  int flags = 0;

  if (intoClause->skipData)
  {
    flags |= EXEC_FLAG_WITH_NO_DATA;
  }

  return flags;
}

   
                                                                      
   
                                                                         
                                                                         
                                                             
   
DestReceiver *
CreateIntoRelDestReceiver(IntoClause *intoClause)
{
  DR_intorel *self = (DR_intorel *)palloc0(sizeof(DR_intorel));

  self->pub.receiveSlot = intorel_receive;
  self->pub.rStartup = intorel_startup;
  self->pub.rShutdown = intorel_shutdown;
  self->pub.rDestroy = intorel_destroy;
  self->pub.mydest = DestIntoRel;
  self->into = intoClause;
                                                               

  return (DestReceiver *)self;
}

   
                                        
   
static void
intorel_startup(DestReceiver *self, int operation, TupleDesc typeinfo)
{
  DR_intorel *myState = (DR_intorel *)self;
  IntoClause *into = myState->into;
  bool is_matview;
  char relkind;
  List *attrList;
  ObjectAddress intoRelationAddr;
  Relation intoRelationDesc;
  RangeTblEntry *rte;
  ListCell *lc;
  int attnum;

  Assert(into != NULL);                                     

                                                                            
  is_matview = (into->viewQuery != NULL);
  relkind = is_matview ? RELKIND_MATVIEW : RELKIND_RELATION;

     
                                                                             
                                                                       
                                                                             
                    
     
  attrList = NIL;
  lc = list_head(into->colNames);
  for (attnum = 0; attnum < typeinfo->natts; attnum++)
  {
    Form_pg_attribute attribute = TupleDescAttr(typeinfo, attnum);
    ColumnDef *col;
    char *colname;

    if (lc)
    {
      colname = strVal(lfirst(lc));
      lc = lnext(lc);
    }
    else
    {
      colname = NameStr(attribute->attname);
    }

    col = makeColumnDef(colname, attribute->atttypid, attribute->atttypmod, attribute->attcollation);

       
                                                                     
                                                                         
                                                                       
                                           
       
    if (!OidIsValid(col->collOid) && type_is_collatable(col->typeName->typeOid))
    {
      ereport(ERROR, (errcode(ERRCODE_INDETERMINATE_COLLATION), errmsg("no collation was derived for column \"%s\" with collatable type %s", col->colname, format_type_be(col->typeName->typeOid)), errhint("Use the COLLATE clause to set the collation explicitly.")));
    }

    attrList = lappend(attrList, col);
  }

  if (lc != NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("too many column names were specified")));
  }

     
                                      
     
  intoRelationAddr = create_ctas_internal(attrList, into);

     
                                          
     
  intoRelationDesc = table_open(intoRelationAddr.objectId, AccessExclusiveLock);

     
                                                       
     
                                                                            
              
     
  rte = makeNode(RangeTblEntry);
  rte->rtekind = RTE_RELATION;
  rte->relid = intoRelationAddr.objectId;
  rte->relkind = relkind;
  rte->rellockmode = RowExclusiveLock;
  rte->requiredPerms = ACL_INSERT;

  for (attnum = 1; attnum <= intoRelationDesc->rd_att->natts; attnum++)
  {
    rte->insertedCols = bms_add_member(rte->insertedCols, attnum - FirstLowInvalidHeapAttributeNumber);
  }

  ExecCheckRTPerms(list_make1(rte), true);

     
                                                                
     
                                                                             
                                                                            
                                                                          
                                             
     
  if (check_enable_rls(intoRelationAddr.objectId, InvalidOid, false) == RLS_ENABLED)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), (errmsg("policies not yet implemented for this command"))));
  }

     
                                                                           
                                                    
     
  if (is_matview && !into->skipData)
  {
    SetMatViewPopulatedState(intoRelationDesc, true);
  }

     
                                                              
     
  myState->rel = intoRelationDesc;
  myState->reladdr = intoRelationAddr;
  myState->output_cid = GetCurrentCommandId(true);

     
                                                                      
                                                             
     
  myState->ti_options = TABLE_INSERT_SKIP_FSM | (XLogIsNeeded() ? 0 : TABLE_INSERT_SKIP_WAL);
  myState->bistate = GetBulkInsertState();

                                                                  
  Assert(RelationGetTargetBlock(intoRelationDesc) == InvalidBlockNumber);
}

   
                                         
   
static bool
intorel_receive(TupleTableSlot *slot, DestReceiver *self)
{
  DR_intorel *myState = (DR_intorel *)self;

     
                                                                     
                                                                           
                                                                        
                                                                        
                                                                        
                                                       
     

  table_tuple_insert(myState->rel, slot, myState->output_cid, myState->ti_options, myState->bistate);

                                                                         

  return true;
}

   
                                     
   
static void
intorel_shutdown(DestReceiver *self)
{
  DR_intorel *myState = (DR_intorel *)self;

  FreeBulkInsertState(myState->bistate);

  table_finish_bulk_insert(myState->rel, myState->ti_options);

                                             
  table_close(myState->rel, NoLock);
  myState->rel = NULL;
}

   
                                                   
   
static void
intorel_destroy(DestReceiver *self)
{
  pfree(self);
}
