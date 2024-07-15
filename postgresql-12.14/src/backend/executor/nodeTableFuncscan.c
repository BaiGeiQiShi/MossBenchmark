                                                                            
   
                       
                                                                             
   
                                                                         
                                                                        
   
   
                  
                                              
   
                                                                            
   
   
                      
                                         
                                                               
                                                                        
                                                          
                                                 
   
#include "postgres.h"

#include "nodes/execnodes.h"
#include "executor/executor.h"
#include "executor/nodeTableFuncscan.h"
#include "executor/tablefunc.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/xml.h"

static TupleTableSlot *
TableFuncNext(TableFuncScanState *node);
static bool
TableFuncRecheck(TableFuncScanState *node, TupleTableSlot *slot);

static void
tfuncFetchRows(TableFuncScanState *tstate, ExprContext *econtext);
static void
tfuncInitialize(TableFuncScanState *tstate, ExprContext *econtext, Datum doc);
static void
tfuncLoadRows(TableFuncScanState *tstate, ExprContext *econtext);

                                                                    
                     
                                                                    
   
                                                                    
                  
   
                                              
                                                                    
   
static TupleTableSlot *
TableFuncNext(TableFuncScanState *node)
{
  TupleTableSlot *scanslot;

  scanslot = node->ss.ss_ScanTupleSlot;

     
                                                                            
                                                                     
     
  if (node->tupstore == NULL)
  {
    tfuncFetchRows(node, node->ss.ps.ps_ExprContext);
  }

     
                                         
     
  (void)tuplestore_gettupleslot(node->tupstore, true, false, scanslot);
  return scanslot;
}

   
                                                                                
   
static bool
TableFuncRecheck(TableFuncScanState *node, TupleTableSlot *slot)
{
                        
  return true;
}

                                                                    
                            
   
                                                                    
           
                                                               
                             
                                                                    
   
static TupleTableSlot *
ExecTableFuncScan(PlanState *pstate)
{
  TableFuncScanState *node = castNode(TableFuncScanState, pstate);

  return ExecScan(&node->ss, (ExecScanAccessMtd)TableFuncNext, (ExecScanRecheckMtd)TableFuncRecheck);
}

                                                                    
                          
                                                                    
   
TableFuncScanState *
ExecInitTableFuncScan(TableFuncScan *node, EState *estate, int eflags)
{
  TableFuncScanState *scanstate;
  TableFunc *tf = node->tablefunc;
  TupleDesc tupdesc;
  int i;

                                   
  Assert(!(eflags & EXEC_FLAG_MARK));

     
                                                 
     
  Assert(outerPlan(node) == NULL);
  Assert(innerPlan(node) == NULL);

     
                                   
     
  scanstate = makeNode(TableFuncScanState);
  scanstate->ss.ps.plan = (Plan *)node;
  scanstate->ss.ps.state = estate;
  scanstate->ss.ps.ExecProcNode = ExecTableFuncScan;

     
                                  
     
                                        
     
  ExecAssignExprContext(estate, &scanstate->ss.ps);

     
                                  
     
  tupdesc = BuildDescFromLists(tf->colnames, tf->coltypes, tf->coltypmods, tf->colcollations);
                                       
  ExecInitScanTupleSlot(estate, &scanstate->ss, tupdesc, &TTSOpsMinimalTuple);

     
                                            
     
  ExecInitResultTypeTL(&scanstate->ss.ps);
  ExecAssignScanProjectionInfo(&scanstate->ss);

     
                                  
     
  scanstate->ss.ps.qual = ExecInitQual(node->scan.plan.qual, &scanstate->ss.ps);

                                            
  scanstate->routine = &XmlTableRoutine;

  scanstate->perTableCxt = AllocSetContextCreate(CurrentMemoryContext, "TableFunc per value context", ALLOCSET_DEFAULT_SIZES);
  scanstate->opaque = NULL;                             

  scanstate->ns_names = tf->ns_names;

  scanstate->ns_uris = ExecInitExprList(tf->ns_uris, (PlanState *)scanstate);
  scanstate->docexpr = ExecInitExpr((Expr *)tf->docexpr, (PlanState *)scanstate);
  scanstate->rowexpr = ExecInitExpr((Expr *)tf->rowexpr, (PlanState *)scanstate);
  scanstate->colexprs = ExecInitExprList(tf->colexprs, (PlanState *)scanstate);
  scanstate->coldefexprs = ExecInitExprList(tf->coldefexprs, (PlanState *)scanstate);

  scanstate->notnulls = tf->notnulls;

                                                     
  scanstate->in_functions = palloc(sizeof(FmgrInfo) * tupdesc->natts);
  scanstate->typioparams = palloc(sizeof(Oid) * tupdesc->natts);

     
                                       
     
  for (i = 0; i < tupdesc->natts; i++)
  {
    Oid in_funcid;

    getTypeInputInfo(TupleDescAttr(tupdesc, i)->atttypid, &in_funcid, &scanstate->typioparams[i]);
    fmgr_info(in_funcid, &scanstate->in_functions[i]);
  }

  return scanstate;
}

                                                                    
                         
   
                                                    
                                                                    
   
void
ExecEndTableFuncScan(TableFuncScanState *node)
{
     
                          
     
  ExecFreeExprContext(&node->ss.ps);

     
                               
     
  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecClearTuple(node->ss.ss_ScanTupleSlot);

     
                                  
     
  if (node->tupstore != NULL)
  {
    tuplestore_end(node->tupstore);
  }
  node->tupstore = NULL;
}

                                                                    
                            
   
                          
                                                                    
   
void
ExecReScanTableFuncScan(TableFuncScanState *node)
{
  Bitmapset *chgparam = node->ss.ps.chgParam;

  if (node->ss.ps.ps_ResultTupleSlot)
  {
    ExecClearTuple(node->ss.ps.ps_ResultTupleSlot);
  }
  ExecScanReScan(&node->ss);

     
                                            
     
  if (chgparam)
  {
    if (node->tupstore != NULL)
    {
      tuplestore_end(node->tupstore);
      node->tupstore = NULL;
    }
  }

  if (node->tupstore != NULL)
  {
    tuplestore_rescan(node->tupstore);
  }
}

                                                                    
                   
   
                                        
                                                                    
   
static void
tfuncFetchRows(TableFuncScanState *tstate, ExprContext *econtext)
{
  const TableFuncRoutine *routine = tstate->routine;
  MemoryContext oldcxt;
  Datum value;
  bool isnull;

  Assert(tstate->opaque == NULL);

                                       
  oldcxt = MemoryContextSwitchTo(econtext->ecxt_per_query_memory);
  tstate->tupstore = tuplestore_begin_heap(false, false, work_mem);

     
                                                                            
                                                                            
                                                                          
                                                                            
                                                                          
                                                                  
     
  MemoryContextSwitchTo(tstate->perTableCxt);

  PG_TRY();
  {
    routine->InitOpaque(tstate, tstate->ss.ss_ScanTupleSlot->tts_tupleDescriptor->natts);

       
                                                                     
                                                      
       
    value = ExecEvalExpr(tstate->docexpr, econtext, &isnull);

    if (!isnull)
    {
                                                                   
      tfuncInitialize(tstate, econtext, value);

                                         
      tstate->ordinal = 1;

                                                             
      tfuncLoadRows(tstate, econtext);
    }
  }
  PG_CATCH();
  {
    if (tstate->opaque != NULL)
    {
      routine->DestroyOpaque(tstate);
    }
    PG_RE_THROW();
  }
  PG_END_TRY();

                                                      

  if (tstate->opaque != NULL)
  {
    routine->DestroyOpaque(tstate);
    tstate->opaque = NULL;
  }

  MemoryContextSwitchTo(oldcxt);
  MemoryContextReset(tstate->perTableCxt);

  return;
}

   
                                                                           
                                     
   
static void
tfuncInitialize(TableFuncScanState *tstate, ExprContext *econtext, Datum doc)
{
  const TableFuncRoutine *routine = tstate->routine;
  TupleDesc tupdesc;
  ListCell *lc1, *lc2;
  bool isnull;
  int colno;
  Datum value;
  int ordinalitycol = ((TableFuncScan *)(tstate->ss.ps.plan))->tablefunc->ordinalitycol;

     
                                                                         
              
     
  routine->SetDocument(tstate, doc);

                                         
  forboth(lc1, tstate->ns_uris, lc2, tstate->ns_names)
  {
    ExprState *expr = (ExprState *)lfirst(lc1);
    Value *ns_node = (Value *)lfirst(lc2);
    char *ns_uri;
    char *ns_name;

    value = ExecEvalExpr((ExprState *)expr, econtext, &isnull);
    if (isnull)
    {
      ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("namespace URI must not be null")));
    }
    ns_uri = TextDatumGetCString(value);

                                                        
    ns_name = ns_node ? strVal(ns_node) : NULL;

    routine->SetNamespace(tstate, ns_name, ns_uri);
  }

                                                                        
  value = ExecEvalExpr(tstate->rowexpr, econtext, &isnull);
  if (isnull)
  {
    ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("row filter expression must not be null")));
  }

  routine->SetRowFilter(tstate, TextDatumGetCString(value));

     
                                                                           
                                                                           
                           
     
  colno = 0;
  tupdesc = tstate->ss.ss_ScanTupleSlot->tts_tupleDescriptor;
  foreach (lc1, tstate->colexprs)
  {
    char *colfilter;
    Form_pg_attribute att = TupleDescAttr(tupdesc, colno);

    if (colno != ordinalitycol)
    {
      ExprState *colexpr = lfirst(lc1);

      if (colexpr != NULL)
      {
        value = ExecEvalExpr(colexpr, econtext, &isnull);
        if (isnull)
        {
          ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("column filter expression must not be null"), errdetail("Filter for column \"%s\" is null.", NameStr(att->attname))));
        }
        colfilter = TextDatumGetCString(value);
      }
      else
      {
        colfilter = NameStr(att->attname);
      }

      routine->SetColumnFilter(tstate, colfilter, colno);
    }

    colno++;
  }
}

   
                                                                         
   
static void
tfuncLoadRows(TableFuncScanState *tstate, ExprContext *econtext)
{
  const TableFuncRoutine *routine = tstate->routine;
  TupleTableSlot *slot = tstate->ss.ss_ScanTupleSlot;
  TupleDesc tupdesc = slot->tts_tupleDescriptor;
  Datum *values = slot->tts_values;
  bool *nulls = slot->tts_isnull;
  int natts = tupdesc->natts;
  MemoryContext oldcxt;
  int ordinalitycol;

  ordinalitycol = ((TableFuncScan *)(tstate->ss.ps.plan))->tablefunc->ordinalitycol;

     
                                                                         
                                                                            
                                                                          
                       
     
  oldcxt = MemoryContextSwitchTo(econtext->ecxt_per_tuple_memory);

     
                                                                         
     
  while (routine->FetchRow(tstate))
  {
    ListCell *cell = list_head(tstate->coldefexprs);
    int colno;

    CHECK_FOR_INTERRUPTS();

    ExecClearTuple(tstate->ss.ss_ScanTupleSlot);

       
                                                                          
                                                       
       
    for (colno = 0; colno < natts; colno++)
    {
      Form_pg_attribute att = TupleDescAttr(tupdesc, colno);

      if (colno == ordinalitycol)
      {
                                             
        values[colno] = Int32GetDatum(tstate->ordinal++);
        nulls[colno] = false;
      }
      else
      {
        bool isnull;

        values[colno] = routine->GetValue(tstate, colno, att->atttypid, att->atttypmod, &isnull);

                                                               
        if (isnull && cell != NULL)
        {
          ExprState *coldefexpr = (ExprState *)lfirst(cell);

          if (coldefexpr != NULL)
          {
            values[colno] = ExecEvalExpr(coldefexpr, econtext, &isnull);
          }
        }

                                                   
        if (isnull && bms_is_member(colno, tstate->notnulls))
        {
          ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("null is not allowed in column \"%s\"", NameStr(att->attname))));
        }

        nulls[colno] = isnull;
      }

                                               
      if (cell != NULL)
      {
        cell = lnext(cell);
      }
    }

    tuplestore_putvalues(tstate->tupstore, tupdesc, values, nulls);

    MemoryContextReset(econtext->ecxt_per_tuple_memory);
  }

  MemoryContextSwitchTo(oldcxt);
}
