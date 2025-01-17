                                                                             
   
                   
                                                           
   
                                                                
   
                  
                                                   
   
                                                                             
   

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "executor/spi.h"
#include "funcapi.h"
#include "nodes/makefuncs.h"
#include "optimizer/optimizer.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

   
                                            
   
PG_FUNCTION_INFO_V1(test_predtest);

Datum
test_predtest(PG_FUNCTION_ARGS)
{
  text *txt = PG_GETARG_TEXT_PP(0);
  char *query_string = text_to_cstring(txt);
  SPIPlanPtr spiplan;
  int spirc;
  TupleDesc tupdesc;
  bool s_i_holds, w_i_holds, s_r_holds, w_r_holds;
  CachedPlan *cplan;
  PlannedStmt *stmt;
  Plan *plan;
  Expr *clause1;
  Expr *clause2;
  bool strong_implied_by, weak_implied_by, strong_refuted_by, weak_refuted_by;
  Datum values[8];
  bool nulls[8];
  int i;

                                                             
  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "SPI_connect failed");
  }

     
                                                                         
                                                                     
                                                                   
                       
     
  spiplan = SPI_prepare(query_string, 0, NULL);
  if (spiplan == NULL)
  {
    elog(ERROR, "SPI_prepare failed for \"%s\"", query_string);
  }

  spirc = SPI_execute_plan(spiplan, NULL, NULL, true, 0);
  if (spirc != SPI_OK_SELECT)
  {
    elog(ERROR, "failed to execute \"%s\"", query_string);
  }
  tupdesc = SPI_tuptable->tupdesc;
  if (tupdesc->natts != 2 || TupleDescAttr(tupdesc, 0)->atttypid != BOOLOID || TupleDescAttr(tupdesc, 1)->atttypid != BOOLOID)
  {
    elog(ERROR, "query must yield two boolean columns");
  }

  s_i_holds = w_i_holds = s_r_holds = w_r_holds = true;
  for (i = 0; i < SPI_processed; i++)
  {
    HeapTuple tup = SPI_tuptable->vals[i];
    Datum dat;
    bool isnull;
    char c1, c2;

                                                         
    dat = SPI_getbinval(tup, tupdesc, 1, &isnull);
    if (isnull)
    {
      c1 = 'n';
    }
    else if (DatumGetBool(dat))
    {
      c1 = 't';
    }
    else
    {
      c1 = 'f';
    }

    dat = SPI_getbinval(tup, tupdesc, 2, &isnull);
    if (isnull)
    {
      c2 = 'n';
    }
    else if (DatumGetBool(dat))
    {
      c2 = 't';
    }
    else
    {
      c2 = 'f';
    }

                                                          

                                                             
    if (c2 == 't' && c1 != 't')
    {
      s_i_holds = false;
    }
                                                                       
    if (c2 != 'f' && c1 == 'f')
    {
      w_i_holds = false;
    }
                                                              
    if (c2 == 't' && c1 != 'f')
    {
      s_r_holds = false;
    }
                                                              
    if (c2 == 't' && c1 == 't')
    {
      w_r_holds = false;
    }
  }

     
                                                                             
                     
     
  cplan = SPI_plan_get_cached_plan(spiplan);

  if (list_length(cplan->stmt_list) != 1)
  {
    elog(ERROR, "failed to decipher query plan");
  }
  stmt = linitial_node(PlannedStmt, cplan->stmt_list);
  if (stmt->commandType != CMD_SELECT)
  {
    elog(ERROR, "failed to decipher query plan");
  }
  plan = stmt->planTree;
  Assert(list_length(plan->targetlist) >= 2);
  clause1 = castNode(TargetEntry, linitial(plan->targetlist))->expr;
  clause2 = castNode(TargetEntry, lsecond(plan->targetlist))->expr;

     
                                                                           
                                                                     
     
                                                                        
                                                                             
                                                                           
                                                       
     
                                                                           
                                                                      
                                                                             
                                                              
     
  clause1 = (Expr *)make_ands_implicit(clause1);
  clause2 = (Expr *)make_ands_implicit(clause2);

  strong_implied_by = predicate_implied_by((List *)clause1, (List *)clause2, false);

  weak_implied_by = predicate_implied_by((List *)clause1, (List *)clause2, true);

  strong_refuted_by = predicate_refuted_by((List *)clause1, (List *)clause2, false);

  weak_refuted_by = predicate_refuted_by((List *)clause1, (List *)clause2, true);

     
                                                           
     
  if (strong_implied_by && !s_i_holds)
  {
    elog(WARNING, "strong_implied_by result is incorrect");
  }
  if (weak_implied_by && !w_i_holds)
  {
    elog(WARNING, "weak_implied_by result is incorrect");
  }
  if (strong_refuted_by && !s_r_holds)
  {
    elog(WARNING, "strong_refuted_by result is incorrect");
  }
  if (weak_refuted_by && !w_r_holds)
  {
    elog(WARNING, "weak_refuted_by result is incorrect");
  }

     
                                                  
     
  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish failed");
  }

  tupdesc = CreateTemplateTupleDesc(8);
  TupleDescInitEntry(tupdesc, (AttrNumber)1, "strong_implied_by", BOOLOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)2, "weak_implied_by", BOOLOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)3, "strong_refuted_by", BOOLOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)4, "weak_refuted_by", BOOLOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)5, "s_i_holds", BOOLOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)6, "w_i_holds", BOOLOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)7, "s_r_holds", BOOLOID, -1, 0);
  TupleDescInitEntry(tupdesc, (AttrNumber)8, "w_r_holds", BOOLOID, -1, 0);
  tupdesc = BlessTupleDesc(tupdesc);

  MemSet(nulls, 0, sizeof(nulls));
  values[0] = BoolGetDatum(strong_implied_by);
  values[1] = BoolGetDatum(weak_implied_by);
  values[2] = BoolGetDatum(strong_refuted_by);
  values[3] = BoolGetDatum(weak_refuted_by);
  values[4] = BoolGetDatum(s_i_holds);
  values[5] = BoolGetDatum(w_i_holds);
  values[6] = BoolGetDatum(s_r_holds);
  values[7] = BoolGetDatum(w_r_holds);

  PG_RETURN_DATUM(HeapTupleGetDatum(heap_form_tuple(tupdesc, values, nulls)));
}
