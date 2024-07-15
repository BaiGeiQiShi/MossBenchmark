                                                                            
   
                 
                                                  
   
                                                                         
                                                                        
   
                                      
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/relscan.h"
#include "access/sysattr.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/portal.h"
#include "utils/rel.h"

static char *
fetch_cursor_param_value(ExprContext *econtext, int paramId);
static ScanState *
search_plan_tree(PlanState *node, Oid table_oid, bool *pending_rescan);

   
                 
   
                                                                             
                                                                              
                                               
   
                                                                               
                                                                             
                                                                           
                                                
   
bool
execCurrentOf(CurrentOfExpr *cexpr, ExprContext *econtext, Oid table_oid, ItemPointer current_tid)
{
  char *cursor_name;
  char *table_name;
  Portal portal;
  QueryDesc *queryDesc;

                                                                         
  if (cexpr->cursor_name)
  {
    cursor_name = cexpr->cursor_name;
  }
  else
  {
    cursor_name = fetch_cursor_param_value(econtext, cexpr->cursor_param);
  }

                                                           
  table_name = get_rel_name(table_oid);
  if (table_name == NULL)
  {
    elog(ERROR, "cache lookup failed for relation %u", table_oid);
  }

                                
  portal = GetPortalByName(cursor_name);
  if (!PortalIsValid(portal))
  {
    ereport(ERROR, (errcode(ERRCODE_UNDEFINED_CURSOR), errmsg("cursor \"%s\" does not exist", cursor_name)));
  }

     
                                                                          
                                            
     
  if (portal->strategy != PORTAL_ONE_SELECT)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_CURSOR_STATE), errmsg("cursor \"%s\" is not a SELECT query", cursor_name)));
  }
  queryDesc = portal->queryDesc;
  if (queryDesc == NULL || queryDesc->estate == NULL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_CURSOR_STATE), errmsg("cursor \"%s\" is held from a previous transaction", cursor_name)));
  }

     
                                                                           
                                                                          
                                                                            
                                                                             
                                            
     
  if (queryDesc->estate->es_rowmarks)
  {
    ExecRowMark *erm;
    Index i;

       
                                                                           
                                                               
       
    erm = NULL;
    for (i = 0; i < queryDesc->estate->es_range_table_size; i++)
    {
      ExecRowMark *thiserm = queryDesc->estate->es_rowmarks[i];

      if (thiserm == NULL || !RowMarkRequiresRowShareLock(thiserm->markType))
      {
        continue;                                        
      }

      if (thiserm->relid == table_oid)
      {
        if (erm)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_CURSOR_STATE), errmsg("cursor \"%s\" has multiple FOR UPDATE/SHARE references to table \"%s\"", cursor_name, table_name)));
        }
        erm = thiserm;
      }
    }

    if (erm == NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_CURSOR_STATE), errmsg("cursor \"%s\" does not have a FOR UPDATE/SHARE reference to table \"%s\"", cursor_name, table_name)));
    }

       
                                                                         
                        
       
    if (portal->atStart || portal->atEnd)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_CURSOR_STATE), errmsg("cursor \"%s\" is not positioned on a row", cursor_name)));
    }

                                                           
    if (ItemPointerIsValid(&(erm->curCtid)))
    {
      *current_tid = erm->curCtid;
      return true;
    }

       
                                                                      
                                                                         
                                 
       
    return false;
  }
  else
  {
       
                                                                        
                                                               
                    
       
    ScanState *scanstate;
    bool pending_rescan = false;

    scanstate = search_plan_tree(queryDesc->planstate, table_oid, &pending_rescan);
    if (!scanstate)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_CURSOR_STATE), errmsg("cursor \"%s\" is not a simply updatable scan of table \"%s\"", cursor_name, table_name)));
    }

       
                                                                         
                                                                           
                                                                        
                                                                        
                                                                           
       
    if (portal->atStart || portal->atEnd)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_CURSOR_STATE), errmsg("cursor \"%s\" is not positioned on a row", cursor_name)));
    }

       
                                                                   
                                                                     
                              
       
    if (TupIsNull(scanstate->ss_ScanTupleSlot) || pending_rescan)
    {
      return false;
    }

       
                                                                         
                                                                         
                                                        
       
    if (IsA(scanstate, IndexOnlyScanState))
    {
         
                                                                        
                                                                        
                                             
         
      IndexScanDesc scan = ((IndexOnlyScanState *)scanstate)->ioss_ScanDesc;

      *current_tid = scan->xs_heaptid;
    }
    else
    {
         
                                                                     
                                                                         
                                                                       
                  
         
      Datum ldatum;
      bool lisnull;
      ItemPointer tuple_tid;

#ifdef USE_ASSERT_CHECKING
      ldatum = slot_getsysattr(scanstate->ss_ScanTupleSlot, TableOidAttributeNumber, &lisnull);
      if (lisnull)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_CURSOR_STATE), errmsg("cursor \"%s\" is not a simply updatable scan of table \"%s\"", cursor_name, table_name)));
      }
      Assert(DatumGetObjectId(ldatum) == table_oid);
#endif

      ldatum = slot_getsysattr(scanstate->ss_ScanTupleSlot, SelfItemPointerAttributeNumber, &lisnull);
      if (lisnull)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_CURSOR_STATE), errmsg("cursor \"%s\" is not a simply updatable scan of table \"%s\"", cursor_name, table_name)));
      }
      tuple_tid = (ItemPointer)DatumGetPointer(ldatum);

      *current_tid = *tuple_tid;
    }

    Assert(ItemPointerIsValid(current_tid));

    return true;
  }
}

   
                            
   
                                                                         
   
static char *
fetch_cursor_param_value(ExprContext *econtext, int paramId)
{
  ParamListInfo paramInfo = econtext->ecxt_param_list_info;

  if (paramInfo && paramId > 0 && paramId <= paramInfo->numParams)
  {
    ParamExternData *prm;
    ParamExternData prmdata;

                                                         
    if (paramInfo->paramFetch != NULL)
    {
      prm = paramInfo->paramFetch(paramInfo, paramId, false, &prmdata);
    }
    else
    {
      prm = &paramInfo->params[paramId - 1];
    }

    if (OidIsValid(prm->ptype) && !prm->isnull)
    {
                                                              
      if (prm->ptype != REFCURSOROID)
      {
        ereport(ERROR, (errcode(ERRCODE_DATATYPE_MISMATCH), errmsg("type of parameter %d (%s) does not match that when preparing the plan (%s)", paramId, format_type_be(prm->ptype), format_type_be(REFCURSOROID))));
      }

                                                           
      return TextDatumGetCString(prm->value);
    }
  }

  ereport(ERROR, (errcode(ERRCODE_UNDEFINED_OBJECT), errmsg("no value found for parameter %d", paramId)));
  return NULL;
}

   
                    
   
                                                                           
                                                    
   
                                                                            
                                                                           
                                                                
   
                                                                          
                                                                            
                                                                            
                                                                          
                                                                            
                                      
   
static ScanState *
search_plan_tree(PlanState *node, Oid table_oid, bool *pending_rescan)
{
  ScanState *result = NULL;

  if (node == NULL)
  {
    return NULL;
  }
  switch (nodeTag(node))
  {
       
                                                                     
                                              
       
                                                                       
                                                                    
                                                                  
                                                                 
                                    
       
  case T_SeqScanState:
  case T_SampleScanState:
  case T_IndexScanState:
  case T_IndexOnlyScanState:
  case T_BitmapHeapScanState:
  case T_TidScanState:
  case T_ForeignScanState:
  case T_CustomScanState:
  {
    ScanState *sstate = (ScanState *)node;

    if (sstate->ss_currentRelation && RelationGetRelid(sstate->ss_currentRelation) == table_oid)
    {
      result = sstate;
    }
    break;
  }

       
                                                                
                                                                     
                                                                       
                                                                      
                                                      
                                                                      
                                                                
                 
       
                                                                 
                                                                   
       
                                                                     
                                                                     
                                                                  
                                                                       
                                                                      
                                                                     
                                                               
                                    
       
  case T_AppendState:
  {
    AppendState *astate = (AppendState *)node;
    int i;

    for (i = 0; i < astate->as_nplans; i++)
    {
      ScanState *elem = search_plan_tree(astate->appendplans[i], table_oid, pending_rescan);

      if (!elem)
      {
        continue;
      }
      if (result)
      {
        return NULL;                       
      }
      result = elem;
    }
    break;
  }

       
                                                                 
                                                             
       
  case T_ResultState:
  case T_LimitState:
    result = search_plan_tree(node->lefttree, table_oid, pending_rescan);
    break;

       
                                                                     
       
  case T_SubqueryScanState:
    result = search_plan_tree(((SubqueryScanState *)node)->subplan, table_oid, pending_rescan);
    break;

  default:
                                                       
    break;
  }

     
                                                                     
                                                                         
     
  if (result && node->chgParam != NULL)
  {
    *pending_rescan = true;
  }

  return result;
}
