                                                                            
   
              
                                                                   
                                                  
   
                                                                    
                                                
   
                                                                     
                                                                      
   
                                                                         
                                                                        
   
   
                  
                                      
   
                                                                            
   

             
                                                                        
                                                                            
                                         
                                                                    
                   
                                                                       
                                                                               
                                
   
                                                                        
                                                                         
                                                                              
                                                                              
                                                            
   
                                                                         
   
                                            
                        
                      
                         
   
                                                                          
                                                  
                                                       
                                                 
                                                                      
                                                                      
                                                  
   
                                                        
   
                                                    
   
                                                                         
                                                                      
                   
   
                                                                          
                                                                       
             
   
                                            
                        
                      
                             
                                    
   
                                                              
   
                                                                          
                                                                       
                                                                             
                                                                          
                                              
   
                                                                        
                                                                            
                                                                            
                                                                               
                                                                            
                                                   
   
                                                                              
                                                                               
                                                                           
                                                                              
              
             
   

#include "postgres.h"

#include <ctype.h>
#include <math.h>

#include "access/brin.h"
#include "access/brin_page.h"
#include "access/gin.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/visibilitymap.h"
#include "catalog/pg_am.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_statistic_ext.h"
#include "executor/nodeAgg.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/plancat.h"
#include "parser/parse_clause.h"
#include "parser/parsetree.h"
#include "statistics/statistics.h"
#include "storage/bufmgr.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/index_selfuncs.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_locale.h"
#include "utils/rel.h"
#include "utils/selfuncs.h"
#include "utils/snapmgr.h"
#include "utils/spccache.h"
#include "utils/syscache.h"
#include "utils/timestamp.h"
#include "utils/typcache.h"

                                                                  
#define pull_varnos(a, b) pull_varnos_new(a, b)
#define NumRelids(a, b) NumRelids_new(a, b)

                                                            
get_relation_stats_hook_type get_relation_stats_hook = NULL;
get_index_stats_hook_type get_index_stats_hook = NULL;

static double
eqsel_internal(PG_FUNCTION_ARGS, bool negate);
static double
eqjoinsel_inner(Oid opfuncoid, Oid collation, VariableStatData *vardata1, VariableStatData *vardata2, double nd1, double nd2, bool isdefault1, bool isdefault2, AttStatsSlot *sslot1, AttStatsSlot *sslot2, Form_pg_statistic stats1, Form_pg_statistic stats2, bool have_mcvs1, bool have_mcvs2);
static double
eqjoinsel_semi(Oid opfuncoid, Oid collation, VariableStatData *vardata1, VariableStatData *vardata2, double nd1, double nd2, bool isdefault1, bool isdefault2, AttStatsSlot *sslot1, AttStatsSlot *sslot2, Form_pg_statistic stats1, Form_pg_statistic stats2, bool have_mcvs1, bool have_mcvs2, RelOptInfo *inner_rel);
static bool
estimate_multivariate_ndistinct(PlannerInfo *root, RelOptInfo *rel, List **varinfos, double *ndistinct);
static bool
convert_to_scalar(Datum value, Oid valuetypid, Oid collid, double *scaledvalue, Datum lobound, Datum hibound, Oid boundstypid, double *scaledlobound, double *scaledhibound);
static double
convert_numeric_to_scalar(Datum value, Oid typid, bool *failure);
static void
convert_string_to_scalar(char *value, double *scaledvalue, char *lobound, double *scaledlobound, char *hibound, double *scaledhibound);
static void
convert_bytea_to_scalar(Datum value, double *scaledvalue, Datum lobound, double *scaledlobound, Datum hibound, double *scaledhibound);
static double
convert_one_string_to_scalar(char *value, int rangelo, int rangehi);
static double
convert_one_bytea_to_scalar(unsigned char *value, int valuelen, int rangelo, int rangehi);
static char *
convert_string_datum(Datum value, Oid typid, Oid collid, bool *failure);
static double
convert_timevalue_to_scalar(Datum value, Oid typid, bool *failure);
static void
examine_simple_variable(PlannerInfo *root, Var *var, VariableStatData *vardata);
static bool
get_variable_range(PlannerInfo *root, VariableStatData *vardata, Oid sortop, Oid collation, Datum *min, Datum *max);
static bool
get_actual_variable_range(PlannerInfo *root, VariableStatData *vardata, Oid sortop, Oid collation, Datum *min, Datum *max);
static bool
get_actual_variable_endpoint(Relation heapRel, Relation indexRel, ScanDirection indexscandir, ScanKey scankeys, int16 typLen, bool typByVal, TupleTableSlot *tableslot, MemoryContext outercontext, Datum *endpointDatum);
static RelOptInfo *
find_join_input_rel(PlannerInfo *root, Relids relids);

   
                                                     
   
                                                                    
                                                                        
                                                                      
                                                              
   
Datum
eqsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8((float8)eqsel_internal(fcinfo, false));
}

   
                                        
   
static double
eqsel_internal(PG_FUNCTION_ARGS, bool negate)
{
  PlannerInfo *root = (PlannerInfo *)PG_GETARG_POINTER(0);
  Oid operator= PG_GETARG_OID(1);
  List *args = (List *)PG_GETARG_POINTER(2);
  int varRelid = PG_GETARG_INT32(3);
  Oid collation = PG_GET_COLLATION();
  VariableStatData vardata;
  Node *other;
  bool varonleft;
  double selec;

     
                                                                         
                                                                         
     
  if (negate)
  {
    operator= get_negator(operator);
    if (!OidIsValid(operator))
    {
                                                                       
      return 1.0 - DEFAULT_EQ_SEL;
    }
  }

     
                                                                             
                                         
     
  if (!get_restriction_variable(root, args, varRelid, &vardata, &other, &varonleft))
  {
    return negate ? (1.0 - DEFAULT_EQ_SEL) : DEFAULT_EQ_SEL;
  }

     
                                                                        
                                                                            
                    
     
  if (IsA(other, Const))
  {
    selec = var_eq_const_ext(&vardata, operator, collation, ((Const *)other)->constvalue, ((Const *)other)->constisnull, varonleft, negate);
  }
  else
  {
    selec = var_eq_non_const(&vardata, operator, other, varonleft, negate);
  }

  ReleaseVariableStats(vardata);

  return selec;
}

   
                                               
   
                                                                        
   
double
var_eq_const(VariableStatData *vardata, Oid operator, Datum constval, bool constisnull, bool varonleft, bool negate)
{
  return var_eq_const_ext(vardata, operator, DEFAULT_COLLATION_OID, constval, constisnull, varonleft, negate);
}

double
var_eq_const_ext(VariableStatData *vardata, Oid operator, Oid collation, Datum constval, bool constisnull, bool varonleft, bool negate)
{
  double selec;
  double nullfrac = 0.0;
  bool isdefault;
  Oid opfuncoid;

     
                                                                             
                                                                          
     
  if (constisnull)
  {
    return 0.0;
  }

     
                                                                     
                                   
     
  if (HeapTupleIsValid(vardata->statsTuple))
  {
    Form_pg_statistic stats;

    stats = (Form_pg_statistic)GETSTRUCT(vardata->statsTuple);
    nullfrac = stats->stanullfrac;
  }

     
                                                                        
                                                                       
                                                                            
                                                                     
                                
     
  if (vardata->isunique && vardata->rel && vardata->rel->tuples >= 1.0)
  {
    selec = 1.0 / vardata->rel->tuples;
  }
  else if (HeapTupleIsValid(vardata->statsTuple) && statistic_proc_security_check(vardata, (opfuncoid = get_opcode(operator))))
  {
    AttStatsSlot sslot;
    bool match = false;
    int i;

       
                                                                      
                                                                          
                                                                           
                                                                    
                    
       
    if (get_attstatsslot(&sslot, vardata->statsTuple, STATISTIC_KIND_MCV, InvalidOid, ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS))
    {
      FmgrInfo eqproc;

      fmgr_info(opfuncoid, &eqproc);

      for (i = 0; i < sslot.nvalues; i++)
      {
                                                           
        if (varonleft)
        {
          match = DatumGetBool(FunctionCall2Coll(&eqproc, collation, sslot.values[i], constval));
        }
        else
        {
          match = DatumGetBool(FunctionCall2Coll(&eqproc, collation, constval, sslot.values[i]));
        }
        if (match)
        {
          break;
        }
      }
    }
    else
    {
                                               
      i = 0;                          
    }

    if (match)
    {
         
                                                                    
                                                                        
         
      selec = sslot.numbers[i];
    }
    else
    {
         
                                                                       
                                                                    
               
         
      double sumcommon = 0.0;
      double otherdistinct;

      for (i = 0; i < sslot.nnumbers; i++)
      {
        sumcommon += sslot.numbers[i];
      }
      selec = 1.0 - sumcommon - nullfrac;
      CLAMP_PROBABILITY(selec);

         
                                                                         
                                                                 
                                                                       
         
      otherdistinct = get_variable_numdistinct(vardata, &isdefault) - sslot.nnumbers;
      if (otherdistinct > 1)
      {
        selec /= otherdistinct;
      }

         
                                                                         
                                                    
         
      if (sslot.nnumbers > 0 && selec > sslot.numbers[sslot.nnumbers - 1])
      {
        selec = sslot.numbers[sslot.nnumbers - 1];
      }
    }

    free_attstatsslot(&sslot);
  }
  else
  {
       
                                                                          
                                                                           
                                                                         
       
    selec = 1.0 / get_variable_numdistinct(vardata, &isdefault);
  }

                                                
  if (negate)
  {
    selec = 1.0 - selec - nullfrac;
  }

                                                   
  CLAMP_PROBABILITY(selec);

  return selec;
}

   
                                                                        
   
                                                                        
   
double
var_eq_non_const(VariableStatData *vardata, Oid operator, Node * other, bool varonleft, bool negate)
{
  double selec;
  double nullfrac = 0.0;
  bool isdefault;

     
                                      
     
  if (HeapTupleIsValid(vardata->statsTuple))
  {
    Form_pg_statistic stats;

    stats = (Form_pg_statistic)GETSTRUCT(vardata->statsTuple);
    nullfrac = stats->stanullfrac;
  }

     
                                                                        
                                                                       
                                                                            
                                                                     
                                
     
  if (vardata->isunique && vardata->rel && vardata->rel->tuples >= 1.0)
  {
    selec = 1.0 / vardata->rel->tuples;
  }
  else if (HeapTupleIsValid(vardata->statsTuple))
  {
    double ndistinct;
    AttStatsSlot sslot;

       
                                                                       
                                                                    
                                                                       
                                                                  
                                                                       
                                                                    
                                                                           
              
       
    selec = 1.0 - nullfrac;
    ndistinct = get_variable_numdistinct(vardata, &isdefault);
    if (ndistinct > 1)
    {
      selec /= ndistinct;
    }

       
                                                                           
                            
       
    if (get_attstatsslot(&sslot, vardata->statsTuple, STATISTIC_KIND_MCV, InvalidOid, ATTSTATSSLOT_NUMBERS))
    {
      if (sslot.nnumbers > 0 && selec > sslot.numbers[0])
      {
        selec = sslot.numbers[0];
      }
      free_attstatsslot(&sslot);
    }
  }
  else
  {
       
                                                                          
                                                                           
                                                                         
       
    selec = 1.0 / get_variable_numdistinct(vardata, &isdefault);
  }

                                                
  if (negate)
  {
    selec = 1.0 - selec - nullfrac;
  }

                                                   
  CLAMP_PROBABILITY(selec);

  return selec;
}

   
                                                       
   
                                                                  
                                                                 
                
   
Datum
neqsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8((float8)eqsel_internal(fcinfo, true));
}

   
                                                                     
   
                                                                        
                                                                      
   
                                                                          
                                                                           
                                                                           
                                                                        
                                                                       
   
                                                                       
                                                                  
                                                                              
                                                                     
   
static double
scalarineqsel(PlannerInfo *root, Oid operator, bool isgt, bool iseq, Oid collation, VariableStatData *vardata, Datum constval, Oid consttype)
{
  Form_pg_statistic stats;
  FmgrInfo opproc;
  double mcv_selec, hist_selec, sumcommon;
  double selec;

  if (!HeapTupleIsValid(vardata->statsTuple))
  {
       
                                                                          
                                                                        
                                                                           
       
    if (vardata->var && IsA(vardata->var, Var) && ((Var *)vardata->var)->varattno == SelfItemPointerAttributeNumber)
    {
      ItemPointer itemptr;
      double block;
      double density;

         
                                                                    
                                                         
         
      if (vardata->rel->pages == 0)
      {
        return 1.0;
      }

      itemptr = (ItemPointer)DatumGetPointer(constval);
      block = ItemPointerGetBlockNumberNoCheck(itemptr);

         
                                                                    
         
                                                                         
                                                                       
                                                    
         
      density = vardata->rel->tuples / (vardata->rel->pages - 0.5);

                                                             
      if (block >= vardata->rel->pages - 1)
      {
        density *= 0.5;
      }

         
                                                                       
                                                                        
                                                                         
                                                                         
                                                                      
                                                                   
         
      if (density > 0.0)
      {
        OffsetNumber offset = ItemPointerGetOffsetNumberNoCheck(itemptr);

        block += Min(offset / density, 1.0);
      }

         
                                                                        
                                    
         
      selec = block / (vardata->rel->pages - 0.5);

         
                                                                         
                                                                        
                                                                     
                                                                         
                                                                         
                        
         
      if (iseq == isgt && vardata->rel->tuples >= 1.0)
      {
        selec -= (1.0 / vardata->rel->tuples);
      }

                                                                     
      if (isgt)
      {
        selec = 1.0 - selec;
      }

      CLAMP_PROBABILITY(selec);
      return selec;
    }

                                               
    return DEFAULT_INEQ_SEL;
  }
  stats = (Form_pg_statistic)GETSTRUCT(vardata->statsTuple);

  fmgr_info(get_opcode(operator), &opproc);

     
                                                                         
                                                                             
                                                                            
                     
     
  mcv_selec = mcv_selectivity_ext(vardata, &opproc, collation, constval, true, &sumcommon);

     
                                                                             
                                                        
     
  hist_selec = ineq_histogram_selectivity_ext(root, vardata, &opproc, isgt, iseq, collation, constval, consttype);

     
                                                                    
                                                                           
                        
     
  selec = 1.0 - stats->stanullfrac - sumcommon;

  if (hist_selec >= 0.0)
  {
    selec *= hist_selec;
  }
  else
  {
       
                                                                      
                                                   
       
    selec *= 0.5;
  }

  selec += mcv_selec;

                                                   
  CLAMP_PROBABILITY(selec);

  return selec;
}

   
                                                                      
   
                                                                          
                                                                        
                                                                              
                                                                            
   
                                                                       
                                                                       
                            
   
double
mcv_selectivity(VariableStatData *vardata, FmgrInfo *opproc, Datum constval, bool varonleft, double *sumcommonp)
{
  return mcv_selectivity_ext(vardata, opproc, DEFAULT_COLLATION_OID, constval, varonleft, sumcommonp);
}

double
mcv_selectivity_ext(VariableStatData *vardata, FmgrInfo *opproc, Oid collation, Datum constval, bool varonleft, double *sumcommonp)
{
  double mcv_selec, sumcommon;
  AttStatsSlot sslot;
  int i;

  mcv_selec = 0.0;
  sumcommon = 0.0;

  if (HeapTupleIsValid(vardata->statsTuple) && statistic_proc_security_check(vardata, opproc->fn_oid) && get_attstatsslot(&sslot, vardata->statsTuple, STATISTIC_KIND_MCV, InvalidOid, ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS))
  {
    for (i = 0; i < sslot.nvalues; i++)
    {
      if (varonleft ? DatumGetBool(FunctionCall2Coll(opproc, collation, sslot.values[i], constval)) : DatumGetBool(FunctionCall2Coll(opproc, collation, constval, sslot.values[i])))
      {
        mcv_selec += sslot.numbers[i];
      }
      sumcommon += sslot.numbers[i];
    }
    free_attstatsslot(&sslot);
  }

  *sumcommonp = sumcommon;
  return mcv_selec;
}

   
                                                                           
   
                                                                           
                                                                  
   
                                                                             
                                                                          
                                                                              
                                                                              
                                                                              
                                                                               
                                                          
   
                                                                               
                                                                           
                                                                            
                                                                            
                                  
   
                                                                          
                                       
   
                                                                       
                                                                       
                                             
   
                                                                            
                                                                     
                                                                           
                                                                           
   
double
histogram_selectivity(VariableStatData *vardata, FmgrInfo *opproc, Datum constval, bool varonleft, int min_hist_size, int n_skip, int *hist_size)
{
  return histogram_selectivity_ext(vardata, opproc, DEFAULT_COLLATION_OID, constval, varonleft, min_hist_size, n_skip, hist_size);
}

double
histogram_selectivity_ext(VariableStatData *vardata, FmgrInfo *opproc, Oid collation, Datum constval, bool varonleft, int min_hist_size, int n_skip, int *hist_size)
{
  double result;
  AttStatsSlot sslot;

                                  
  Assert(n_skip >= 0);
  Assert(min_hist_size > 2 * n_skip);

  if (HeapTupleIsValid(vardata->statsTuple) && statistic_proc_security_check(vardata, opproc->fn_oid) && get_attstatsslot(&sslot, vardata->statsTuple, STATISTIC_KIND_HISTOGRAM, InvalidOid, ATTSTATSSLOT_VALUES))
  {
    *hist_size = sslot.nvalues;
    if (sslot.nvalues >= min_hist_size)
    {
      int nmatch = 0;
      int i;

      for (i = n_skip; i < sslot.nvalues - n_skip; i++)
      {
        if (varonleft ? DatumGetBool(FunctionCall2Coll(opproc, collation, sslot.values[i], constval)) : DatumGetBool(FunctionCall2Coll(opproc, collation, constval, sslot.values[i])))
        {
          nmatch++;
        }
      }
      result = ((double)nmatch) / ((double)(sslot.nvalues - 2 * n_skip));
    }
    else
    {
      result = -1;
    }
    free_attstatsslot(&sslot);
  }
  else
  {
    *hist_size = 0;
    result = -1;
  }

  return result;
}

   
                                                                        
   
                                                                      
                                                                       
                                                                      
   
                                                                            
   
                                                                            
                                                                     
                                                           
   
                                                                        
   
double
ineq_histogram_selectivity(PlannerInfo *root, VariableStatData *vardata, FmgrInfo *opproc, bool isgt, bool iseq, Datum constval, Oid consttype)
{
  return ineq_histogram_selectivity_ext(root, vardata, opproc, isgt, iseq, DEFAULT_COLLATION_OID, constval, consttype);
}

double
ineq_histogram_selectivity_ext(PlannerInfo *root, VariableStatData *vardata, FmgrInfo *opproc, bool isgt, bool iseq, Oid collation, Datum constval, Oid consttype)
{
  double hist_selec;
  AttStatsSlot sslot;

  hist_selec = -1.0;

     
                                                                       
                                                                           
                                                                         
                                                                           
                                                                            
                                                                         
                                                                            
                                                                           
                                                                             
                                                
     
  if (HeapTupleIsValid(vardata->statsTuple) && statistic_proc_security_check(vardata, opproc->fn_oid) && get_attstatsslot(&sslot, vardata->statsTuple, STATISTIC_KIND_HISTOGRAM, InvalidOid, ATTSTATSSLOT_VALUES))
  {
    if (sslot.nvalues > 1)
    {
         
                                                                    
                                                                         
                                                                       
                                                                         
                                                                       
                                                                      
                                                     
         
                                                                        
                                                                        
                                                                    
         
                                                                   
                                                                         
                                                               
                                                                     
                                                                         
                                                                      
                                                                     
         
      double histfrac;
      int lobound = 0;                                                
      int hibound = sslot.nvalues;                            
      bool have_end = false;

         
                                                                        
                                                                        
                                                                    
                
         
      if (sslot.nvalues == 2)
      {
        have_end = get_actual_variable_range(root, vardata, sslot.staop, collation, &sslot.values[0], &sslot.values[1]);
      }

      while (lobound < hibound)
      {
        int probe = (lobound + hibound) / 2;
        bool ltcmp;

           
                                                                      
                                                                    
                                                                
           
        if (probe == 0 && sslot.nvalues > 2)
        {
          have_end = get_actual_variable_range(root, vardata, sslot.staop, collation, &sslot.values[0], NULL);
        }
        else if (probe == sslot.nvalues - 1 && sslot.nvalues > 2)
        {
          have_end = get_actual_variable_range(root, vardata, sslot.staop, collation, NULL, &sslot.values[probe]);
        }

        ltcmp = DatumGetBool(FunctionCall2Coll(opproc, collation, sslot.values[probe], constval));
        if (isgt)
        {
          ltcmp = !ltcmp;
        }
        if (ltcmp)
        {
          lobound = probe + 1;
        }
        else
        {
          hibound = probe;
        }
      }

      if (lobound <= 0)
      {
           
                                                             
                                                                   
                                                                     
                                                                  
                                                                  
                            
           
        histfrac = 0.0;
      }
      else if (lobound >= sslot.nvalues)
      {
           
                                                                     
           
        histfrac = 1.0;
      }
      else
      {
                                                           
        int i = lobound;
        double eq_selec = 0;
        double val, high, low;
        double binfrac;

           
                                                                      
                                                                       
                                                                       
                                                                       
                                                                      
                                                                     
                                                                     
                                                                   
           
                                                                  
                                                                       
                                                                      
                                                                     
                                                            
           
        if (i == 1 || isgt == iseq)
        {
          double otherdistinct;
          bool isdefault;
          AttStatsSlot mcvslot;

                                                       
          otherdistinct = get_variable_numdistinct(vardata, &isdefault);

                                                     
          if (get_attstatsslot(&mcvslot, vardata->statsTuple, STATISTIC_KIND_MCV, InvalidOid, ATTSTATSSLOT_NUMBERS))
          {
            otherdistinct -= mcvslot.nnumbers;
            free_attstatsslot(&mcvslot);
          }

                                                                
          if (otherdistinct > 1)
          {
            eq_selec = 1.0 / otherdistinct;
          }
        }

           
                                                                 
                                                                 
                                          
           
        if (convert_to_scalar(constval, consttype, collation, &val, sslot.values[i - 1], sslot.values[i], vardata->vartype, &low, &high))
        {
          if (high <= low)
          {
                                                         
            binfrac = 0.5;
          }
          else if (val <= low)
          {
            binfrac = 0.0;
          }
          else if (val >= high)
          {
            binfrac = 1.0;
          }
          else
          {
            binfrac = (val - low) / (high - low);

               
                                                                  
                                                            
                                                                 
                             
               
            if (isnan(binfrac) || binfrac < 0.0 || binfrac > 1.0)
            {
              binfrac = 0.5;
            }
          }
        }
        else
        {
             
                                                                     
                                                           
                                                                   
                                                                   
                                                              
                                               
             
          binfrac = 0.5;
        }

           
                                                                  
                                                                    
                                                   
           
        histfrac = (double)(i - 1) + binfrac;
        histfrac /= (double)(sslot.nvalues - 1);

           
                                                                     
                                                                      
                                                                    
                                                                      
                                                                    
                                                                      
                                                                       
                                                                      
                                                                    
                                                                       
                                                                     
                                                                       
                                            
           
                                                                     
                                                                     
                                                                     
                                                                 
                                                                   
                                                                   
                                              
           
                                                                    
                                                                 
                                                                   
                                                                   
                                                                      
                                                                    
                                                                      
                                                    
           
        if (i == 1)
        {
          histfrac += eq_selec * (1.0 - binfrac);
        }

           
                                                                      
                                                                      
                                                 
           
        if (isgt == iseq)
        {
          histfrac -= eq_selec;
        }
      }

         
                                                                         
                                              
         
      hist_selec = isgt ? (1.0 - histfrac) : histfrac;

         
                                                                      
                                                                       
                                                                      
                                                                     
                                                                        
                                                                        
                     
         
      if (have_end)
      {
        CLAMP_PROBABILITY(hist_selec);
      }
      else
      {
        double cutoff = 0.01 / (double)(sslot.nvalues - 1);

        if (hist_selec < cutoff)
        {
          hist_selec = cutoff;
        }
        else if (hist_selec > 1.0 - cutoff)
        {
          hist_selec = 1.0 - cutoff;
        }
      }
    }

    free_attstatsslot(&sslot);
  }

  return hist_selec;
}

   
                                                                      
                           
   
static Datum
scalarineqsel_wrapper(PG_FUNCTION_ARGS, bool isgt, bool iseq)
{
  PlannerInfo *root = (PlannerInfo *)PG_GETARG_POINTER(0);
  Oid operator= PG_GETARG_OID(1);
  List *args = (List *)PG_GETARG_POINTER(2);
  int varRelid = PG_GETARG_INT32(3);
  Oid collation = PG_GET_COLLATION();
  VariableStatData vardata;
  Node *other;
  bool varonleft;
  Datum constval;
  Oid consttype;
  double selec;

     
                                                                          
                                              
     
  if (!get_restriction_variable(root, args, varRelid, &vardata, &other, &varonleft))
  {
    PG_RETURN_FLOAT8(DEFAULT_INEQ_SEL);
  }

     
                                                                          
     
  if (!IsA(other, Const))
  {
    ReleaseVariableStats(vardata);
    PG_RETURN_FLOAT8(DEFAULT_INEQ_SEL);
  }

     
                                                                             
                                      
     
  if (((Const *)other)->constisnull)
  {
    ReleaseVariableStats(vardata);
    PG_RETURN_FLOAT8(0.0);
  }
  constval = ((Const *)other)->constvalue;
  consttype = ((Const *)other)->consttype;

     
                                                                         
     
  if (!varonleft)
  {
    operator= get_commutator(operator);
    if (!operator)
    {
                                                                       
      ReleaseVariableStats(vardata);
      PG_RETURN_FLOAT8(DEFAULT_INEQ_SEL);
    }
    isgt = !isgt;
  }

                                                        
  selec = scalarineqsel(root, operator, isgt, iseq, collation, &vardata, constval, consttype);

  ReleaseVariableStats(vardata);

  PG_RETURN_FLOAT8((float8)selec);
}

   
                                                   
   
Datum
scalarltsel(PG_FUNCTION_ARGS)
{
  return scalarineqsel_wrapper(fcinfo, false, false);
}

   
                                                    
   
Datum
scalarlesel(PG_FUNCTION_ARGS)
{
  return scalarineqsel_wrapper(fcinfo, false, true);
}

   
                                                   
   
Datum
scalargtsel(PG_FUNCTION_ARGS)
{
  return scalarineqsel_wrapper(fcinfo, true, false);
}

   
                                                    
   
Datum
scalargesel(PG_FUNCTION_ARGS)
{
  return scalarineqsel_wrapper(fcinfo, true, true);
}

   
                                                   
   
                                                                        
                                                                             
                                                                             
                                                                 
   
Selectivity
boolvarsel(PlannerInfo *root, Node *arg, int varRelid)
{
  VariableStatData vardata;
  double selec;

  examine_variable(root, arg, varRelid, &vardata);
  if (HeapTupleIsValid(vardata.statsTuple))
  {
       
                                                                       
                                                           
       
    selec = var_eq_const_ext(&vardata, BooleanEqualOperator, InvalidOid, BoolGetDatum(true), false, true, false);
  }
  else
  {
                                                
    selec = 0.5;
  }
  ReleaseVariableStats(vardata);
  return selec;
}

   
                                                    
   
Selectivity
booltestsel(PlannerInfo *root, BoolTestType booltesttype, Node *arg, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo)
{
  VariableStatData vardata;
  double selec;

  examine_variable(root, arg, varRelid, &vardata);

  if (HeapTupleIsValid(vardata.statsTuple))
  {
    Form_pg_statistic stats;
    double freq_null;
    AttStatsSlot sslot;

    stats = (Form_pg_statistic)GETSTRUCT(vardata.statsTuple);
    freq_null = stats->stanullfrac;

    if (get_attstatsslot(&sslot, vardata.statsTuple, STATISTIC_KIND_MCV, InvalidOid, ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS) && sslot.nnumbers > 0)
    {
      double freq_true;
      double freq_false;

         
                                                                
         
      if (DatumGetBool(sslot.values[0]))
      {
        freq_true = sslot.numbers[0];
      }
      else
      {
        freq_true = 1.0 - sslot.numbers[0] - freq_null;
      }

         
                                                                        
                                            
         
      freq_false = 1.0 - freq_true - freq_null;

      switch (booltesttype)
      {
      case IS_UNKNOWN:
                                     
        selec = freq_null;
        break;
      case IS_NOT_UNKNOWN:
                                    
        selec = 1.0 - freq_null;
        break;
      case IS_TRUE:
                                     
        selec = freq_true;
        break;
      case IS_NOT_TRUE:
                                    
        selec = 1.0 - freq_true;
        break;
      case IS_FALSE:
                                      
        selec = freq_false;
        break;
      case IS_NOT_FALSE:
                                     
        selec = 1.0 - freq_false;
        break;
      default:
        elog(ERROR, "unrecognized booltesttype: %d", (int)booltesttype);
        selec = 0.0;                          
        break;
      }

      free_attstatsslot(&sslot);
    }
    else
    {
         
                                                                       
                                                                       
                                                                       
         
      switch (booltesttype)
      {
      case IS_UNKNOWN:
                                     
        selec = freq_null;
        break;
      case IS_NOT_UNKNOWN:
                                    
        selec = 1.0 - freq_null;
        break;
      case IS_TRUE:
      case IS_FALSE:
                                                          
        selec = (1.0 - freq_null) / 2.0;
        break;
      case IS_NOT_TRUE:
      case IS_NOT_FALSE:
                                                               
                                                           
        selec = (freq_null + 1.0) / 2.0;
        break;
      default:
        elog(ERROR, "unrecognized booltesttype: %d", (int)booltesttype);
        selec = 0.0;                          
        break;
      }
    }
  }
  else
  {
       
                                                                     
                                                                   
                                                                           
                                                 
       
    switch (booltesttype)
    {
    case IS_UNKNOWN:
      selec = DEFAULT_UNK_SEL;
      break;
    case IS_NOT_UNKNOWN:
      selec = DEFAULT_NOT_UNK_SEL;
      break;
    case IS_TRUE:
    case IS_NOT_FALSE:
      selec = (double)clause_selectivity(root, arg, varRelid, jointype, sjinfo);
      break;
    case IS_FALSE:
    case IS_NOT_TRUE:
      selec = 1.0 - (double)clause_selectivity(root, arg, varRelid, jointype, sjinfo);
      break;
    default:
      elog(ERROR, "unrecognized booltesttype: %d", (int)booltesttype);
      selec = 0.0;                          
      break;
    }
  }

  ReleaseVariableStats(vardata);

                                                   
  CLAMP_PROBABILITY(selec);

  return (Selectivity)selec;
}

   
                                                 
   
Selectivity
nulltestsel(PlannerInfo *root, NullTestType nulltesttype, Node *arg, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo)
{
  VariableStatData vardata;
  double selec;

  examine_variable(root, arg, varRelid, &vardata);

  if (HeapTupleIsValid(vardata.statsTuple))
  {
    Form_pg_statistic stats;
    double freq_null;

    stats = (Form_pg_statistic)GETSTRUCT(vardata.statsTuple);
    freq_null = stats->stanullfrac;

    switch (nulltesttype)
    {
    case IS_NULL:

         
                                 
         
      selec = freq_null;
      break;
    case IS_NOT_NULL:

         
                                                              
                    
         
      selec = 1.0 - freq_null;
      break;
    default:
      elog(ERROR, "unrecognized nulltesttype: %d", (int)nulltesttype);
      return (Selectivity)0;                          
    }
  }
  else if (vardata.var && IsA(vardata.var, Var) && ((Var *)vardata.var)->varattno < 0)
  {
       
                                                                         
             
       
    selec = (nulltesttype == IS_NULL) ? 0.0 : 1.0;
  }
  else
  {
       
                                                   
       
    switch (nulltesttype)
    {
    case IS_NULL:
      selec = DEFAULT_UNK_SEL;
      break;
    case IS_NOT_NULL:
      selec = DEFAULT_NOT_UNK_SEL;
      break;
    default:
      elog(ERROR, "unrecognized nulltesttype: %d", (int)nulltesttype);
      return (Selectivity)0;                          
    }
  }

  ReleaseVariableStats(vardata);

                                                   
  CLAMP_PROBABILITY(selec);

  return (Selectivity)selec;
}

   
                                                                                
   
                                                                                
                                                                            
                                                                          
                                                            
   
static Node *
strip_array_coercion(Node *node)
{
  for (;;)
  {
    if (node && IsA(node, ArrayCoerceExpr))
    {
      ArrayCoerceExpr *acoerce = (ArrayCoerceExpr *)node;

         
                                                                       
                                                                         
         
      if (IsA(acoerce->elemexpr, RelabelType) && IsA(((RelabelType *)acoerce->elemexpr)->arg, CaseTestExpr))
      {
        node = (Node *)acoerce->arg;
      }
      else
      {
        break;
      }
    }
    else if (node && IsA(node, RelabelType))
    {
                                                                  
      node = (Node *)((RelabelType *)node)->arg;
    }
    else
    {
      break;
    }
  }
  return node;
}

   
                                                             
   
Selectivity
scalararraysel(PlannerInfo *root, ScalarArrayOpExpr *clause, bool is_join_clause, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo)
{
  Oid operator= clause->opno;
  bool useOr = clause->useOr;
  bool isEquality = false;
  bool isInequality = false;
  Node *leftop;
  Node *rightop;
  Oid nominal_element_type;
  Oid nominal_element_collation;
  TypeCacheEntry *typentry;
  RegProcedure oprsel;
  FmgrInfo oprselproc;
  Selectivity s1;
  Selectivity s1disjoint;

                                         
  Assert(list_length(clause->args) == 2);
  leftop = (Node *)linitial(clause->args);
  rightop = (Node *)lsecond(clause->args);

                                                   
  leftop = estimate_expression_value(root, leftop);
  rightop = estimate_expression_value(root, rightop);

                                                              
  nominal_element_type = get_base_element_type(exprType(rightop));
  if (!OidIsValid(nominal_element_type))
  {
    return (Selectivity)0.5;                                
  }
                                                            
  nominal_element_collation = exprCollation(rightop);

                                                                
  rightop = strip_array_coercion(rightop);

     
                                                                       
                                         
     
  typentry = lookup_type_cache(nominal_element_type, TYPECACHE_EQ_OPR);
  if (OidIsValid(typentry->eq_opr))
  {
    if (operator== typentry->eq_opr)
    {
      isEquality = true;
    }
    else if (get_negator(operator) == typentry->eq_opr)
    {
      isInequality = true;
    }
  }

     
                                                                             
                                                                          
                                                                            
                                                                             
     
  if ((isEquality || isInequality) && !is_join_clause)
  {
    s1 = scalararraysel_containment(root, leftop, rightop, nominal_element_type, isEquality, useOr, varRelid);
    if (s1 >= 0.0)
    {
      return s1;
    }
  }

     
                                                                         
                     
     
  if (is_join_clause)
  {
    oprsel = get_oprjoin(operator);
  }
  else
  {
    oprsel = get_oprrest(operator);
  }
  if (!oprsel)
  {
    return (Selectivity)0.5;
  }
  fmgr_info(oprsel, &oprselproc);

     
                                                                        
                                                                            
                                                                         
                                                                             
                                                                             
                                                                        
     
  if (oprsel == F_EQSEL || oprsel == F_EQJOINSEL)
  {
    isEquality = true;
  }
  else if (oprsel == F_NEQSEL || oprsel == F_NEQJOINSEL)
  {
    isInequality = true;
  }

     
                              
     
                                                                       
                                                                           
                                                                            
     
                                                                          
                                                                    
     
                                    
     
  if (rightop && IsA(rightop, Const))
  {
    Datum arraydatum = ((Const *)rightop)->constvalue;
    bool arrayisnull = ((Const *)rightop)->constisnull;
    ArrayType *arrayval;
    int16 elmlen;
    bool elmbyval;
    char elmalign;
    int num_elems;
    Datum *elem_values;
    bool *elem_nulls;
    int i;

    if (arrayisnull)                                       
    {
      return (Selectivity)0.0;
    }
    arrayval = DatumGetArrayTypeP(arraydatum);
    get_typlenbyvalalign(ARR_ELEMTYPE(arrayval), &elmlen, &elmbyval, &elmalign);
    deconstruct_array(arrayval, ARR_ELEMTYPE(arrayval), elmlen, elmbyval, elmalign, &elem_values, &elem_nulls, &num_elems);

       
                                                                      
                                                                         
                                                                          
                                                                         
       
                                                                      
                                                                     
                                                                          
                                                                          
                                                                 
                                                                     
                                                                   
       
    s1 = s1disjoint = (useOr ? 0.0 : 1.0);

    for (i = 0; i < num_elems; i++)
    {
      List *args;
      Selectivity s2;

      args = list_make2(leftop, makeConst(nominal_element_type, -1, nominal_element_collation, elmlen, elem_values[i], elem_nulls[i], elmbyval));
      if (is_join_clause)
      {
        s2 = DatumGetFloat8(FunctionCall5Coll(&oprselproc, clause->inputcollid, PointerGetDatum(root), ObjectIdGetDatum(operator), PointerGetDatum(args), Int16GetDatum(jointype), PointerGetDatum(sjinfo)));
      }
      else
      {
        s2 = DatumGetFloat8(FunctionCall4Coll(&oprselproc, clause->inputcollid, PointerGetDatum(root), ObjectIdGetDatum(operator), PointerGetDatum(args), Int32GetDatum(varRelid)));
      }

      if (useOr)
      {
        s1 = s1 + s2 - s1 * s2;
        if (isEquality)
        {
          s1disjoint += s2;
        }
      }
      else
      {
        s1 = s1 * s2;
        if (isInequality)
        {
          s1disjoint += s2 - 1.0;
        }
      }
    }

                                                          
    if ((useOr ? isEquality : isInequality) && s1disjoint >= 0.0 && s1disjoint <= 1.0)
    {
      s1 = s1disjoint;
    }
  }
  else if (rightop && IsA(rightop, ArrayExpr) && !((ArrayExpr *)rightop)->multidims)
  {
    ArrayExpr *arrayexpr = (ArrayExpr *)rightop;
    int16 elmlen;
    bool elmbyval;
    ListCell *l;

    get_typlenbyval(arrayexpr->element_typeid, &elmlen, &elmbyval);

       
                                                                          
                                                                          
                                                                         
                                                                        
                                                                     
       
    s1 = s1disjoint = (useOr ? 0.0 : 1.0);

    foreach (l, arrayexpr->elements)
    {
      Node *elem = (Node *)lfirst(l);
      List *args;
      Selectivity s2;

         
                                                                        
                                                                       
                                                   
         
      args = list_make2(leftop, elem);
      if (is_join_clause)
      {
        s2 = DatumGetFloat8(FunctionCall5Coll(&oprselproc, clause->inputcollid, PointerGetDatum(root), ObjectIdGetDatum(operator), PointerGetDatum(args), Int16GetDatum(jointype), PointerGetDatum(sjinfo)));
      }
      else
      {
        s2 = DatumGetFloat8(FunctionCall4Coll(&oprselproc, clause->inputcollid, PointerGetDatum(root), ObjectIdGetDatum(operator), PointerGetDatum(args), Int32GetDatum(varRelid)));
      }

      if (useOr)
      {
        s1 = s1 + s2 - s1 * s2;
        if (isEquality)
        {
          s1disjoint += s2;
        }
      }
      else
      {
        s1 = s1 * s2;
        if (isInequality)
        {
          s1disjoint += s2 - 1.0;
        }
      }
    }

                                                          
    if ((useOr ? isEquality : isInequality) && s1disjoint >= 0.0 && s1disjoint <= 1.0)
    {
      s1 = s1disjoint;
    }
  }
  else
  {
    CaseTestExpr *dummyexpr;
    List *args;
    Selectivity s2;
    int i;

       
                                                                   
                                                                         
                                                      
       
    dummyexpr = makeNode(CaseTestExpr);
    dummyexpr->typeId = nominal_element_type;
    dummyexpr->typeMod = -1;
    dummyexpr->collation = clause->inputcollid;
    args = list_make2(leftop, dummyexpr);
    if (is_join_clause)
    {
      s2 = DatumGetFloat8(FunctionCall5Coll(&oprselproc, clause->inputcollid, PointerGetDatum(root), ObjectIdGetDatum(operator), PointerGetDatum(args), Int16GetDatum(jointype), PointerGetDatum(sjinfo)));
    }
    else
    {
      s2 = DatumGetFloat8(FunctionCall4Coll(&oprselproc, clause->inputcollid, PointerGetDatum(root), ObjectIdGetDatum(operator), PointerGetDatum(args), Int32GetDatum(varRelid)));
    }
    s1 = useOr ? 0.0 : 1.0;

       
                                                                       
                                                                    
                                    
       
    for (i = 0; i < 10; i++)
    {
      if (useOr)
      {
        s1 = s1 + s2 - s1 * s2;
      }
      else
      {
        s1 = s1 * s2;
      }
    }
  }

                                                   
  CLAMP_PROBABILITY(s1);

  return s1;
}

   
                                                                      
   
                                                       
   
int
estimate_array_length(Node *arrayexpr)
{
                                                                  
  arrayexpr = strip_array_coercion(arrayexpr);

  if (arrayexpr && IsA(arrayexpr, Const))
  {
    Datum arraydatum = ((Const *)arrayexpr)->constvalue;
    bool arrayisnull = ((Const *)arrayexpr)->constisnull;
    ArrayType *arrayval;

    if (arrayisnull)
    {
      return 0;
    }
    arrayval = DatumGetArrayTypeP(arraydatum);
    return ArrayGetNItems(ARR_NDIM(arrayval), ARR_DIMS(arrayval));
  }
  else if (arrayexpr && IsA(arrayexpr, ArrayExpr) && !((ArrayExpr *)arrayexpr)->multidims)
  {
    return list_length(((ArrayExpr *)arrayexpr)->elements);
  }
  else
  {
                                                   
    return 10;
  }
}

   
                                                         
   
                                                                          
                                                                           
                                                                        
                                                                     
               
   
Selectivity
rowcomparesel(PlannerInfo *root, RowCompareExpr *clause, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo)
{
  Selectivity s1;
  Oid opno = linitial_oid(clause->opnos);
  Oid inputcollid = linitial_oid(clause->inputcollids);
  List *opargs;
  bool is_join_clause;

                                                     
  opargs = list_make2(linitial(clause->largs), linitial(clause->rargs));

     
                                                                    
                                                                            
                                                     
     
  if (varRelid != 0)
  {
       
                                                                           
                              
       
    is_join_clause = false;
  }
  else if (sjinfo == NULL)
  {
       
                                                                        
                  
       
    is_join_clause = false;
  }
  else
  {
       
                                                                      
       
    is_join_clause = (NumRelids(root, (Node *)opargs) > 1);
  }

  if (is_join_clause)
  {
                                                 
    s1 = join_selectivity(root, opno, opargs, inputcollid, jointype, sjinfo);
  }
  else
  {
                                                        
    s1 = restriction_selectivity(root, opno, opargs, inputcollid, varRelid);
  }

  return s1;
}

   
                                         
   
Datum
eqjoinsel(PG_FUNCTION_ARGS)
{
  PlannerInfo *root = (PlannerInfo *)PG_GETARG_POINTER(0);
  Oid operator= PG_GETARG_OID(1);
  List *args = (List *)PG_GETARG_POINTER(2);

#ifdef NOT_USED
  JoinType jointype = (JoinType)PG_GETARG_INT16(3);
#endif
  SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)PG_GETARG_POINTER(4);
  Oid collation = PG_GET_COLLATION();
  double selec;
  double selec_inner;
  VariableStatData vardata1;
  VariableStatData vardata2;
  double nd1;
  double nd2;
  bool isdefault1;
  bool isdefault2;
  Oid opfuncoid;
  AttStatsSlot sslot1;
  AttStatsSlot sslot2;
  Form_pg_statistic stats1 = NULL;
  Form_pg_statistic stats2 = NULL;
  bool have_mcvs1 = false;
  bool have_mcvs2 = false;
  bool join_is_reversed;
  RelOptInfo *inner_rel;

  get_join_variables(root, args, sjinfo, &vardata1, &vardata2, &join_is_reversed);

  nd1 = get_variable_numdistinct(&vardata1, &isdefault1);
  nd2 = get_variable_numdistinct(&vardata2, &isdefault2);

  opfuncoid = get_opcode(operator);

  memset(&sslot1, 0, sizeof(sslot1));
  memset(&sslot2, 0, sizeof(sslot2));

  if (HeapTupleIsValid(vardata1.statsTuple))
  {
                                                                    
    stats1 = (Form_pg_statistic)GETSTRUCT(vardata1.statsTuple);
    if (statistic_proc_security_check(&vardata1, opfuncoid))
    {
      have_mcvs1 = get_attstatsslot(&sslot1, vardata1.statsTuple, STATISTIC_KIND_MCV, InvalidOid, ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS);
    }
  }

  if (HeapTupleIsValid(vardata2.statsTuple))
  {
                                                                    
    stats2 = (Form_pg_statistic)GETSTRUCT(vardata2.statsTuple);
    if (statistic_proc_security_check(&vardata2, opfuncoid))
    {
      have_mcvs2 = get_attstatsslot(&sslot2, vardata2.statsTuple, STATISTIC_KIND_MCV, InvalidOid, ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS);
    }
  }

                                                                  
  selec_inner = eqjoinsel_inner(opfuncoid, collation, &vardata1, &vardata2, nd1, nd2, isdefault1, isdefault2, &sslot1, &sslot2, stats1, stats2, have_mcvs1, have_mcvs2);

  switch (sjinfo->jointype)
  {
  case JOIN_INNER:
  case JOIN_LEFT:
  case JOIN_FULL:
    selec = selec_inner;
    break;
  case JOIN_SEMI:
  case JOIN_ANTI:

       
                                                                       
                                                                  
                                                                     
                                           
       
    inner_rel = find_join_input_rel(root, sjinfo->min_righthand);

    if (!join_is_reversed)
    {
      selec = eqjoinsel_semi(opfuncoid, collation, &vardata1, &vardata2, nd1, nd2, isdefault1, isdefault2, &sslot1, &sslot2, stats1, stats2, have_mcvs1, have_mcvs2, inner_rel);
    }
    else
    {
      Oid commop = get_commutator(operator);
      Oid commopfuncoid = OidIsValid(commop) ? get_opcode(commop) : InvalidOid;

      selec = eqjoinsel_semi(commopfuncoid, collation, &vardata2, &vardata1, nd2, nd1, isdefault2, isdefault1, &sslot2, &sslot1, stats2, stats1, have_mcvs2, have_mcvs1, inner_rel);
    }

       
                                                                    
                                                                   
                                                                      
                                                                      
                                                                      
                                                                     
                                                                    
                      
       
    selec = Min(selec, inner_rel->rows * selec_inner);
    break;
  default:
                                        
    elog(ERROR, "unrecognized join type: %d", (int)sjinfo->jointype);
    selec = 0;                          
    break;
  }

  free_attstatsslot(&sslot1);
  free_attstatsslot(&sslot2);

  ReleaseVariableStats(vardata1);
  ReleaseVariableStats(vardata2);

  CLAMP_PROBABILITY(selec);

  PG_RETURN_FLOAT8((float8)selec);
}

   
                                                       
   
                                                                        
                                                    
   
static double
eqjoinsel_inner(Oid opfuncoid, Oid collation, VariableStatData *vardata1, VariableStatData *vardata2, double nd1, double nd2, bool isdefault1, bool isdefault2, AttStatsSlot *sslot1, AttStatsSlot *sslot2, Form_pg_statistic stats1, Form_pg_statistic stats2, bool have_mcvs1, bool have_mcvs2)
{
  double selec;

  if (have_mcvs1 && have_mcvs2)
  {
       
                                                                        
                                                                        
                                                                   
                                                                           
                                                                           
                                                                        
                                                              
                                                                          
                                                                           
                                                                           
       
    FmgrInfo eqproc;
    bool *hasmatch1;
    bool *hasmatch2;
    double nullfrac1 = stats1->stanullfrac;
    double nullfrac2 = stats2->stanullfrac;
    double matchprodfreq, matchfreq1, matchfreq2, unmatchfreq1, unmatchfreq2, otherfreq1, otherfreq2, totalsel1, totalsel2;
    int i, nmatches;

    fmgr_info(opfuncoid, &eqproc);
    hasmatch1 = (bool *)palloc0(sslot1->nvalues * sizeof(bool));
    hasmatch2 = (bool *)palloc0(sslot2->nvalues * sizeof(bool));

       
                                                                         
                                                                           
                                                                          
                                               
       
    matchprodfreq = 0.0;
    nmatches = 0;
    for (i = 0; i < sslot1->nvalues; i++)
    {
      int j;

      for (j = 0; j < sslot2->nvalues; j++)
      {
        if (hasmatch2[j])
        {
          continue;
        }
        if (DatumGetBool(FunctionCall2Coll(&eqproc, collation, sslot1->values[i], sslot2->values[j])))
        {
          hasmatch1[i] = hasmatch2[j] = true;
          matchprodfreq += sslot1->numbers[i] * sslot2->numbers[j];
          nmatches++;
          break;
        }
      }
    }
    CLAMP_PROBABILITY(matchprodfreq);
                                                          
    matchfreq1 = unmatchfreq1 = 0.0;
    for (i = 0; i < sslot1->nvalues; i++)
    {
      if (hasmatch1[i])
      {
        matchfreq1 += sslot1->numbers[i];
      }
      else
      {
        unmatchfreq1 += sslot1->numbers[i];
      }
    }
    CLAMP_PROBABILITY(matchfreq1);
    CLAMP_PROBABILITY(unmatchfreq1);
    matchfreq2 = unmatchfreq2 = 0.0;
    for (i = 0; i < sslot2->nvalues; i++)
    {
      if (hasmatch2[i])
      {
        matchfreq2 += sslot2->numbers[i];
      }
      else
      {
        unmatchfreq2 += sslot2->numbers[i];
      }
    }
    CLAMP_PROBABILITY(matchfreq2);
    CLAMP_PROBABILITY(unmatchfreq2);
    pfree(hasmatch1);
    pfree(hasmatch2);

       
                                                                          
              
       
    otherfreq1 = 1.0 - nullfrac1 - matchfreq1 - unmatchfreq1;
    otherfreq2 = 1.0 - nullfrac2 - matchfreq2 - unmatchfreq2;
    CLAMP_PROBABILITY(otherfreq1);
    CLAMP_PROBABILITY(otherfreq2);

       
                                                                       
                                                                   
                                                                          
                                                                     
                                                                         
                                 
       
    totalsel1 = matchprodfreq;
    if (nd2 > sslot2->nvalues)
    {
      totalsel1 += unmatchfreq1 * otherfreq2 / (nd2 - sslot2->nvalues);
    }
    if (nd2 > nmatches)
    {
      totalsel1 += otherfreq1 * (otherfreq2 + unmatchfreq2) / (nd2 - nmatches);
    }
                                                             
    totalsel2 = matchprodfreq;
    if (nd1 > sslot1->nvalues)
    {
      totalsel2 += unmatchfreq2 * otherfreq1 / (nd1 - sslot1->nvalues);
    }
    if (nd1 > nmatches)
    {
      totalsel2 += otherfreq2 * (otherfreq1 + unmatchfreq1) / (nd1 - nmatches);
    }

       
                                                                       
                                                                           
                                                                          
                                     
       
    selec = (totalsel1 < totalsel2) ? totalsel1 : totalsel2;
  }
  else
  {
       
                                                                   
                                                                         
                                                                          
                                                                       
                                                                           
                                               
                                                                          
                                                                           
                                                                           
                                                                        
                                                                         
                                                                           
                                                                        
                                                            
       
                                                                          
                                                                         
                                           
       
    double nullfrac1 = stats1 ? stats1->stanullfrac : 0.0;
    double nullfrac2 = stats2 ? stats2->stanullfrac : 0.0;

    selec = (1.0 - nullfrac1) * (1.0 - nullfrac2);
    if (nd1 > nd2)
    {
      selec /= nd1;
    }
    else
    {
      selec /= nd2;
    }
  }

  return selec;
}

   
                                              
   
                                                                              
                                                         
                                                                            
   
static double
eqjoinsel_semi(Oid opfuncoid, Oid collation, VariableStatData *vardata1, VariableStatData *vardata2, double nd1, double nd2, bool isdefault1, bool isdefault2, AttStatsSlot *sslot1, AttStatsSlot *sslot2, Form_pg_statistic stats1, Form_pg_statistic stats2, bool have_mcvs1, bool have_mcvs2, RelOptInfo *inner_rel)
{
  double selec;

     
                                                                            
                                                                          
                                                                        
                                                                           
                                                                             
                                                                         
                                                                             
                                                                           
                                                
     
                                                                            
                                                                      
                                                         
     
                                                                             
                                                                           
                                                                             
     
  if (vardata2->rel)
  {
    if (nd2 >= vardata2->rel->rows)
    {
      nd2 = vardata2->rel->rows;
      isdefault2 = false;
    }
  }
  if (nd2 >= inner_rel->rows)
  {
    nd2 = inner_rel->rows;
    isdefault2 = false;
  }

  if (have_mcvs1 && have_mcvs2 && OidIsValid(opfuncoid))
  {
       
                                                                        
                                                                        
                                                                   
                                                                           
                                                                           
                                                                        
       
    FmgrInfo eqproc;
    bool *hasmatch1;
    bool *hasmatch2;
    double nullfrac1 = stats1->stanullfrac;
    double matchfreq1, uncertainfrac, uncertain;
    int i, nmatches, clamped_nvalues2;

       
                                                                     
                                                                        
                                                                         
                                                                         
                                                                           
       
    clamped_nvalues2 = Min(sslot2->nvalues, nd2);

    fmgr_info(opfuncoid, &eqproc);
    hasmatch1 = (bool *)palloc0(sslot1->nvalues * sizeof(bool));
    hasmatch2 = (bool *)palloc0(clamped_nvalues2 * sizeof(bool));

       
                                                                         
                                                                           
                                                                          
                                               
       
    nmatches = 0;
    for (i = 0; i < sslot1->nvalues; i++)
    {
      int j;

      for (j = 0; j < clamped_nvalues2; j++)
      {
        if (hasmatch2[j])
        {
          continue;
        }
        if (DatumGetBool(FunctionCall2Coll(&eqproc, collation, sslot1->values[i], sslot2->values[j])))
        {
          hasmatch1[i] = hasmatch2[j] = true;
          nmatches++;
          break;
        }
      }
    }
                                            
    matchfreq1 = 0.0;
    for (i = 0; i < sslot1->nvalues; i++)
    {
      if (hasmatch1[i])
      {
        matchfreq1 += sslot1->numbers[i];
      }
    }
    CLAMP_PROBABILITY(matchfreq1);
    pfree(hasmatch1);
    pfree(hasmatch2);

       
                                                                      
                                                                          
                                                                        
                                                                         
                                                                         
                                                                         
                                                                           
                                  
       
                                                                       
                                                                          
                                                                       
                      
       
    if (!isdefault1 && !isdefault2)
    {
      nd1 -= nmatches;
      nd2 -= nmatches;
      if (nd1 <= nd2 || nd2 < 0)
      {
        uncertainfrac = 1.0;
      }
      else
      {
        uncertainfrac = nd2 / nd1;
      }
    }
    else
    {
      uncertainfrac = 0.5;
    }
    uncertain = 1.0 - matchfreq1 - nullfrac1;
    CLAMP_PROBABILITY(uncertain);
    selec = matchfreq1 + uncertainfrac * uncertain;
  }
  else
  {
       
                                                                       
                         
       
    double nullfrac1 = stats1 ? stats1->stanullfrac : 0.0;

    if (!isdefault1 && !isdefault2)
    {
      if (nd1 <= nd2 || nd2 < 0)
      {
        selec = 1.0 - nullfrac1;
      }
      else
      {
        selec = (nd2 / nd1) * (1.0 - nullfrac1);
      }
    }
    else
    {
      selec = 0.5 * (1.0 - nullfrac1);
    }
  }

  return selec;
}

   
                                           
   
Datum
neqjoinsel(PG_FUNCTION_ARGS)
{
  PlannerInfo *root = (PlannerInfo *)PG_GETARG_POINTER(0);
  Oid operator= PG_GETARG_OID(1);
  List *args = (List *)PG_GETARG_POINTER(2);
  JoinType jointype = (JoinType)PG_GETARG_INT16(3);
  SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)PG_GETARG_POINTER(4);
  Oid collation = PG_GET_COLLATION();
  float8 result;

  if (jointype == JOIN_SEMI || jointype == JOIN_ANTI)
  {
       
                                                                           
                                                                          
                                                                        
                                                                          
                                                                  
                                                                      
                                                                         
       
                                                                        
                                                                          
                                                
       
                                                                       
       
    VariableStatData leftvar;
    VariableStatData rightvar;
    bool reversed;
    HeapTuple statsTuple;
    double nullfrac;

    get_join_variables(root, args, sjinfo, &leftvar, &rightvar, &reversed);
    statsTuple = reversed ? rightvar.statsTuple : leftvar.statsTuple;
    if (HeapTupleIsValid(statsTuple))
    {
      nullfrac = ((Form_pg_statistic)GETSTRUCT(statsTuple))->stanullfrac;
    }
    else
    {
      nullfrac = 0.0;
    }
    ReleaseVariableStats(leftvar);
    ReleaseVariableStats(rightvar);

    result = 1.0 - nullfrac;
  }
  else
  {
       
                                                                      
                                                               
       
    Oid eqop = get_negator(operator);

    if (eqop)
    {
      result = DatumGetFloat8(DirectFunctionCall5Coll(eqjoinsel, collation, PointerGetDatum(root), ObjectIdGetDatum(eqop), PointerGetDatum(args), Int16GetDatum(jointype), PointerGetDatum(sjinfo)));
    }
    else
    {
                                                                       
      result = DEFAULT_EQ_SEL;
    }
    result = 1.0 - result;
  }

  PG_RETURN_FLOAT8(result);
}

   
                                                          
   
Datum
scalarltjoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(DEFAULT_INEQ_SEL);
}

   
                                                           
   
Datum
scalarlejoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(DEFAULT_INEQ_SEL);
}

   
                                                          
   
Datum
scalargtjoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(DEFAULT_INEQ_SEL);
}

   
                                                           
   
Datum
scalargejoinsel(PG_FUNCTION_ARGS)
{
  PG_RETURN_FLOAT8(DEFAULT_INEQ_SEL);
}

   
                                                        
   
                                                                      
                                                                     
                                                                      
                                                                     
   
                                                                          
                                                                        
   
                                                                           
                                                                   
   
                    
                                                                         
                                                                 
                                                                       
                                                        
                                                                  
   
void
mergejoinscansel(PlannerInfo *root, Node *clause, Oid opfamily, int strategy, bool nulls_first, Selectivity *leftstart, Selectivity *leftend, Selectivity *rightstart, Selectivity *rightend)
{
  Node *left, *right;
  VariableStatData leftvar, rightvar;
  int op_strategy;
  Oid op_lefttype;
  Oid op_righttype;
  Oid opno, collation, lsortop, rsortop, lstatop, rstatop, ltop, leop, revltop, revleop;
  bool isgt;
  Datum leftmin, leftmax, rightmin, rightmax;
  double selec;

                                                            
                                                                 
  *leftstart = *rightstart = 0.0;
  *leftend = *rightend = 1.0;

                                    
  if (!is_opclause(clause))
  {
    return;                       
  }
  opno = ((OpExpr *)clause)->opno;
  collation = ((OpExpr *)clause)->inputcollid;
  left = get_leftop((Expr *)clause);
  right = get_rightop((Expr *)clause);
  if (!right)
  {
    return;                       
  }

                                     
  examine_variable(root, left, 0, &leftvar);
  examine_variable(root, right, 0, &rightvar);

                                                            
  get_op_opfamily_properties(opno, opfamily, false, &op_strategy, &op_lefttype, &op_righttype);
  Assert(op_strategy == BTEqualStrategyNumber);

     
                                                                           
                                                                       
     
                                                                            
                                                                      
     
  switch (strategy)
  {
  case BTLessStrategyNumber:
    isgt = false;
    if (op_lefttype == op_righttype)
    {
                     
      ltop = get_opfamily_member(opfamily, op_lefttype, op_righttype, BTLessStrategyNumber);
      leop = get_opfamily_member(opfamily, op_lefttype, op_righttype, BTLessEqualStrategyNumber);
      lsortop = ltop;
      rsortop = ltop;
      lstatop = lsortop;
      rstatop = rsortop;
      revltop = ltop;
      revleop = leop;
    }
    else
    {
      ltop = get_opfamily_member(opfamily, op_lefttype, op_righttype, BTLessStrategyNumber);
      leop = get_opfamily_member(opfamily, op_lefttype, op_righttype, BTLessEqualStrategyNumber);
      lsortop = get_opfamily_member(opfamily, op_lefttype, op_lefttype, BTLessStrategyNumber);
      rsortop = get_opfamily_member(opfamily, op_righttype, op_righttype, BTLessStrategyNumber);
      lstatop = lsortop;
      rstatop = rsortop;
      revltop = get_opfamily_member(opfamily, op_righttype, op_lefttype, BTLessStrategyNumber);
      revleop = get_opfamily_member(opfamily, op_righttype, op_lefttype, BTLessEqualStrategyNumber);
    }
    break;
  case BTGreaterStrategyNumber:
                               
    isgt = true;
    if (op_lefttype == op_righttype)
    {
                     
      ltop = get_opfamily_member(opfamily, op_lefttype, op_righttype, BTGreaterStrategyNumber);
      leop = get_opfamily_member(opfamily, op_lefttype, op_righttype, BTGreaterEqualStrategyNumber);
      lsortop = ltop;
      rsortop = ltop;
      lstatop = get_opfamily_member(opfamily, op_lefttype, op_lefttype, BTLessStrategyNumber);
      rstatop = lstatop;
      revltop = ltop;
      revleop = leop;
    }
    else
    {
      ltop = get_opfamily_member(opfamily, op_lefttype, op_righttype, BTGreaterStrategyNumber);
      leop = get_opfamily_member(opfamily, op_lefttype, op_righttype, BTGreaterEqualStrategyNumber);
      lsortop = get_opfamily_member(opfamily, op_lefttype, op_lefttype, BTGreaterStrategyNumber);
      rsortop = get_opfamily_member(opfamily, op_righttype, op_righttype, BTGreaterStrategyNumber);
      lstatop = get_opfamily_member(opfamily, op_lefttype, op_lefttype, BTLessStrategyNumber);
      rstatop = get_opfamily_member(opfamily, op_righttype, op_righttype, BTLessStrategyNumber);
      revltop = get_opfamily_member(opfamily, op_righttype, op_lefttype, BTGreaterStrategyNumber);
      revleop = get_opfamily_member(opfamily, op_righttype, op_lefttype, BTGreaterEqualStrategyNumber);
    }
    break;
  default:
    goto fail;                         
  }

  if (!OidIsValid(lsortop) || !OidIsValid(rsortop) || !OidIsValid(lstatop) || !OidIsValid(rstatop) || !OidIsValid(ltop) || !OidIsValid(leop) || !OidIsValid(revltop) || !OidIsValid(revleop))
  {
    goto fail;                                    
  }

                                        
  if (!isgt)
  {
    if (!get_variable_range(root, &leftvar, lstatop, collation, &leftmin, &leftmax))
    {
      goto fail;                                    
    }
    if (!get_variable_range(root, &rightvar, rstatop, collation, &rightmin, &rightmax))
    {
      goto fail;                                    
    }
  }
  else
  {
                                      
    if (!get_variable_range(root, &leftvar, lstatop, collation, &leftmax, &leftmin))
    {
      goto fail;                                    
    }
    if (!get_variable_range(root, &rightvar, rstatop, collation, &rightmax, &rightmin))
    {
      goto fail;                                    
    }
  }

     
                                                                        
                                                                        
                                                     
     
  selec = scalarineqsel(root, leop, isgt, true, collation, &leftvar, rightmax, op_righttype);
  if (selec != DEFAULT_INEQ_SEL)
  {
    *leftend = selec;
  }

                                             
  selec = scalarineqsel(root, revleop, isgt, true, collation, &rightvar, leftmax, op_lefttype);
  if (selec != DEFAULT_INEQ_SEL)
  {
    *rightend = selec;
  }

     
                                                                      
                                                                             
                                                                            
                      
     
  if (*leftend > *rightend)
  {
    *leftend = 1.0;
  }
  else if (*leftend < *rightend)
  {
    *rightend = 1.0;
  }
  else
  {
    *leftend = *rightend = 1.0;
  }

     
                                                                             
                                                                      
                                                                             
                      
     
  selec = scalarineqsel(root, ltop, isgt, false, collation, &leftvar, rightmin, op_righttype);
  if (selec != DEFAULT_INEQ_SEL)
  {
    *leftstart = selec;
  }

                                             
  selec = scalarineqsel(root, revltop, isgt, false, collation, &rightvar, leftmin, op_lefttype);
  if (selec != DEFAULT_INEQ_SEL)
  {
    *rightstart = selec;
  }

     
                                                                         
                                                                            
                                                                            
                      
     
  if (*leftstart < *rightstart)
  {
    *leftstart = 0.0;
  }
  else if (*leftstart > *rightstart)
  {
    *rightstart = 0.0;
  }
  else
  {
    *leftstart = *rightstart = 0.0;
  }

     
                                                                            
                                                                            
                                                                      
                                                                           
     
  if (nulls_first)
  {
    Form_pg_statistic stats;

    if (HeapTupleIsValid(leftvar.statsTuple))
    {
      stats = (Form_pg_statistic)GETSTRUCT(leftvar.statsTuple);
      *leftstart += stats->stanullfrac;
      CLAMP_PROBABILITY(*leftstart);
      *leftend += stats->stanullfrac;
      CLAMP_PROBABILITY(*leftend);
    }
    if (HeapTupleIsValid(rightvar.statsTuple))
    {
      stats = (Form_pg_statistic)GETSTRUCT(rightvar.statsTuple);
      *rightstart += stats->stanullfrac;
      CLAMP_PROBABILITY(*rightstart);
      *rightend += stats->stanullfrac;
      CLAMP_PROBABILITY(*rightend);
    }
  }

                                                             
  if (*leftstart >= *leftend)
  {
    *leftstart = 0.0;
    *leftend = 1.0;
  }
  if (*rightstart >= *rightend)
  {
    *rightstart = 0.0;
    *rightend = 1.0;
  }

fail:
  ReleaseVariableStats(leftvar);
  ReleaseVariableStats(rightvar);
}

   
                                                                    
                                                                          
            
   
typedef struct
{
  Node *var;                                                    
  RelOptInfo *rel;                              
  double ndistinct;                        
} GroupVarInfo;

static List *
add_unique_group_var(PlannerInfo *root, List *varinfos, Node *var, VariableStatData *vardata)
{
  GroupVarInfo *varinfo;
  double ndistinct;
  bool isdefault;
  ListCell *lc;

  ndistinct = get_variable_numdistinct(vardata, &isdefault);

                                                               
  lc = list_head(varinfos);
  while (lc)
  {
    varinfo = (GroupVarInfo *)lfirst(lc);

                                                                
    lc = lnext(lc);

                               
    if (equal(var, varinfo->var))
    {
      return varinfos;
    }

       
                                                                   
                                                        
       
    if (vardata->rel != varinfo->rel && exprs_known_equal(root, var, varinfo->var))
    {
      if (varinfo->ndistinct <= ndistinct)
      {
                                             
        return varinfos;
      }
      else
      {
                                   
        varinfos = list_delete_ptr(varinfos, varinfo);
      }
    }
  }

  varinfo = (GroupVarInfo *)palloc(sizeof(GroupVarInfo));

  varinfo->var = var;
  varinfo->rel = vardata->rel;
  varinfo->ndistinct = ndistinct;
  varinfos = lappend(varinfos, varinfo);
  return varinfos;
}

   
                                                                       
   
                                                                          
                                                                       
                
   
                                                                       
                                                                     
                                                                     
                                       
   
           
                    
                                                     
                                                                       
                
                                                                      
                       
   
                                                                          
                                                                         
                                                                        
                                                                           
                                                                               
                  
                                                                          
                                                                      
                                                                      
                                               
                                                                        
                                                                     
                                                                    
                                                                  
                                                                   
                                                                    
                                                      
                                                                     
                                                                       
                                                   
                                                                             
                                                                            
                                                                         
                                                                        
                                                                         
                                                                       
                                                              
                                                                             
                                                                        
                                                                   
                                                                       
                                                                          
                                                                           
                                                                          
                                                                         
                                                                          
                                                                           
                                                       
                                                                            
                                            
                                                                             
                                                                         
                                                                        
                                             
   
double
estimate_num_groups(PlannerInfo *root, List *groupExprs, double input_rows, List **pgset)
{
  List *varinfos = NIL;
  double srf_multiplier = 1.0;
  double numdistinct;
  ListCell *l;
  int i;

     
                                                                            
                                                                           
                                                                          
            
     
  input_rows = clamp_row_est(input_rows);

     
                                                                            
                                                                        
                                        
     
  if (groupExprs == NIL || (pgset && list_length(*pgset) < 1))
  {
    return 1.0;
  }

     
                                                                        
                                                                             
                                                                        
                                                                        
                            
     
  numdistinct = 1.0;

  i = 0;
  foreach (l, groupExprs)
  {
    Node *groupexpr = (Node *)lfirst(l);
    double this_srf_multiplier;
    VariableStatData vardata;
    List *varshere;
    ListCell *l2;

                                             
    if (pgset && !list_member_int(*pgset, i++))
    {
      continue;
    }

       
                                                                          
                                                                           
                                                                         
                                                                     
                                                                    
                                                                          
                                                                          
                                                                  
                                                               
       
    this_srf_multiplier = expression_returns_set_rows(root, groupexpr);
    if (srf_multiplier < this_srf_multiplier)
    {
      srf_multiplier = this_srf_multiplier;
    }

                                                         
    if (exprType(groupexpr) == BOOLOID)
    {
      numdistinct *= 2.0;
      continue;
    }

       
                                                                         
                                                                          
                    
       
    examine_variable(root, groupexpr, 0, &vardata);
    if (HeapTupleIsValid(vardata.statsTuple) || vardata.isunique)
    {
      varinfos = add_unique_group_var(root, varinfos, groupexpr, &vardata);
      ReleaseVariableStats(vardata);
      continue;
    }
    ReleaseVariableStats(vardata);

       
                                                                    
                                                                     
                                                                       
                                                                           
       
    varshere = pull_var_clause(groupexpr, PVC_RECURSE_AGGREGATES | PVC_RECURSE_WINDOWFUNCS | PVC_RECURSE_PLACEHOLDERS);

       
                                                                       
                                                                           
                                                                      
                               
       
    if (varshere == NIL)
    {
      if (contain_volatile_functions(groupexpr))
      {
        return input_rows;
      }
      continue;
    }

       
                                           
       
    foreach (l2, varshere)
    {
      Node *var = (Node *)lfirst(l2);

      examine_variable(root, var, 0, &vardata);
      varinfos = add_unique_group_var(root, varinfos, var, &vardata);
      ReleaseVariableStats(vardata);
    }
  }

     
                                                                          
           
     
  if (varinfos == NIL)
  {
                                                              
    numdistinct *= srf_multiplier;
                   
    numdistinct = ceil(numdistinct);
                                            
    if (numdistinct > input_rows)
    {
      numdistinct = input_rows;
    }
    if (numdistinct < 1.0)
    {
      numdistinct = 1.0;
    }
    return numdistinct;
  }

     
                                                            
     
                                                                           
                                                                          
                                                                        
                                                     
     
  do
  {
    GroupVarInfo *varinfo1 = (GroupVarInfo *)linitial(varinfos);
    RelOptInfo *rel = varinfo1->rel;
    double reldistinct = 1;
    double relmaxndistinct = reldistinct;
    int relvarcount = 0;
    List *newvarinfos = NIL;
    List *relvarinfos = NIL;

       
                                                                        
                                         
       
    relvarinfos = lcons(varinfo1, relvarinfos);
    for_each_cell(l, lnext(list_head(varinfos)))
    {
      GroupVarInfo *varinfo2 = (GroupVarInfo *)lfirst(l);

      if (varinfo2->rel == varinfo1->rel)
      {
                                     
        relvarinfos = lcons(varinfo2, relvarinfos);
      }
      else
      {
                                              
        newvarinfos = lcons(varinfo2, newvarinfos);
      }
    }

       
                                                                  
                                                                          
                                                                           
                                                                           
                                                                      
                                                                 
       
                                                                      
                                                                        
                                  
       
    while (relvarinfos)
    {
      double mvndistinct;

      if (estimate_multivariate_ndistinct(root, rel, &relvarinfos, &mvndistinct))
      {
        reldistinct *= mvndistinct;
        if (relmaxndistinct < mvndistinct)
        {
          relmaxndistinct = mvndistinct;
        }
        relvarcount++;
      }
      else
      {
        foreach (l, relvarinfos)
        {
          GroupVarInfo *varinfo2 = (GroupVarInfo *)lfirst(l);

          reldistinct *= varinfo2->ndistinct;
          if (relmaxndistinct < varinfo2->ndistinct)
          {
            relmaxndistinct = varinfo2->ndistinct;
          }
          relvarcount++;
        }

                                           
        relvarinfos = NIL;
      }
    }

       
                                                                
       
    Assert(IS_SIMPLE_REL(rel));
    if (rel->tuples > 0)
    {
         
                                                                         
                                                                         
                                                                         
                                                                    
                                                         
         
      double clamp = rel->tuples;

      if (relvarcount > 1)
      {
        clamp *= 0.1;
        if (clamp < relmaxndistinct)
        {
          clamp = relmaxndistinct;
                                                               
          if (clamp > rel->tuples)
          {
            clamp = rel->tuples;
          }
        }
      }
      if (reldistinct > clamp)
      {
        reldistinct = clamp;
      }

         
                                                                   
                                                                     
                                                                   
         
      if (reldistinct > 0 && rel->rows < rel->tuples)
      {
           
                                                                       
                                                                    
                                                              
           
                                                        
           
                                                           
           
                                                         
                                                                 
                                                        
           
                                                                      
                                  
           
                                                        
           
                                                                    
                                                                      
                                                                      
                                              
           
                                     
           
                                                                   
                                                   
                                                                  
           
                                                                      
                                                                    
                                                                    
                                            
           
        reldistinct *= (1 - pow((rel->tuples - rel->rows) / rel->tuples, rel->tuples / reldistinct));
      }
      reldistinct = clamp_row_est(reldistinct);

         
                                                   
         
      numdistinct *= reldistinct;
    }

    varinfos = newvarinfos;
  } while (varinfos != NIL);

                                                      
  numdistinct *= srf_multiplier;

                 
  numdistinct = ceil(numdistinct);

                                          
  if (numdistinct > input_rows)
  {
    numdistinct = input_rows;
  }
  if (numdistinct < 1.0)
  {
    numdistinct = 1.0;
  }

  return numdistinct;
}

   
                                                                         
                                                  
   
                                          
   
                                                                        
                                              
   
                                                                           
                                        
   
                                                                             
                                                                         
                                                                             
                                                                        
                                                  
   
                                                                           
                                                                          
                                                                          
                                                                             
                                                                         
                                                                           
                                                                          
                                                                        
                                                                           
                                                                              
                                                                              
                                                                           
                                                                               
   
                                                                             
                                                                            
                                                                          
                                                                      
   
                                                                           
                                                                            
                                                                           
                                                                             
                   
   
void
estimate_hash_bucket_stats(PlannerInfo *root, Node *hashkey, double nbuckets, Selectivity *mcv_freq, Selectivity *bucketsize_frac)
{
  VariableStatData vardata;
  double estfract, ndistinct, stanullfrac, avgfreq;
  bool isdefault;
  AttStatsSlot sslot;

  examine_variable(root, hashkey, 0, &vardata);

                                                                    
  *mcv_freq = 0.0;

  if (HeapTupleIsValid(vardata.statsTuple))
  {
    if (get_attstatsslot(&sslot, vardata.statsTuple, STATISTIC_KIND_MCV, InvalidOid, ATTSTATSSLOT_NUMBERS))
    {
         
                                                          
         
      if (sslot.nnumbers > 0)
      {
        *mcv_freq = sslot.numbers[0];
      }
      free_attstatsslot(&sslot);
    }
  }

                                     
  ndistinct = get_variable_numdistinct(&vardata, &isdefault);

     
                                                                        
                                                                    
     
  if (isdefault)
  {
    *bucketsize_frac = (Selectivity)Max(0.1, *mcv_freq);
    ReleaseVariableStats(vardata);
    return;
  }

                                  
  if (HeapTupleIsValid(vardata.statsTuple))
  {
    Form_pg_statistic stats;

    stats = (Form_pg_statistic)GETSTRUCT(vardata.statsTuple);
    stanullfrac = stats->stanullfrac;
  }
  else
  {
    stanullfrac = 0.0;
  }

                                                                    
  avgfreq = (1.0 - stanullfrac) / ndistinct;

     
                                                                          
                                                                      
                          
     
                                                                   
                                                                           
     
  if (vardata.rel && vardata.rel->tuples > 0)
  {
    ndistinct *= vardata.rel->rows / vardata.rel->tuples;
    ndistinct = clamp_row_est(ndistinct);
  }

     
                                                                          
                                                                            
                                  
     
  if (ndistinct > nbuckets)
  {
    estfract = 1.0 / nbuckets;
  }
  else
  {
    estfract = 1.0 / ndistinct;
  }

     
                                                                            
     
  if (avgfreq > 0.0 && *mcv_freq > avgfreq)
  {
    estfract *= *mcv_freq / avgfreq;
  }

     
                                                                       
                                                                             
                                                
     
  if (estfract < 1.0e-6)
  {
    estfract = 1.0e-6;
  }
  else if (estfract > 1.0)
  {
    estfract = 1.0;
  }

  *bucketsize_frac = (Selectivity)estfract;

  ReleaseVariableStats(vardata);
}

   
                              
                                                                       
                                                                      
   
                                                                       
                                                
   
                                                                           
                                                                            
                                                                               
                                         
   
double
estimate_hashagg_tablesize(Path *path, const AggClauseCosts *agg_costs, double dNumGroups)
{
  Size hashentrysize;

                                                       
  hashentrysize = MAXALIGN(path->pathtarget->width) + MAXALIGN(SizeofMinimalTupleHeader);

                                                       
  hashentrysize += agg_costs->transitionSpace;
                                        
  hashentrysize += hash_agg_entry_size(agg_costs->numAggs);

     
                                                                           
                                                                    
                                                                             
                                            
     
  return hashentrysize * dNumGroups;
}

                                                                            
   
                    
   
                                                                            
   

   
                                                                              
                                                                               
                                                                             
                                                   
   
                                                     
   
                                                               
   
static bool
estimate_multivariate_ndistinct(PlannerInfo *root, RelOptInfo *rel, List **varinfos, double *ndistinct)
{
  ListCell *lc;
  Bitmapset *attnums = NULL;
  int nmatches;
  Oid statOid = InvalidOid;
  MVNDistinct *stats;
  Bitmapset *matched = NULL;
  RangeTblEntry *rte;

                                                                    
  if (!rel->statlist)
  {
    return false;
  }

     
                                                                        
                                                                     
                                                                         
                                                                          
                                                              
     
  rte = planner_rt_fetch(rel->relid, root);
  if (rte->inh && rte->relkind != RELKIND_PARTITIONED_TABLE)
  {
    return false;
  }

                                               
  foreach (lc, *varinfos)
  {
    GroupVarInfo *varinfo = (GroupVarInfo *)lfirst(lc);
    AttrNumber attnum;

    Assert(varinfo->rel == rel);

    if (!IsA(varinfo->var, Var))
    {
      continue;
    }

    attnum = ((Var *)varinfo->var)->varattno;

    if (!AttrNumberIsForUserDefinedAttr(attnum))
    {
      continue;
    }

    attnums = bms_add_member(attnums, attnum);
  }

                                                                
  nmatches = 1;                                      
  foreach (lc, rel->statlist)
  {
    StatisticExtInfo *info = (StatisticExtInfo *)lfirst(lc);
    Bitmapset *shared;
    int nshared;

                                        
    if (info->kind != STATS_EXT_NDISTINCT)
    {
      continue;
    }

                                                                      
    shared = bms_intersect(info->keys, attnums);
    nshared = bms_num_members(shared);

       
                                                                         
                                                  
       
                                                                         
                                              
       
    if (nshared > nmatches)
    {
      statOid = info->statOid;
      nmatches = nshared;
      matched = shared;
    }
  }

                 
  if (statOid == InvalidOid)
  {
    return false;
  }
  Assert(nmatches > 1 && matched != NULL);

  stats = statext_ndistinct_load(statOid);

     
                                                                             
                                                    
     
  if (stats)
  {
    int i;
    List *newlist = NIL;
    MVNDistinctItem *item = NULL;

                                                                     
    for (i = 0; i < stats->nitems; i++)
    {
      MVNDistinctItem *tmpitem = &stats->items[i];

      if (bms_subset_compare(tmpitem->attrs, matched) == BMS_EQUAL)
      {
        item = tmpitem;
        break;
      }
    }

                                    
    if (!item)
    {
      elog(ERROR, "corrupt MVNDistinct entry");
    }

                                                                   
    foreach (lc, *varinfos)
    {
      GroupVarInfo *varinfo = (GroupVarInfo *)lfirst(lc);
      AttrNumber attnum;

      if (!IsA(varinfo->var, Var))
      {
        newlist = lappend(newlist, varinfo);
        continue;
      }

      attnum = ((Var *)varinfo->var)->varattno;

      if (AttrNumberIsForUserDefinedAttr(attnum) && bms_is_member(attnum, matched))
      {
        continue;
      }

      newlist = lappend(newlist, varinfo);
    }

    *varinfos = newlist;
    *ndistinct = item->ndistinct;
    return true;
  }

  return false;
}

   
                     
                                                                      
                                      
                                   
   
                                                                        
                           
   
                                                                  
                                                                            
                                 
   
                                                                 
                                                                     
                                                                         
   
                                                                        
                             
   
                                                                       
                                                                       
                                                                             
                                                                     
                      
   
                                                                         
                                                   
   
static bool
convert_to_scalar(Datum value, Oid valuetypid, Oid collid, double *scaledvalue, Datum lobound, Datum hibound, Oid boundstypid, double *scaledlobound, double *scaledhibound)
{
  bool failure = false;

     
                                                                      
                                                                          
                                                                          
                                                                        
                                                                             
                                    
     
                                                                           
                                                                          
                                                                           
                                                                            
                                                                          
                                                                  
                                                                     
                                                                      
                             
     
  switch (valuetypid)
  {
       
                              
       
  case BOOLOID:
  case INT2OID:
  case INT4OID:
  case INT8OID:
  case FLOAT4OID:
  case FLOAT8OID:
  case NUMERICOID:
  case OIDOID:
  case REGPROCOID:
  case REGPROCEDUREOID:
  case REGOPEROID:
  case REGOPERATOROID:
  case REGCLASSOID:
  case REGTYPEOID:
  case REGCONFIGOID:
  case REGDICTIONARYOID:
  case REGROLEOID:
  case REGNAMESPACEOID:
    *scaledvalue = convert_numeric_to_scalar(value, valuetypid, &failure);
    *scaledlobound = convert_numeric_to_scalar(lobound, boundstypid, &failure);
    *scaledhibound = convert_numeric_to_scalar(hibound, boundstypid, &failure);
    return !failure;

       
                             
       
  case CHAROID:
  case BPCHAROID:
  case VARCHAROID:
  case TEXTOID:
  case NAMEOID:
  {
    char *valstr = convert_string_datum(value, valuetypid, collid, &failure);
    char *lostr = convert_string_datum(lobound, boundstypid, collid, &failure);
    char *histr = convert_string_datum(hibound, boundstypid, collid, &failure);

       
                                                                
                                                                
                                        
       
    if (failure)
    {
      return false;
    }

    convert_string_to_scalar(valstr, scaledvalue, lostr, scaledlobound, histr, scaledhibound);
    pfree(valstr);
    pfree(lostr);
    pfree(histr);
    return true;
  }

       
                           
       
  case BYTEAOID:
  {
                                                   
    if (boundstypid != BYTEAOID)
    {
      return false;
    }
    convert_bytea_to_scalar(value, scaledvalue, lobound, scaledlobound, hibound, scaledhibound);
    return true;
  }

       
                           
       
  case TIMESTAMPOID:
  case TIMESTAMPTZOID:
  case DATEOID:
  case INTERVALOID:
  case TIMEOID:
  case TIMETZOID:
    *scaledvalue = convert_timevalue_to_scalar(value, valuetypid, &failure);
    *scaledlobound = convert_timevalue_to_scalar(lobound, boundstypid, &failure);
    *scaledhibound = convert_timevalue_to_scalar(hibound, boundstypid, &failure);
    return !failure;

       
                              
       
  case INETOID:
  case CIDROID:
  case MACADDROID:
  case MACADDR8OID:
    *scaledvalue = convert_network_to_scalar(value, valuetypid, &failure);
    *scaledlobound = convert_network_to_scalar(lobound, boundstypid, &failure);
    *scaledhibound = convert_network_to_scalar(hibound, boundstypid, &failure);
    return !failure;
  }
                                 
  *scaledvalue = *scaledlobound = *scaledhibound = 0;
  return false;
}

   
                                                            
   
                                                               
                                            
   
static double
convert_numeric_to_scalar(Datum value, Oid typid, bool *failure)
{
  switch (typid)
  {
  case BOOLOID:
    return (double)DatumGetBool(value);
  case INT2OID:
    return (double)DatumGetInt16(value);
  case INT4OID:
    return (double)DatumGetInt32(value);
  case INT8OID:
    return (double)DatumGetInt64(value);
  case FLOAT4OID:
    return (double)DatumGetFloat4(value);
  case FLOAT8OID:
    return (double)DatumGetFloat8(value);
  case NUMERICOID:
                                                                 
    return (double)DatumGetFloat8(DirectFunctionCall1(numeric_float8_no_overflow, value));
  case OIDOID:
  case REGPROCOID:
  case REGPROCEDUREOID:
  case REGOPEROID:
  case REGOPERATOROID:
  case REGCLASSOID:
  case REGTYPEOID:
  case REGCONFIGOID:
  case REGDICTIONARYOID:
  case REGROLEOID:
  case REGNAMESPACEOID:
                                          
    return (double)DatumGetObjectId(value);
  }

  *failure = true;
  return 0;
}

   
                                                                     
   
                                                                      
                                                                    
   
                                                                   
                                                                    
                                                                 
                                                                        
                                                                         
                                                                          
   
                                                                        
                                                                        
                                                                           
                                                                       
                                                                         
                                                           
   
static void
convert_string_to_scalar(char *value, double *scaledvalue, char *lobound, double *scaledlobound, char *hibound, double *scaledhibound)
{
  int rangelo, rangehi;
  char *sptr;

  rangelo = rangehi = (unsigned char)hibound[0];
  for (sptr = lobound; *sptr; sptr++)
  {
    if (rangelo > (unsigned char)*sptr)
    {
      rangelo = (unsigned char)*sptr;
    }
    if (rangehi < (unsigned char)*sptr)
    {
      rangehi = (unsigned char)*sptr;
    }
  }
  for (sptr = hibound; *sptr; sptr++)
  {
    if (rangelo > (unsigned char)*sptr)
    {
      rangelo = (unsigned char)*sptr;
    }
    if (rangehi < (unsigned char)*sptr)
    {
      rangehi = (unsigned char)*sptr;
    }
  }
                                                                         
  if (rangelo <= 'Z' && rangehi >= 'A')
  {
    if (rangelo > 'A')
    {
      rangelo = 'A';
    }
    if (rangehi < 'Z')
    {
      rangehi = 'Z';
    }
  }
                        
  if (rangelo <= 'z' && rangehi >= 'a')
  {
    if (rangelo > 'a')
    {
      rangelo = 'a';
    }
    if (rangehi < 'z')
    {
      rangehi = 'z';
    }
  }
                    
  if (rangelo <= '9' && rangehi >= '0')
  {
    if (rangelo > '0')
    {
      rangelo = '0';
    }
    if (rangehi < '9')
    {
      rangehi = '9';
    }
  }

     
                                                                         
                                                  
     
  if (rangehi - rangelo < 9)
  {
    rangelo = ' ';
    rangehi = 127;
  }

     
                                                       
     
  while (*lobound)
  {
    if (*lobound != *hibound || *lobound != *value)
    {
      break;
    }
    lobound++, hibound++, value++;
  }

     
                                    
     
  *scaledvalue = convert_one_string_to_scalar(value, rangelo, rangehi);
  *scaledlobound = convert_one_string_to_scalar(lobound, rangelo, rangehi);
  *scaledhibound = convert_one_string_to_scalar(hibound, rangelo, rangehi);
}

static double
convert_one_string_to_scalar(char *value, int rangelo, int rangehi)
{
  int slen = strlen(value);
  double num, denom, base;

  if (slen <= 0)
  {
    return 0.0;                                      
  }

     
                                                                          
                                                                       
                                                                        
                                                                            
                                                                             
                                                                            
                                    
     
  if (slen > 12)
  {
    slen = 12;
  }

                                              
  base = rangehi - rangelo + 1;
  num = 0.0;
  denom = base;
  while (slen-- > 0)
  {
    int ch = (unsigned char)*value++;

    if (ch < rangelo)
    {
      ch = rangelo - 1;
    }
    else if (ch > rangehi)
    {
      ch = rangehi + 1;
    }
    num += ((double)(ch - rangelo)) / denom;
    denom *= base;
  }

  return num;
}

   
                                                                        
   
                                                               
                                                                             
   
                                                                        
                                                                         
   
static char *
convert_string_datum(Datum value, Oid typid, Oid collid, bool *failure)
{
  char *val;

  switch (typid)
  {
  case CHAROID:
    val = (char *)palloc(2);
    val[0] = DatumGetChar(value);
    val[1] = '\0';
    break;
  case BPCHAROID:
  case VARCHAROID:
  case TEXTOID:
    val = TextDatumGetCString(value);
    break;
  case NAMEOID:
  {
    NameData *nm = (NameData *)DatumGetPointer(value);

    val = pstrdup(NameStr(*nm));
    break;
  }
  default:
    *failure = true;
    return NULL;
  }

  if (!lc_collate_is_c(collid))
  {
    char *xfrmstr;
    size_t xfrmlen;
    size_t xfrmlen2 PG_USED_FOR_ASSERTS_ONLY;

       
                                                                          
                                                
       
                                                                           
                                                                          
                                                                       
              
       
#if _MSC_VER == 1400                  

       
       
                                                                                             
       
    {
      char x[1];

      xfrmlen = strxfrm(x, val, 0);
    }
#else
    xfrmlen = strxfrm(NULL, val, 0);
#endif
#ifdef WIN32

       
                                                                         
                                                                          
                                                                 
       
    if (xfrmlen == INT_MAX)
    {
      return val;
    }
#endif
    xfrmstr = (char *)palloc(xfrmlen + 1);
    xfrmlen2 = strxfrm(xfrmstr, val, xfrmlen + 1);

       
                                                                      
                                                                      
       
    Assert(xfrmlen2 <= xfrmlen);
    pfree(val);
    val = xfrmstr;
  }

  return val;
}

   
                                                          
   
                                                                   
                                                                
   
                                                                          
                                                                       
                                                                   
                  
   
static void
convert_bytea_to_scalar(Datum value, double *scaledvalue, Datum lobound, double *scaledlobound, Datum hibound, double *scaledhibound)
{
  bytea *valuep = DatumGetByteaPP(value);
  bytea *loboundp = DatumGetByteaPP(lobound);
  bytea *hiboundp = DatumGetByteaPP(hibound);
  int rangelo, rangehi, valuelen = VARSIZE_ANY_EXHDR(valuep), loboundlen = VARSIZE_ANY_EXHDR(loboundp), hiboundlen = VARSIZE_ANY_EXHDR(hiboundp), i, minlen;
  unsigned char *valstr = (unsigned char *)VARDATA_ANY(valuep);
  unsigned char *lostr = (unsigned char *)VARDATA_ANY(loboundp);
  unsigned char *histr = (unsigned char *)VARDATA_ANY(hiboundp);

     
                                                                        
     
  rangelo = 0;
  rangehi = 255;

     
                                                       
     
  minlen = Min(Min(valuelen, loboundlen), hiboundlen);
  for (i = 0; i < minlen; i++)
  {
    if (*lostr != *histr || *lostr != *valstr)
    {
      break;
    }
    lostr++, histr++, valstr++;
    loboundlen--, hiboundlen--, valuelen--;
  }

     
                                    
     
  *scaledvalue = convert_one_bytea_to_scalar(valstr, valuelen, rangelo, rangehi);
  *scaledlobound = convert_one_bytea_to_scalar(lostr, loboundlen, rangelo, rangehi);
  *scaledhibound = convert_one_bytea_to_scalar(histr, hiboundlen, rangelo, rangehi);
}

static double
convert_one_bytea_to_scalar(unsigned char *value, int valuelen, int rangelo, int rangehi)
{
  double num, denom, base;

  if (valuelen <= 0)
  {
    return 0.0;                                      
  }

     
                                                                         
                                    
     
  if (valuelen > 10)
  {
    valuelen = 10;
  }

                                              
  base = rangehi - rangelo + 1;
  num = 0.0;
  denom = base;
  while (valuelen-- > 0)
  {
    int ch = *value++;

    if (ch < rangelo)
    {
      ch = rangelo - 1;
    }
    else if (ch > rangehi)
    {
      ch = rangehi + 1;
    }
    num += ((double)(ch - rangelo)) / denom;
    denom *= base;
  }

  return num;
}

   
                                                              
   
                                                               
                                            
   
static double
convert_timevalue_to_scalar(Datum value, Oid typid, bool *failure)
{
  switch (typid)
  {
  case TIMESTAMPOID:
    return DatumGetTimestamp(value);
  case TIMESTAMPTZOID:
    return DatumGetTimestampTz(value);
  case DATEOID:
    return date2timestamp_no_overflow(DatumGetDateADT(value));
  case INTERVALOID:
  {
    Interval *interval = DatumGetIntervalP(value);

       
                                                                
                                                          
                                                          
       
    return interval->time + interval->day * (double)USECS_PER_DAY + interval->month * ((DAYS_PER_YEAR / (double)MONTHS_PER_YEAR) * USECS_PER_DAY);
  }
  case TIMEOID:
    return DatumGetTimeADT(value);
  case TIMETZOID:
  {
    TimeTzADT *timetz = DatumGetTimeTzADTP(value);

                                 
    return (double)(timetz->time + (timetz->zone * 1000000.0));
  }
  }

  *failure = true;
  return 0;
}

   
                            
                                                                   
                                                                       
                                                                         
                                                                     
                                                                   
   
           
                          
                              
                                                             
   
                                                       
                                                                    
                                                                          
                                                                          
   
                                                              
   
                                                                              
                                                                             
   
bool
get_restriction_variable(PlannerInfo *root, List *args, int varRelid, VariableStatData *vardata, Node **other, bool *varonleft)
{
  Node *left, *right;
  VariableStatData rdata;

                                                                 
  if (list_length(args) != 2)
  {
    return false;
  }

  left = (Node *)linitial(args);
  right = (Node *)lsecond(args);

     
                                                                            
                                                   
     
  examine_variable(root, left, varRelid, vardata);
  examine_variable(root, right, varRelid, &rdata);

     
                                                          
     
  if (vardata->rel && rdata.rel == NULL)
  {
    *varonleft = true;
    *other = estimate_expression_value(root, rdata.var);
                                                            
    return true;
  }

  if (vardata->rel == NULL && rdata.rel)
  {
    *varonleft = false;
    *other = estimate_expression_value(root, vardata->var);
                                                               
    *vardata = rdata;
    return true;
  }

                                                              
  ReleaseVariableStats(*vardata);
  ReleaseVariableStats(rdata);

  return false;
}

   
                      
                                                            
                                                                   
                                                       
   
                                                                       
                                                                      
                                                                     
   
void
get_join_variables(PlannerInfo *root, List *args, SpecialJoinInfo *sjinfo, VariableStatData *vardata1, VariableStatData *vardata2, bool *join_is_reversed)
{
  Node *left, *right;

  if (list_length(args) != 2)
  {
    elog(ERROR, "join operator should take two arguments");
  }

  left = (Node *)linitial(args);
  right = (Node *)lsecond(args);

  examine_variable(root, left, 0, vardata1);
  examine_variable(root, right, 0, vardata2);

  if (vardata1->rel && bms_is_subset(vardata1->rel->relids, sjinfo->syn_righthand))
  {
    *join_is_reversed = true;                     
  }
  else if (vardata2->rel && bms_is_subset(vardata2->rel->relids, sjinfo->syn_lefthand))
  {
    *join_is_reversed = true;                     
  }
  else
  {
    *join_is_reversed = false;
  }
}

   
                    
                                                         
                                                                  
   
           
                          
                                        
                                                             
   
                                           
                                                                      
                                                                       
                                                                        
                                                                 
                                             
                                                                       
                    
                                                               
                                                                     
                                                                   
                                                                            
                                                                    
                                                          
                                                                          
                                                                      
                                                                  
                                                                      
                                                    
                                                                     
                                                             
                                     
   
                                                                          
   
void
examine_variable(PlannerInfo *root, Node *node, int varRelid, VariableStatData *vardata)
{
  Node *basenode;
  Relids varnos;
  RelOptInfo *onerel;

                                                              
  MemSet(vardata, 0, sizeof(VariableStatData));

                                               
  vardata->vartype = exprType(node);

                                                    

  if (IsA(node, RelabelType))
  {
    basenode = (Node *)((RelabelType *)node)->arg;
  }
  else
  {
    basenode = node;
  }

                                  

  if (IsA(basenode, Var) && (varRelid == 0 || varRelid == ((Var *)basenode)->varno))
  {
    Var *var = (Var *)basenode;

                                                         
    vardata->var = basenode;                                    
    vardata->rel = find_base_rel(root, var->varno);
    vardata->atttype = var->vartype;
    vardata->atttypmod = var->vartypmod;
    vardata->isunique = has_unique_index(vardata->rel, var->varattno);

                                  
    examine_simple_variable(root, var, vardata);

    return;
  }

     
                                                                   
                                                                        
                                          
     
  varnos = pull_varnos(root, basenode);

  onerel = NULL;

  switch (bms_membership(varnos))
  {
  case BMS_EMPTY_SET:
                                                           
    break;
  case BMS_SINGLETON:
    if (varRelid == 0 || bms_is_member(varRelid, varnos))
    {
      onerel = find_base_rel(root, (varRelid ? varRelid : bms_singleton_member(varnos)));
      vardata->rel = onerel;
      node = basenode;                           
    }
                                     
    break;
  case BMS_MULTIPLE:
    if (varRelid == 0)
    {
                                                     
      vardata->rel = find_join_rel(root, varnos);
      node = basenode;                           
    }
    else if (bms_is_member(varRelid, varnos))
    {
                                                        
      vardata->rel = find_base_rel(root, varRelid);
      node = basenode;                           
                                                                             
    }
                                     
    break;
  }

  bms_free(varnos);

  vardata->var = node;
  vardata->atttype = exprType(node);
  vardata->atttypmod = exprTypmod(node);

  if (onerel)
  {
       
                                                                         
                                                                  
                   
       
                                                                          
                                                                      
                                      
       
                                                                           
                                                                     
                                                     
       
    ListCell *ilist;

    foreach (ilist, onerel->indexlist)
    {
      IndexOptInfo *index = (IndexOptInfo *)lfirst(ilist);
      ListCell *indexpr_item;
      int pos;

      indexpr_item = list_head(index->indexprs);
      if (indexpr_item == NULL)
      {
        continue;                             
      }

      for (pos = 0; pos < index->ncolumns; pos++)
      {
        if (index->indexkeys[pos] == 0)
        {
          Node *indexkey;

          if (indexpr_item == NULL)
          {
            elog(ERROR, "too few entries in indexprs list");
          }
          indexkey = (Node *)lfirst(indexpr_item);
          if (indexkey && IsA(indexkey, RelabelType))
          {
            indexkey = (Node *)((RelabelType *)indexkey)->arg;
          }
          if (equal(node, indexkey))
          {
               
                                                                  
                                                
               
            if (index->unique && index->nkeycolumns == 1 && pos == 0 && (index->indpred == NIL || index->predOK))
            {
              vardata->isunique = true;
            }

               
                                                             
                                                                   
                                                                  
                                                                  
                                
               
                                                               
                                                                
               
            if (get_index_stats_hook && (*get_index_stats_hook)(root, index->indexoid, pos + 1, vardata))
            {
                 
                                                            
                                                               
                                           
                 
              if (HeapTupleIsValid(vardata->statsTuple) && !vardata->freefunc)
              {
                elog(ERROR, "no function provided to release variable stats with");
              }
            }
            else if (index->indpred == NIL)
            {
              vardata->statsTuple = SearchSysCache3(STATRELATTINH, ObjectIdGetDatum(index->indexoid), Int16GetDatum(pos + 1), BoolGetDatum(false));
              vardata->freefunc = ReleaseSysCache;

              if (HeapTupleIsValid(vardata->statsTuple))
              {
                                                            
                RangeTblEntry *rte;
                Oid userid;

                rte = planner_rt_fetch(index->rel->relid, root);
                Assert(rte->rtekind == RTE_RELATION);

                   
                                                              
                                                   
                   
                userid = rte->checkAsUser ? rte->checkAsUser : GetUserId();

                   
                                                          
                                                              
                                                         
                                                            
                                                   
                                                             
                                    
                   
                vardata->acl_ok = rte->securityQuals == NIL && (pg_class_aclcheck(rte->relid, userid, ACL_SELECT) == ACLCHECK_OK);

                   
                                                           
                                                               
                                                         
                                                             
                                                             
                                                            
                                                           
                                                             
                                                          
                                                               
                                                          
                                                              
                                                             
                                            
                   
                if (!vardata->acl_ok && root->append_rel_array != NULL)
                {
                  AppendRelInfo *appinfo;
                  Index varno = index->rel->relid;

                  appinfo = root->append_rel_array[varno];
                  while (appinfo && planner_rt_fetch(appinfo->parent_relid, root)->rtekind == RTE_RELATION)
                  {
                    varno = appinfo->parent_relid;
                    appinfo = root->append_rel_array[varno];
                  }
                  if (varno != index->rel->relid)
                  {
                                                         
                    rte = planner_rt_fetch(varno, root);
                    Assert(rte->rtekind == RTE_RELATION);

                    userid = rte->checkAsUser ? rte->checkAsUser : GetUserId();

                    vardata->acl_ok = rte->securityQuals == NIL && (pg_class_aclcheck(rte->relid, userid, ACL_SELECT) == ACLCHECK_OK);
                  }
                }
              }
              else
              {
                                                         
                vardata->acl_ok = true;
              }
            }
            if (vardata->statsTuple)
            {
              break;
            }
          }
          indexpr_item = lnext(indexpr_item);
        }
      }
      if (vardata->statsTuple)
      {
        break;
      }
    }
  }
}

   
                           
                                             
   
                                                                         
                                
   
                                                                               
   
static void
examine_simple_variable(PlannerInfo *root, Var *var, VariableStatData *vardata)
{
  RangeTblEntry *rte = root->simple_rte_array[var->varno];

  Assert(IsA(rte, RangeTblEntry));

  if (get_relation_stats_hook && (*get_relation_stats_hook)(root, rte, var->varattno, vardata))
  {
       
                                                                           
                                                      
       
    if (HeapTupleIsValid(vardata->statsTuple) && !vardata->freefunc)
    {
      elog(ERROR, "no function provided to release variable stats with");
    }
  }
  else if (rte->rtekind == RTE_RELATION)
  {
       
                                                                         
                              
       
    vardata->statsTuple = SearchSysCache3(STATRELATTINH, ObjectIdGetDatum(rte->relid), Int16GetDatum(var->varattno), BoolGetDatum(rte->inh));
    vardata->freefunc = ReleaseSysCache;

    if (HeapTupleIsValid(vardata->statsTuple))
    {
      Oid userid;

         
                                                                       
                                                                      
                                                                       
                                                                    
         
      userid = rte->checkAsUser ? rte->checkAsUser : GetUserId();

      vardata->acl_ok = rte->securityQuals == NIL && ((pg_class_aclcheck(rte->relid, userid, ACL_SELECT) == ACLCHECK_OK) || (pg_attribute_aclcheck(rte->relid, var->varattno, userid, ACL_SELECT) == ACLCHECK_OK));

         
                                                                       
                                                                  
                                                                   
                                                                     
                                                                         
                                                                        
                                                                       
                                                                   
         
      if (!vardata->acl_ok && var->varattno > 0 && root->append_rel_array != NULL)
      {
        AppendRelInfo *appinfo;
        Index varno = var->varno;
        int varattno = var->varattno;
        bool found = false;

        appinfo = root->append_rel_array[varno];

           
                                                                    
                                                             
                                                                    
                                                                       
                                  
           
        while (appinfo && planner_rt_fetch(appinfo->parent_relid, root)->rtekind == RTE_RELATION)
        {
          int parent_varattno;
          ListCell *l;

          parent_varattno = 1;
          found = false;
          foreach (l, appinfo->translated_vars)
          {
            Var *childvar = lfirst_node(Var, l);

                                                          
            if (childvar != NULL && varattno == childvar->varattno)
            {
              found = true;
              break;
            }
            parent_varattno++;
          }

          if (!found)
          {
            break;
          }

          varno = appinfo->parent_relid;
          varattno = parent_varattno;

                                                             
          appinfo = root->append_rel_array[varno];
        }

           
                                                                      
                                                                       
                           
           
        if (!found)
        {
          return;
        }

                                                                 
        rte = planner_rt_fetch(varno, root);
        Assert(rte->rtekind == RTE_RELATION);

        userid = rte->checkAsUser ? rte->checkAsUser : GetUserId();

        vardata->acl_ok = rte->securityQuals == NIL && ((pg_class_aclcheck(rte->relid, userid, ACL_SELECT) == ACLCHECK_OK) || (pg_attribute_aclcheck(rte->relid, varattno, userid, ACL_SELECT) == ACLCHECK_OK));
      }
    }
    else
    {
                                                            
      vardata->acl_ok = true;
    }
  }
  else if (rte->rtekind == RTE_SUBQUERY && !rte->inh)
  {
       
                                                                    
       
    Query *subquery = rte->subquery;
    RelOptInfo *rel;
    TargetEntry *ste;

       
                                                                          
       
    if (var->varattno == InvalidAttrNumber)
    {
      return;
    }

       
                                                                       
                                                                        
                                                                     
                                                                    
                                                                           
                                           
       
    if (subquery->setOperations || subquery->groupClause || subquery->groupingSets)
    {
      return;
    }

       
                                                                         
                                                                           
                                                                        
                                                                        
                   
       
    rel = find_base_rel(root, var->varno);

                                                                  
    if (rel->subroot == NULL)
    {
      return;
    }
    Assert(IsA(rel->subroot, PlannerInfo));

       
                                                                          
                                                                         
                                                                   
                                                                          
                                                                           
                                                
       
    subquery = rel->subroot->parse;
    Assert(IsA(subquery, Query));

                                                                        
    ste = get_tle_by_resno(subquery->targetList, var->varattno);
    if (ste == NULL || ste->resjunk)
    {
      elog(ERROR, "subquery %s does not have attribute %d", rte->eref->aliasname, var->varattno);
    }
    var = (Var *)ste->expr;

       
                                                                         
                                                                           
                                                                        
                                        
       
    if (subquery->distinctClause)
    {
      if (list_length(subquery->distinctClause) == 1 && targetIsInSortList(ste, InvalidOid, subquery->distinctClause))
      {
        vardata->isunique = true;
      }
                             
      return;
    }

       
                                                                         
                                                                           
                                                                        
                  
       
                                                                   
                                                                          
                                                                        
                                                                     
                                                                         
                                             
       
    if (rte->security_barrier)
    {
      return;
    }

                                                                
    if (var && IsA(var, Var) && var->varlevelsup == 0)
    {
         
                                                                        
                                                                   
                                                                       
                                                                   
                                                                  
         
      examine_simple_variable(rel->subroot, var, vardata);
    }
  }
  else
  {
       
                                                                          
                                                                         
                                                                         
                                                                 
       
  }
}

   
                                                                      
                                                                              
                                                                            
                                      
   
bool
statistic_proc_security_check(VariableStatData *vardata, Oid func_oid)
{
  if (vardata->acl_ok)
  {
    return true;
  }

  if (!OidIsValid(func_oid))
  {
    return false;
  }

  if (get_func_leakproof(func_oid))
  {
    return true;
  }

  ereport(DEBUG2, (errmsg_internal("not using statistics because function \"%s\" is not leak-proof", get_func_name(func_oid))));
  return false;
}

   
                            
                                                           
   
                                        
                                                                           
                        
   
                                                                           
                                                                      
   
double
get_variable_numdistinct(VariableStatData *vardata, bool *isdefault)
{
  double stadistinct;
  double stanullfrac = 0.0;
  double ntuples;

  *isdefault = false;

     
                                                                           
                                                                            
                                                                            
                                                                
     
  if (HeapTupleIsValid(vardata->statsTuple))
  {
                                    
    Form_pg_statistic stats;

    stats = (Form_pg_statistic)GETSTRUCT(vardata->statsTuple);
    stadistinct = stats->stadistinct;
    stanullfrac = stats->stanullfrac;
  }
  else if (vardata->vartype == BOOLOID)
  {
       
                                                                      
       
                                                                         
            
       
    stadistinct = 2.0;
  }
  else if (vardata->rel && vardata->rel->rtekind == RTE_VALUES)
  {
       
                                                                           
                                                                         
                                                                         
                                                                        
                                                                           
       
    stadistinct = -1.0;                                
  }
  else
  {
       
                                                                         
                                      
       
    if (vardata->var && IsA(vardata->var, Var))
    {
      switch (((Var *)vardata->var)->varattno)
      {
      case SelfItemPointerAttributeNumber:
        stadistinct = -1.0;                                
        break;
      case TableOidAttributeNumber:
        stadistinct = 1.0;                   
        break;
      default:
        stadistinct = 0.0;                      
        break;
      }
    }
    else
    {
      stadistinct = 0.0;                      
    }

       
                                                              
       
  }

     
                                                                            
                                                                            
                                                                            
                                                                           
                                  
     
  if (vardata->isunique)
  {
    stadistinct = -1.0 * (1.0 - stanullfrac);
  }

     
                                               
     
  if (stadistinct > 0.0)
  {
    return clamp_row_est(stadistinct);
  }

     
                                                                        
     
  if (vardata->rel == NULL)
  {
    *isdefault = true;
    return DEFAULT_NUM_DISTINCT;
  }
  ntuples = vardata->rel->tuples;
  if (ntuples <= 0.0)
  {
    *isdefault = true;
    return DEFAULT_NUM_DISTINCT;
  }

     
                                              
     
  if (stadistinct < 0.0)
  {
    return clamp_row_est(-stadistinct * ntuples);
  }

     
                                                                            
                                                                            
                                            
     
  if (ntuples < DEFAULT_NUM_DISTINCT)
  {
    return clamp_row_est(ntuples);
  }

  *isdefault = true;
  return DEFAULT_NUM_DISTINCT;
}

   
                      
                                                                      
                                                                   
                                        
   
                                                                        
                                                                             
                                        
   
static bool
get_variable_range(PlannerInfo *root, VariableStatData *vardata, Oid sortop, Oid collation, Datum *min, Datum *max)
{
  Datum tmin = 0;
  Datum tmax = 0;
  bool have_data = false;
  int16 typLen;
  bool typByVal;
  Oid opfuncoid;
  AttStatsSlot sslot;
  int i;

     
                                                                            
                                                                             
                                                                         
                                                                         
                           
     
#ifdef NOT_USED
  if (get_actual_variable_range(root, vardata, sortop, collation, min, max))
  {
    return true;
  }
#endif

  if (!HeapTupleIsValid(vardata->statsTuple))
  {
                                               
    return false;
  }

     
                                                                    
                                                                        
                                                                       
                                                                             
                                                             
     
  if (!statistic_proc_security_check(vardata, (opfuncoid = get_opcode(sortop))))
  {
    return false;
  }

  get_typlenbyval(vardata->atttype, &typLen, &typByVal);

     
                                                              
     
                                                                          
                                                                         
                                       
     
  if (get_attstatsslot(&sslot, vardata->statsTuple, STATISTIC_KIND_HISTOGRAM, sortop, ATTSTATSSLOT_VALUES))
  {
    if (sslot.nvalues > 0)
    {
      tmin = datumCopy(sslot.values[0], typByVal, typLen);
      tmax = datumCopy(sslot.values[sslot.nvalues - 1], typByVal, typLen);
      have_data = true;
    }
    free_attstatsslot(&sslot);
  }
  else if (get_attstatsslot(&sslot, vardata->statsTuple, STATISTIC_KIND_HISTOGRAM, InvalidOid, 0))
  {
    free_attstatsslot(&sslot);
    return false;
  }

     
                                                                         
                                                                           
                                                                            
                                                                          
                                                                          
                      
     
  if (get_attstatsslot(&sslot, vardata->statsTuple, STATISTIC_KIND_MCV, InvalidOid, have_data ? ATTSTATSSLOT_VALUES : (ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS)))
  {
    bool use_mcvs = have_data;

    if (!have_data)
    {
      double sumcommon = 0.0;
      double nullfrac;
      int i;

      for (i = 0; i < sslot.nnumbers; i++)
      {
        sumcommon += sslot.numbers[i];
      }
      nullfrac = ((Form_pg_statistic)GETSTRUCT(vardata->statsTuple))->stanullfrac;
      if (sumcommon + nullfrac > 0.99999)
      {
        use_mcvs = true;
      }
    }

    if (use_mcvs)
    {
         
                                                                   
                                   
         
      bool tmin_is_mcv = false;
      bool tmax_is_mcv = false;
      FmgrInfo opproc;

      fmgr_info(opfuncoid, &opproc);

      for (i = 0; i < sslot.nvalues; i++)
      {
        if (!have_data)
        {
          tmin = tmax = sslot.values[i];
          tmin_is_mcv = tmax_is_mcv = have_data = true;
          continue;
        }
        if (DatumGetBool(FunctionCall2Coll(&opproc, collation, sslot.values[i], tmin)))
        {
          tmin = sslot.values[i];
          tmin_is_mcv = true;
        }
        if (DatumGetBool(FunctionCall2Coll(&opproc, collation, tmax, sslot.values[i])))
        {
          tmax = sslot.values[i];
          tmax_is_mcv = true;
        }
      }
      if (tmin_is_mcv)
      {
        tmin = datumCopy(tmin, typByVal, typLen);
      }
      if (tmax_is_mcv)
      {
        tmax = datumCopy(tmax, typByVal, typLen);
      }
    }

    free_attstatsslot(&sslot);
  }

  *min = tmin;
  *max = tmax;
  return have_data;
}

   
                             
                                                                    
                                                                     
                                             
                                                                   
                                                                
                                   
   
                                                 
                                        
   
static bool
get_actual_variable_range(PlannerInfo *root, VariableStatData *vardata, Oid sortop, Oid collation, Datum *min, Datum *max)
{
  bool have_data = false;
  RelOptInfo *rel = vardata->rel;
  RangeTblEntry *rte;
  ListCell *lc;

                                                         
  if (rel == NULL || rel->indexlist == NIL)
  {
    return false;
  }
                                                     
  rte = root->simple_rte_array[rel->relid];
  Assert(rte->rtekind == RTE_RELATION);

                                                                  
  foreach (lc, rel->indexlist)
  {
    IndexOptInfo *index = (IndexOptInfo *)lfirst(lc);
    ScanDirection indexscandir;

                                  
    if (index->relam != BTREE_AM_OID)
    {
      continue;
    }

       
                                                                           
                 
       
    if (index->indpred != NIL)
    {
      continue;
    }

       
                                                                       
                                                            
       
    if (index->hypothetical)
    {
      continue;
    }

       
                                                                           
                                                              
       
    if (collation != index->indexcollations[0])
    {
      continue;                                      
    }
    if (!match_index_to_operand(vardata->var, 0, index))
    {
      continue;
    }
    switch (get_op_opfamily_strategy(sortop, index->sortopfamily[0]))
    {
    case BTLessStrategyNumber:
      if (index->reverse_sort[0])
      {
        indexscandir = BackwardScanDirection;
      }
      else
      {
        indexscandir = ForwardScanDirection;
      }
      break;
    case BTGreaterStrategyNumber:
      if (index->reverse_sort[0])
      {
        indexscandir = ForwardScanDirection;
      }
      else
      {
        indexscandir = BackwardScanDirection;
      }
      break;
    default:
                                          
      continue;
    }

       
                                                                           
                                                                        
       
    {
      MemoryContext tmpcontext;
      MemoryContext oldcontext;
      Relation heapRel;
      Relation indexRel;
      TupleTableSlot *slot;
      int16 typLen;
      bool typByVal;
      ScanKeyData scankeys[1];

                                                             
      tmpcontext = AllocSetContextCreate(CurrentMemoryContext, "get_actual_variable_range workspace", ALLOCSET_DEFAULT_SIZES);
      oldcontext = MemoryContextSwitchTo(tmpcontext);

         
                                                                       
                                                 
         
      heapRel = table_open(rte->relid, NoLock);
      indexRel = index_open(index->indexoid, NoLock);

                                                           
      slot = table_slot_create(heapRel, NULL);
      get_typlenbyval(vardata->atttype, &typLen, &typByVal);

                                                                  
      ScanKeyEntryInitialize(&scankeys[0], SK_ISNULL | SK_SEARCHNOTNULL, 1,                        
          InvalidStrategy,                                                                   
          InvalidOid,                                                                                
          InvalidOid,                                                                         
          InvalidOid,                                                                                 
          (Datum)0);                                                                      

                                   
      if (min)
      {
        have_data = get_actual_variable_endpoint(heapRel, indexRel, indexscandir, scankeys, typLen, typByVal, slot, oldcontext, min);
      }
      else
      {
                                                           
        have_data = true;
      }

                                                               
      if (max && have_data)
      {
                                                                  
        have_data = get_actual_variable_endpoint(heapRel, indexRel, -indexscandir, scankeys, typLen, typByVal, slot, oldcontext, max);
      }

                               
      ExecDropSingleTupleTableSlot(slot);

      index_close(indexRel, NoLock);
      table_close(heapRel, NoLock);

      MemoryContextSwitchTo(oldcontext);
      MemoryContextDelete(tmpcontext);

                          
      break;
    }
  }

  return have_data;
}

   
                                                                          
                                                              
                                                                           
                  
   
                                                                 
                                                                      
                                                                      
                      
                                                                              
                                                                         
   
                                                                         
                                             
   
static bool
get_actual_variable_endpoint(Relation heapRel, Relation indexRel, ScanDirection indexscandir, ScanKey scankeys, int16 typLen, bool typByVal, TupleTableSlot *tableslot, MemoryContext outercontext, Datum *endpointDatum)
{
  bool have_data = false;
  SnapshotData SnapshotNonVacuumable;
  IndexScanDesc index_scan;
  Buffer vmbuffer = InvalidBuffer;
  BlockNumber last_heap_block = InvalidBlockNumber;
  int n_visited_heap_pages = 0;
  ItemPointer tid;
  Datum values[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];
  MemoryContext oldcontext;

     
                                                                        
                                                                          
                                                                            
     
                                                                    
                                                                           
                                                                             
                                                                        
                                                                            
                                                                     
                                                                            
                                                                        
                                                                            
                                                                            
                                                              
     
                                                              
                                                                           
                                                                           
                                                                            
                                                                            
                                                                             
                                                                           
                                                                          
                                                                       
     
                                                                        
                                                                          
                                                                          
                                                                           
                                                                            
                                                                           
                                                                            
                                                                           
                                        
     
                                                                          
                                                                            
                                                                             
                                                                        
                                                          
     
  InitNonVacuumableSnapshot(SnapshotNonVacuumable, RecentGlobalXmin);

  index_scan = index_beginscan(heapRel, indexRel, &SnapshotNonVacuumable, 1, 0);
                                     
  index_scan->xs_want_itup = true;
  index_rescan(index_scan, scankeys, 1, NULL, 0);

                                                     
  while ((tid = index_getnext_tid(index_scan, indexscandir)) != NULL)
  {
    BlockNumber block = ItemPointerGetBlockNumber(tid);

    if (!VM_ALL_VISIBLE(heapRel, block, &vmbuffer))
    {
                                                               
      if (!index_fetch_heap(index_scan, tableslot))
      {
           
                                                                
                                                                   
                                                            
           
                                                                      
                                                                     
                                                                     
                                                                 
           
#define VISITED_PAGES_LIMIT 100

        if (block != last_heap_block)
        {
          last_heap_block = block;
          n_visited_heap_pages++;
          if (n_visited_heap_pages > VISITED_PAGES_LIMIT)
          {
            break;
          }
        }

        continue;                                             
      }

                                                              
      ExecClearTuple(tableslot);

         
                                                                      
                                                                
         
    }

       
                                                                         
                                       
       
    if (!index_scan->xs_itup)
    {
      elog(ERROR, "no data returned for index-only scan");
    }
    if (index_scan->xs_recheck)
    {
      elog(ERROR, "unexpected recheck indication from btree");
    }

                                           
    index_deform_tuple(index_scan->xs_itup, index_scan->xs_itupdesc, values, isnull);

                                                   
    if (isnull[0])
    {
      elog(ERROR, "found unexpected null value in index \"%s\"", RelationGetRelationName(indexRel));
    }

                                                             
    oldcontext = MemoryContextSwitchTo(outercontext);
    *endpointDatum = datumCopy(values[0], typByVal, typLen);
    MemoryContextSwitchTo(oldcontext);
    have_data = true;
    break;
  }

  if (vmbuffer != InvalidBuffer)
  {
    ReleaseBuffer(vmbuffer);
  }
  index_endscan(index_scan);

  return have_data;
}

   
                       
                                           
   
                                                                             
            
   
static RelOptInfo *
find_join_input_rel(PlannerInfo *root, Relids relids)
{
  RelOptInfo *rel = NULL;

  switch (bms_membership(relids))
  {
  case BMS_EMPTY_SET:
                           
    break;
  case BMS_SINGLETON:
    rel = find_base_rel(root, bms_singleton_member(relids));
    break;
  case BMS_MULTIPLE:
    rel = find_join_rel(root, relids);
    break;
  }

  if (rel == NULL)
  {
    elog(ERROR, "could not find RelOptInfo for given relids");
  }

  return rel;
}

                                                                            
   
                                   
   
                                                                            
   

   
                                                                             
   
List *
get_quals_from_indexclauses(List *indexclauses)
{
  List *result = NIL;
  ListCell *lc;

  foreach (lc, indexclauses)
  {
    IndexClause *iclause = lfirst_node(IndexClause, lc);
    ListCell *lc2;

    foreach (lc2, iclause->indexquals)
    {
      RestrictInfo *rinfo = lfirst_node(RestrictInfo, lc2);

      result = lappend(result, rinfo);
    }
  }
  return result;
}

   
                                                                          
                                                                          
                                                                            
   
                                                                           
                                                                            
                                                               
   
Cost
index_other_operands_eval_cost(PlannerInfo *root, List *indexquals)
{
  Cost qual_arg_cost = 0;
  ListCell *lc;

  foreach (lc, indexquals)
  {
    Expr *clause = (Expr *)lfirst(lc);
    Node *other_operand;
    QualCost index_qual_cost;

       
                                                                       
                                        
       
    if (IsA(clause, RestrictInfo))
    {
      clause = ((RestrictInfo *)clause)->clause;
    }

    if (IsA(clause, OpExpr))
    {
      OpExpr *op = (OpExpr *)clause;

      other_operand = (Node *)lsecond(op->args);
    }
    else if (IsA(clause, RowCompareExpr))
    {
      RowCompareExpr *rc = (RowCompareExpr *)clause;

      other_operand = (Node *)rc->rargs;
    }
    else if (IsA(clause, ScalarArrayOpExpr))
    {
      ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *)clause;

      other_operand = (Node *)lsecond(saop->args);
    }
    else if (IsA(clause, NullTest))
    {
      other_operand = NULL;
    }
    else
    {
      elog(ERROR, "unsupported indexqual type: %d", (int)nodeTag(clause));
      other_operand = NULL;                          
    }

    cost_qual_eval_node(&index_qual_cost, other_operand, root);
    qual_arg_cost += index_qual_cost.startup + index_qual_cost.per_tuple;
  }
  return qual_arg_cost;
}

void
genericcostestimate(PlannerInfo *root, IndexPath *path, double loop_count, GenericCosts *costs)
{
  IndexOptInfo *index = path->indexinfo;
  List *indexQuals = get_quals_from_indexclauses(path->indexclauses);
  List *indexOrderBys = path->indexorderbys;
  Cost indexStartupCost;
  Cost indexTotalCost;
  Selectivity indexSelectivity;
  double indexCorrelation;
  double numIndexPages;
  double numIndexTuples;
  double spc_random_page_cost;
  double num_sa_scans;
  double num_outer_scans;
  double num_scans;
  double qual_op_cost;
  double qual_arg_cost;
  List *selectivityQuals;
  ListCell *l;

     
                                                                          
                                                                   
                  
     
  selectivityQuals = add_predicate_to_index_quals(index, indexQuals);

     
                                                                         
                                         
     
  num_sa_scans = 1;
  foreach (l, indexQuals)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(l);

    if (IsA(rinfo->clause, ScalarArrayOpExpr))
    {
      ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *)rinfo->clause;
      int alength = estimate_array_length(lsecond(saop->args));

      if (alength > 1)
      {
        num_sa_scans *= alength;
      }
    }
  }

                                                                       
  indexSelectivity = clauselist_selectivity(root, selectivityQuals, index->rel->relid, JOIN_INNER, NULL);

     
                                                                        
                                                                            
                                                               
     
  numIndexTuples = costs->numIndexTuples;
  if (numIndexTuples <= 0.0)
  {
    numIndexTuples = indexSelectivity * index->rel->tuples;

       
                                                                      
                                                                          
                                                                          
                                                                        
                                                       
       
    numIndexTuples = rint(numIndexTuples / num_sa_scans);
  }

     
                                                                            
                                                              
                                        
     
  if (numIndexTuples > index->tuples)
  {
    numIndexTuples = index->tuples;
  }
  if (numIndexTuples < 1.0)
  {
    numIndexTuples = 1.0;
  }

     
                                                                
     
                                                                             
                                                                            
                                                               
     
                                                                           
                                                                            
                                                                          
                                                                    
     
  if (index->pages > 1 && index->tuples > 1)
  {
    numIndexPages = ceil(numIndexTuples * index->pages / index->tuples);
  }
  else
  {
    numIndexPages = 1.0;
  }

                                                                 
  get_tablespace_page_costs(index->reltablespace, &spc_random_page_cost, NULL);

     
                                        
     
                                                                             
                                                                      
                                                                           
                                                                           
                                                                       
                                                                           
                              
     
                                                                       
                                                                       
                                                                          
                                                                            
                                                        
     
  num_outer_scans = loop_count;
  num_scans = num_sa_scans * num_outer_scans;

  if (num_scans > 1)
  {
    double pages_fetched;

                                                   
    pages_fetched = numIndexPages * num_scans;

                                                                    
    pages_fetched = index_pages_fetched(pages_fetched, index->pages, (double)index->pages, root);

       
                                                                           
                                                                          
                                                
       
    indexTotalCost = (pages_fetched * spc_random_page_cost) / num_outer_scans;
  }
  else
  {
       
                                                                        
                     
       
    indexTotalCost = numIndexPages * spc_random_page_cost;
  }

     
                                                                         
                                                                            
                                                                            
                                                                      
                                                                       
                                                                           
                                                                        
                           
     
                                                                           
                                                                        
                                                                    
     
  qual_arg_cost = index_other_operands_eval_cost(root, indexQuals) + index_other_operands_eval_cost(root, indexOrderBys);
  qual_op_cost = cpu_operator_cost * (list_length(indexQuals) + list_length(indexOrderBys));

  indexStartupCost = qual_arg_cost;
  indexTotalCost += qual_arg_cost;
  indexTotalCost += numIndexTuples * num_sa_scans * (cpu_index_tuple_cost + qual_op_cost);

     
                                                                  
     
  indexCorrelation = 0.0;

     
                                  
     
  costs->indexStartupCost = indexStartupCost;
  costs->indexTotalCost = indexTotalCost;
  costs->indexSelectivity = indexSelectivity;
  costs->indexCorrelation = indexCorrelation;
  costs->numIndexPages = numIndexPages;
  costs->numIndexTuples = numIndexTuples;
  costs->spc_random_page_cost = spc_random_page_cost;
  costs->num_sa_scans = num_sa_scans;
}

   
                                                                      
   
                                                                            
                                                                            
                                                                             
                                                                        
                                                                            
                                                                             
                                                                            
                                                                             
                                                                              
                                                                        
   
                                                                      
                                                                    
                                                                     
                                                          
   
List *
add_predicate_to_index_quals(IndexOptInfo *index, List *indexQuals)
{
  List *predExtraQuals = NIL;
  ListCell *lc;

  if (index->indpred == NIL)
  {
    return indexQuals;
  }

  foreach (lc, index->indpred)
  {
    Node *predQual = (Node *)lfirst(lc);
    List *oneQual = list_make1(predQual);

    if (!predicate_implied_by(oneQual, indexQuals, false))
    {
      predExtraQuals = list_concat(predExtraQuals, oneQual);
    }
  }
                                                                  
  return list_concat(predExtraQuals, indexQuals);
}

void
btcostestimate(PlannerInfo *root, IndexPath *path, double loop_count, Cost *indexStartupCost, Cost *indexTotalCost, Selectivity *indexSelectivity, double *indexCorrelation, double *indexPages)
{
  IndexOptInfo *index = path->indexinfo;
  GenericCosts costs;
  Oid relid;
  AttrNumber colnum;
  VariableStatData vardata;
  double numIndexTuples;
  Cost descentCost;
  List *indexBoundQuals;
  int indexcol;
  bool eqQualHere;
  bool found_saop;
  bool found_is_null_op;
  double num_sa_scans;
  ListCell *lc;

     
                                                                            
                                                                           
                                                                             
                                                                            
                                                                          
                                                                             
                                                                     
                                                          
     
                                                                      
                           
     
                                                                           
                                                                      
                                                     
     
  indexBoundQuals = NIL;
  indexcol = 0;
  eqQualHere = false;
  found_saop = false;
  found_is_null_op = false;
  num_sa_scans = 1;
  foreach (lc, path->indexclauses)
  {
    IndexClause *iclause = lfirst_node(IndexClause, lc);
    ListCell *lc2;

    if (indexcol != iclause->indexcol)
    {
                                             
      if (!eqQualHere)
      {
        break;                                       
      }
      eqQualHere = false;
      indexcol++;
      if (indexcol != iclause->indexcol)
      {
        break;                                   
      }
    }

                                                                  
    foreach (lc2, iclause->indexquals)
    {
      RestrictInfo *rinfo = lfirst_node(RestrictInfo, lc2);
      Expr *clause = rinfo->clause;
      Oid clause_op = InvalidOid;
      int op_strategy;

      if (IsA(clause, OpExpr))
      {
        OpExpr *op = (OpExpr *)clause;

        clause_op = op->opno;
      }
      else if (IsA(clause, RowCompareExpr))
      {
        RowCompareExpr *rc = (RowCompareExpr *)clause;

        clause_op = linitial_oid(rc->opnos);
      }
      else if (IsA(clause, ScalarArrayOpExpr))
      {
        ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *)clause;
        Node *other_operand = (Node *)lsecond(saop->args);
        int alength = estimate_array_length(other_operand);

        clause_op = saop->opno;
        found_saop = true;
                                                                      
        if (alength > 1)
        {
          num_sa_scans *= alength;
        }
      }
      else if (IsA(clause, NullTest))
      {
        NullTest *nt = (NullTest *)clause;

        if (nt->nulltesttype == IS_NULL)
        {
          found_is_null_op = true;
                                                          
          eqQualHere = true;
        }
      }
      else
      {
        elog(ERROR, "unsupported indexqual type: %d", (int)nodeTag(clause));
      }

                                       
      if (OidIsValid(clause_op))
      {
        op_strategy = get_op_opfamily_strategy(clause_op, index->opfamily[indexcol]);
        Assert(op_strategy != 0);                                 
        if (op_strategy == BTEqualStrategyNumber)
        {
          eqQualHere = true;
        }
      }

      indexBoundQuals = lappend(indexBoundQuals, rinfo);
    }
  }

     
                                                                           
                                                           
                                                                       
                                                                       
     
  if (index->unique && indexcol == index->nkeycolumns - 1 && eqQualHere && !found_saop && !found_is_null_op)
  {
    numIndexTuples = 1.0;
  }
  else
  {
    List *selectivityQuals;
    Selectivity btreeSelectivity;

       
                                                                 
                                                                          
                                             
       
    selectivityQuals = add_predicate_to_index_quals(index, indexBoundQuals);

    btreeSelectivity = clauselist_selectivity(root, selectivityQuals, index->rel->relid, JOIN_INNER, NULL);
    numIndexTuples = btreeSelectivity * index->rel->tuples;

       
                                                              
                                                                           
                   
       
    numIndexTuples = rint(numIndexTuples / num_sa_scans);
  }

     
                                           
     
  MemSet(&costs, 0, sizeof(costs));
  costs.numIndexTuples = numIndexTuples;

  genericcostestimate(root, path, loop_count, &costs);

     
                                                                      
                                                                             
                                                                             
                                                                     
                                       
     
                                                                         
                                                                         
                                                          
     
  if (index->tuples > 1)                             
  {
    descentCost = ceil(log(index->tuples) / log(2.0)) * cpu_operator_cost;
    costs.indexStartupCost += descentCost;
    costs.indexTotalCost += costs.num_sa_scans * descentCost;
  }

     
                                                                             
                                                                      
                                                                          
                                                                           
                                                                             
                                                                        
                                                                           
                                                                           
     
  descentCost = (index->tree_height + 1) * 50.0 * cpu_operator_cost;
  costs.indexStartupCost += descentCost;
  costs.indexTotalCost += costs.num_sa_scans * descentCost;

     
                                                                            
                                                                  
                                                                           
                                                                          
                                                                        
                                                                       
     
  MemSet(&vardata, 0, sizeof(vardata));

  if (index->indexkeys[0] != 0)
  {
                                                                    
    RangeTblEntry *rte = planner_rt_fetch(index->rel->relid, root);

    Assert(rte->rtekind == RTE_RELATION);
    relid = rte->relid;
    Assert(relid != InvalidOid);
    colnum = index->indexkeys[0];

    if (get_relation_stats_hook && (*get_relation_stats_hook)(root, rte, colnum, &vardata))
    {
         
                                                                      
                                                               
         
      if (HeapTupleIsValid(vardata.statsTuple) && !vardata.freefunc)
      {
        elog(ERROR, "no function provided to release variable stats with");
      }
    }
    else
    {
      vardata.statsTuple = SearchSysCache3(STATRELATTINH, ObjectIdGetDatum(relid), Int16GetDatum(colnum), BoolGetDatum(rte->inh));
      vardata.freefunc = ReleaseSysCache;
    }
  }
  else
  {
                                                                   
    relid = index->indexoid;
    colnum = 1;

    if (get_index_stats_hook && (*get_index_stats_hook)(root, relid, colnum, &vardata))
    {
         
                                                                      
                                                               
         
      if (HeapTupleIsValid(vardata.statsTuple) && !vardata.freefunc)
      {
        elog(ERROR, "no function provided to release variable stats with");
      }
    }
    else
    {
      vardata.statsTuple = SearchSysCache3(STATRELATTINH, ObjectIdGetDatum(relid), Int16GetDatum(colnum), BoolGetDatum(false));
      vardata.freefunc = ReleaseSysCache;
    }
  }

  if (HeapTupleIsValid(vardata.statsTuple))
  {
    Oid sortop;
    AttStatsSlot sslot;

    sortop = get_opfamily_member(index->opfamily[0], index->opcintype[0], index->opcintype[0], BTLessStrategyNumber);
    if (OidIsValid(sortop) && get_attstatsslot(&sslot, vardata.statsTuple, STATISTIC_KIND_CORRELATION, sortop, ATTSTATSSLOT_NUMBERS))
    {
      double varCorrelation;

      Assert(sslot.nnumbers == 1);
      varCorrelation = sslot.numbers[0];

      if (index->reverse_sort[0])
      {
        varCorrelation = -varCorrelation;
      }

      if (index->nkeycolumns > 1)
      {
        costs.indexCorrelation = varCorrelation * 0.75;
      }
      else
      {
        costs.indexCorrelation = varCorrelation;
      }

      free_attstatsslot(&sslot);
    }
  }

  ReleaseVariableStats(vardata);

  *indexStartupCost = costs.indexStartupCost;
  *indexTotalCost = costs.indexTotalCost;
  *indexSelectivity = costs.indexSelectivity;
  *indexCorrelation = costs.indexCorrelation;
  *indexPages = costs.numIndexPages;
}

void
hashcostestimate(PlannerInfo *root, IndexPath *path, double loop_count, Cost *indexStartupCost, Cost *indexTotalCost, Selectivity *indexSelectivity, double *indexCorrelation, double *indexPages)
{
  GenericCosts costs;

  MemSet(&costs, 0, sizeof(costs));

  genericcostestimate(root, path, loop_count, &costs);

     
                                                                          
                                                                          
                                                                             
                   
     
                                                                          
                                                                       
                                                                           
                                                                            
                                                            
     
                                                                           
                                                                            
                                                                    
                                                                      
                                                                          
     
                                                                        
                                                                            
                                                                         
                                                                             
                                               
     

  *indexStartupCost = costs.indexStartupCost;
  *indexTotalCost = costs.indexTotalCost;
  *indexSelectivity = costs.indexSelectivity;
  *indexCorrelation = costs.indexCorrelation;
  *indexPages = costs.numIndexPages;
}

void
gistcostestimate(PlannerInfo *root, IndexPath *path, double loop_count, Cost *indexStartupCost, Cost *indexTotalCost, Selectivity *indexSelectivity, double *indexCorrelation, double *indexPages)
{
  IndexOptInfo *index = path->indexinfo;
  GenericCosts costs;
  Cost descentCost;

  MemSet(&costs, 0, sizeof(costs));

  genericcostestimate(root, path, loop_count, &costs);

     
                                                                          
                                                                             
                                                                       
                           
     
                                                                        
                                                                   
     
  if (index->tree_height < 0)               
  {
    if (index->pages > 1)                             
    {
      index->tree_height = (int)(log(index->pages) / log(100.0));
    }
    else
    {
      index->tree_height = 0;
    }
  }

     
                                                                            
                                                                       
                                                                     
     
  if (index->tuples > 1)                             
  {
    descentCost = ceil(log(index->tuples)) * cpu_operator_cost;
    costs.indexStartupCost += descentCost;
    costs.indexTotalCost += costs.num_sa_scans * descentCost;
  }

     
                                                                        
     
  descentCost = (index->tree_height + 1) * 50.0 * cpu_operator_cost;
  costs.indexStartupCost += descentCost;
  costs.indexTotalCost += costs.num_sa_scans * descentCost;

  *indexStartupCost = costs.indexStartupCost;
  *indexTotalCost = costs.indexTotalCost;
  *indexSelectivity = costs.indexSelectivity;
  *indexCorrelation = costs.indexCorrelation;
  *indexPages = costs.numIndexPages;
}

void
spgcostestimate(PlannerInfo *root, IndexPath *path, double loop_count, Cost *indexStartupCost, Cost *indexTotalCost, Selectivity *indexSelectivity, double *indexCorrelation, double *indexPages)
{
  IndexOptInfo *index = path->indexinfo;
  GenericCosts costs;
  Cost descentCost;

  MemSet(&costs, 0, sizeof(costs));

  genericcostestimate(root, path, loop_count, &costs);

     
                                                                          
                                                                             
                                                                       
                           
     
                                                                        
                                                                   
     
  if (index->tree_height < 0)               
  {
    if (index->pages > 1)                             
    {
      index->tree_height = (int)(log(index->pages) / log(100.0));
    }
    else
    {
      index->tree_height = 0;
    }
  }

     
                                                                            
                                                                       
                                                                     
     
  if (index->tuples > 1)                             
  {
    descentCost = ceil(log(index->tuples)) * cpu_operator_cost;
    costs.indexStartupCost += descentCost;
    costs.indexTotalCost += costs.num_sa_scans * descentCost;
  }

     
                                                                        
     
  descentCost = (index->tree_height + 1) * 50.0 * cpu_operator_cost;
  costs.indexStartupCost += descentCost;
  costs.indexTotalCost += costs.num_sa_scans * descentCost;

  *indexStartupCost = costs.indexStartupCost;
  *indexTotalCost = costs.indexTotalCost;
  *indexSelectivity = costs.indexSelectivity;
  *indexCorrelation = costs.indexCorrelation;
  *indexPages = costs.numIndexPages;
}

   
                                        
   

typedef struct
{
  bool haveFullScan;
  double partialEntries;
  double exactEntries;
  double searchEntries;
  double arrayScans;
} GinQualCounts;

   
                                                                         
                                                                    
                                                                
   
static bool
gincost_pattern(IndexOptInfo *index, int indexcol, Oid clause_op, Datum query, GinQualCounts *counts)
{
  Oid extractProcOid;
  Oid collation;
  int strategy_op;
  Oid lefttype, righttype;
  int32 nentries = 0;
  bool *partial_matches = NULL;
  Pointer *extra_data = NULL;
  bool *nullFlags = NULL;
  int32 searchMode = GIN_SEARCH_MODE_DEFAULT;
  int32 i;

  Assert(indexcol < index->nkeycolumns);

     
                                                                             
                                                                
                                                                           
                                     
     
  get_op_opfamily_properties(clause_op, index->opfamily[indexcol], false, &strategy_op, &lefttype, &righttype);

     
                                                                           
                                                          
                                            
     
  extractProcOid = get_opfamily_proc(index->opfamily[indexcol], index->opcintype[indexcol], index->opcintype[indexcol], GIN_EXTRACTQUERY_PROC);

  if (!OidIsValid(extractProcOid))
  {
                                                                  
    elog(ERROR, "missing support function %d for attribute %d of index \"%s\"", GIN_EXTRACTQUERY_PROC, indexcol + 1, get_rel_name(index->indexoid));
  }

     
                                                                          
     
  if (OidIsValid(index->indexcollations[indexcol]))
  {
    collation = index->indexcollations[indexcol];
  }
  else
  {
    collation = DEFAULT_COLLATION_OID;
  }

  OidFunctionCall7Coll(extractProcOid, collation, query, PointerGetDatum(&nentries), UInt16GetDatum(strategy_op), PointerGetDatum(&partial_matches), PointerGetDatum(&extra_data), PointerGetDatum(&nullFlags), PointerGetDatum(&searchMode));

  if (nentries <= 0 && searchMode == GIN_SEARCH_MODE_DEFAULT)
  {
                              
    return false;
  }

  for (i = 0; i < nentries; i++)
  {
       
                                                                          
                                                                
       
    if (partial_matches && partial_matches[i])
    {
      counts->partialEntries += 100;
    }
    else
    {
      counts->exactEntries++;
    }

    counts->searchEntries++;
  }

  if (searchMode == GIN_SEARCH_MODE_INCLUDE_EMPTY)
  {
                                                        
    counts->exactEntries++;
    counts->searchEntries++;
  }
  else if (searchMode != GIN_SEARCH_MODE_DEFAULT)
  {
                                  
    counts->haveFullScan = true;
  }

  return true;
}

   
                                                                         
                                                                           
                                                                
   
static bool
gincost_opexpr(PlannerInfo *root, IndexOptInfo *index, int indexcol, OpExpr *clause, GinQualCounts *counts)
{
  Oid clause_op = clause->opno;
  Node *operand = (Node *)lsecond(clause->args);

                                                                      
  operand = estimate_expression_value(root, operand);

  if (IsA(operand, RelabelType))
  {
    operand = (Node *)((RelabelType *)operand)->arg;
  }

     
                                                                         
                                                                           
                                                            
     
  if (!IsA(operand, Const))
  {
    counts->exactEntries++;
    counts->searchEntries++;
    return true;
  }

                                                 
  if (((Const *)operand)->constisnull)
  {
    return false;
  }

                                                                    
  return gincost_pattern(index, indexcol, clause_op, ((Const *)operand)->constvalue, counts);
}

   
                                                                         
                                                                           
                                                                
   
                                                                           
                                                                     
                                                                          
                                                                          
                                                                            
                                                                        
   
static bool
gincost_scalararrayopexpr(PlannerInfo *root, IndexOptInfo *index, int indexcol, ScalarArrayOpExpr *clause, double numIndexEntries, GinQualCounts *counts)
{
  Oid clause_op = clause->opno;
  Node *rightop = (Node *)lsecond(clause->args);
  ArrayType *arrayval;
  int16 elmlen;
  bool elmbyval;
  char elmalign;
  int numElems;
  Datum *elemValues;
  bool *elemNulls;
  GinQualCounts arraycounts;
  int numPossible = 0;
  int i;

  Assert(clause->useOr);

                                                                      
  rightop = estimate_expression_value(root, rightop);

  if (IsA(rightop, RelabelType))
  {
    rightop = (Node *)((RelabelType *)rightop)->arg;
  }

     
                                                                         
                                                                           
                                                                          
                                                                     
     
  if (!IsA(rightop, Const))
  {
    counts->exactEntries++;
    counts->searchEntries++;
    counts->arrayScans *= estimate_array_length(rightop);
    return true;
  }

                                                 
  if (((Const *)rightop)->constisnull)
  {
    return false;
  }

                                                                   
  arrayval = DatumGetArrayTypeP(((Const *)rightop)->constvalue);
  get_typlenbyvalalign(ARR_ELEMTYPE(arrayval), &elmlen, &elmbyval, &elmalign);
  deconstruct_array(arrayval, ARR_ELEMTYPE(arrayval), elmlen, elmbyval, elmalign, &elemValues, &elemNulls, &numElems);

  memset(&arraycounts, 0, sizeof(arraycounts));

  for (i = 0; i < numElems; i++)
  {
    GinQualCounts elemcounts;

                                                                    
    if (elemNulls[i])
    {
      continue;
    }

                                                                      
    memset(&elemcounts, 0, sizeof(elemcounts));

    if (gincost_pattern(index, indexcol, clause_op, elemValues[i], &elemcounts))
    {
                                                                    
      numPossible++;

      if (elemcounts.haveFullScan)
      {
           
                                                                  
                                                                   
                            
           
        elemcounts.partialEntries = 0;
        elemcounts.exactEntries = numIndexEntries;
        elemcounts.searchEntries = numIndexEntries;
      }
      arraycounts.partialEntries += elemcounts.partialEntries;
      arraycounts.exactEntries += elemcounts.exactEntries;
      arraycounts.searchEntries += elemcounts.searchEntries;
    }
  }

  if (numPossible == 0)
  {
                                              
    return false;
  }

     
                                                                      
                                                                             
                                                                  
     
  counts->partialEntries += arraycounts.partialEntries / numPossible;
  counts->exactEntries += arraycounts.exactEntries / numPossible;
  counts->searchEntries += arraycounts.searchEntries / numPossible;

  counts->arrayScans *= numPossible;

  return true;
}

   
                                                                       
   
void
gincostestimate(PlannerInfo *root, IndexPath *path, double loop_count, Cost *indexStartupCost, Cost *indexTotalCost, Selectivity *indexSelectivity, double *indexCorrelation, double *indexPages)
{
  IndexOptInfo *index = path->indexinfo;
  List *indexQuals = get_quals_from_indexclauses(path->indexclauses);
  List *selectivityQuals;
  double numPages = index->pages, numTuples = index->tuples;
  double numEntryPages, numDataPages, numPendingPages, numEntries;
  GinQualCounts counts;
  bool matchPossible;
  double partialScale;
  double entryPagesFetched, dataPagesFetched, dataPagesFetchedBySel;
  double qual_op_cost, qual_arg_cost, spc_random_page_cost, outer_scans;
  Relation indexRel;
  GinStatsData ginStats;
  ListCell *lc;

     
                                                                           
                                                   
     
  if (!index->hypothetical)
  {
                                                             
    indexRel = index_open(index->indexoid, NoLock);
    ginGetStats(indexRel, &ginStats);
    index_close(indexRel, NoLock);
  }
  else
  {
    memset(&ginStats, 0, sizeof(ginStats));
  }

     
                                                                        
                                                                           
                                                                          
                                                                         
                                                                      
                                                                             
                                                                             
                                                                             
                                                                       
     
  if (ginStats.nPendingPages < numPages)
  {
    numPendingPages = ginStats.nPendingPages;
  }
  else
  {
    numPendingPages = 0;
  }

  if (numPages > 0 && ginStats.nTotalPages <= numPages && ginStats.nTotalPages > numPages / 4 && ginStats.nEntryPages > 0 && ginStats.nEntries > 0)
  {
       
                                                                      
                                                                       
                                                 
       
    double scale = numPages / ginStats.nTotalPages;

    numEntryPages = ceil(ginStats.nEntryPages * scale);
    numDataPages = ceil(ginStats.nDataPages * scale);
    numEntries = ceil(ginStats.nEntries * scale);
                                            
    numEntryPages = Min(numEntryPages, numPages - numPendingPages);
    numDataPages = Min(numDataPages, numPages - numPendingPages - numEntryPages);
  }
  else
  {
       
                                                                        
                                                                         
                                                                       
                                                                      
       
                                                                         
                                                                      
                                                                           
                                                                         
                                                                        
                                                                    
       
    numPages = Max(numPages, 10);
    numEntryPages = floor((numPages - numPendingPages) * 0.90);
    numDataPages = numPages - numPendingPages - numEntryPages;
    numEntries = floor(numEntryPages * 100);
  }

                                                                          
  if (numEntries < 1)
  {
    numEntries = 1;
  }

     
                                                                           
                                                                            
                           
     
  selectivityQuals = add_predicate_to_index_quals(index, indexQuals);

                                                                       
  *indexSelectivity = clauselist_selectivity(root, selectivityQuals, index->rel->relid, JOIN_INNER, NULL);

                                                                 
  get_tablespace_page_costs(index->reltablespace, &spc_random_page_cost, NULL);

     
                                                                  
     
  *indexCorrelation = 0.0;

     
                                                                          
     
  memset(&counts, 0, sizeof(counts));
  counts.arrayScans = 1;
  matchPossible = true;

  foreach (lc, path->indexclauses)
  {
    IndexClause *iclause = lfirst_node(IndexClause, lc);
    ListCell *lc2;

    foreach (lc2, iclause->indexquals)
    {
      RestrictInfo *rinfo = lfirst_node(RestrictInfo, lc2);
      Expr *clause = rinfo->clause;

      if (IsA(clause, OpExpr))
      {
        matchPossible = gincost_opexpr(root, index, iclause->indexcol, (OpExpr *)clause, &counts);
        if (!matchPossible)
        {
          break;
        }
      }
      else if (IsA(clause, ScalarArrayOpExpr))
      {
        matchPossible = gincost_scalararrayopexpr(root, index, iclause->indexcol, (ScalarArrayOpExpr *)clause, numEntries, &counts);
        if (!matchPossible)
        {
          break;
        }
      }
      else
      {
                                                        
        elog(ERROR, "unsupported GIN indexqual type: %d", (int)nodeTag(clause));
      }
    }
  }

                                                               
  if (!matchPossible)
  {
    *indexStartupCost = 0;
    *indexTotalCost = 0;
    *indexSelectivity = 0;
    return;
  }

  if (counts.haveFullScan || indexQuals == NIL)
  {
       
                                                                           
                                                                   
       
    counts.partialEntries = 0;
    counts.exactEntries = numEntries;
    counts.searchEntries = numEntries;
  }

                                                                
  outer_scans = loop_count;

     
                                                                        
           
     
  entryPagesFetched = numPendingPages;

     
                                                         
                                                                           
                                                                           
                                                                        
                     
     
  entryPagesFetched += ceil(counts.searchEntries * rint(pow(numEntryPages, 0.15)));

     
                                                                            
                                                                            
                                                                            
                                                                       
                                                        
     
  partialScale = counts.partialEntries / numEntries;
  partialScale = Min(partialScale, 1.0);

  entryPagesFetched += ceil(numEntryPages * partialScale);

     
                                                                            
                                                                          
                                
     
  dataPagesFetched = ceil(numDataPages * partialScale);

     
                                                                             
                                                                           
                                                                  
     
  if (outer_scans > 1 || counts.arrayScans > 1)
  {
    entryPagesFetched *= outer_scans * counts.arrayScans;
    entryPagesFetched = index_pages_fetched(entryPagesFetched, (BlockNumber)numEntryPages, numEntryPages, root);
    entryPagesFetched /= outer_scans;
    dataPagesFetched *= outer_scans * counts.arrayScans;
    dataPagesFetched = index_pages_fetched(dataPagesFetched, (BlockNumber)numDataPages, numDataPages, root);
    dataPagesFetched /= outer_scans;
  }

     
                                                                             
                    
     
  *indexStartupCost = (entryPagesFetched + dataPagesFetched) * spc_random_page_cost;

     
                                                                   
     
                                                                            
                                                                            
                                                                             
                  
     
  dataPagesFetched = ceil(numDataPages * counts.exactEntries / numEntries);

     
                                                                             
                                                                     
                                                                             
                                                                          
                                               
     
                                                                  
                                                                       
                                                
     
  dataPagesFetchedBySel = ceil(*indexSelectivity * (numTuples / (BLCKSZ / 3)));
  if (dataPagesFetchedBySel > dataPagesFetched)
  {
    dataPagesFetched = dataPagesFetchedBySel;
  }

                                                    
  if (outer_scans > 1 || counts.arrayScans > 1)
  {
    dataPagesFetched *= outer_scans * counts.arrayScans;
    dataPagesFetched = index_pages_fetched(dataPagesFetched, (BlockNumber)numDataPages, numDataPages, root);
    dataPagesFetched /= outer_scans;
  }

                                                       
  *indexTotalCost = *indexStartupCost + dataPagesFetched * spc_random_page_cost;

     
                                                                           
                                                                   
     
  qual_arg_cost = index_other_operands_eval_cost(root, indexQuals);
  qual_op_cost = cpu_operator_cost * list_length(indexQuals);

  *indexStartupCost += qual_arg_cost;
  *indexTotalCost += qual_arg_cost;
  *indexTotalCost += (numTuples * *indexSelectivity) * (cpu_index_tuple_cost + qual_op_cost);
  *indexPages = dataPagesFetched;
}

   
                                                                        
   
void
brincostestimate(PlannerInfo *root, IndexPath *path, double loop_count, Cost *indexStartupCost, Cost *indexTotalCost, Selectivity *indexSelectivity, double *indexCorrelation, double *indexPages)
{
  IndexOptInfo *index = path->indexinfo;
  List *indexQuals = get_quals_from_indexclauses(path->indexclauses);
  double numPages = index->pages;
  RelOptInfo *baserel = index->rel;
  RangeTblEntry *rte = planner_rt_fetch(baserel->relid, root);
  Cost spc_seq_page_cost;
  Cost spc_random_page_cost;
  double qual_arg_cost;
  double qualSelectivity;
  BrinStatsData statsData;
  double indexRanges;
  double minimalRanges;
  double estimatedRanges;
  double selec;
  Relation indexRel;
  ListCell *l;
  VariableStatData vardata;

  Assert(rte->rtekind == RTE_RELATION);

                                                                         
  get_tablespace_page_costs(index->reltablespace, &spc_random_page_cost, &spc_seq_page_cost);

     
                                                                            
                                                                          
     
  if (!index->hypothetical)
  {
       
                                                                           
       
    indexRel = index_open(index->indexoid, NoLock);
    brinGetStats(indexRel, &statsData);
    index_close(indexRel, NoLock);

                                                           
    indexRanges = Max(ceil((double)baserel->pages / statsData.pagesPerRange), 1.0);
  }
  else
  {
       
                                                                         
                                
       
    indexRanges = Max(ceil((double)baserel->pages / BRIN_DEFAULT_PAGES_PER_RANGE), 1.0);

    statsData.pagesPerRange = BRIN_DEFAULT_PAGES_PER_RANGE;
    statsData.revmapNumPages = (indexRanges / REVMAP_PAGE_MAXITEMS) + 1;
  }

     
                               
     
                                                                          
                                                                           
                                                                            
                                                   
     
  *indexCorrelation = 0;

  foreach (l, path->indexclauses)
  {
    IndexClause *iclause = lfirst_node(IndexClause, l);
    AttrNumber attnum = index->indexkeys[iclause->indexcol];

                                                                   
    if (attnum != 0)
    {
                                                                     
      if (get_relation_stats_hook && (*get_relation_stats_hook)(root, rte, attnum, &vardata))
      {
           
                                                                    
                                                                     
           
        if (HeapTupleIsValid(vardata.statsTuple) && !vardata.freefunc)
        {
          elog(ERROR, "no function provided to release variable stats with");
        }
      }
      else
      {
        vardata.statsTuple = SearchSysCache3(STATRELATTINH, ObjectIdGetDatum(rte->relid), Int16GetDatum(attnum), BoolGetDatum(false));
        vardata.freefunc = ReleaseSysCache;
      }
    }
    else
    {
         
                                                                         
                                          
         

                                                  
      attnum = iclause->indexcol + 1;

      if (get_index_stats_hook && (*get_index_stats_hook)(root, index->indexoid, attnum, &vardata))
      {
           
                                                                    
                                                                     
           
        if (HeapTupleIsValid(vardata.statsTuple) && !vardata.freefunc)
        {
          elog(ERROR, "no function provided to release variable stats with");
        }
      }
      else
      {
        vardata.statsTuple = SearchSysCache3(STATRELATTINH, ObjectIdGetDatum(index->indexoid), Int16GetDatum(attnum), BoolGetDatum(false));
        vardata.freefunc = ReleaseSysCache;
      }
    }

    if (HeapTupleIsValid(vardata.statsTuple))
    {
      AttStatsSlot sslot;

      if (get_attstatsslot(&sslot, vardata.statsTuple, STATISTIC_KIND_CORRELATION, InvalidOid, ATTSTATSSLOT_NUMBERS))
      {
        double varCorrelation = 0.0;

        if (sslot.nnumbers > 0)
        {
          varCorrelation = Abs(sslot.numbers[0]);
        }

        if (varCorrelation > *indexCorrelation)
        {
          *indexCorrelation = varCorrelation;
        }

        free_attstatsslot(&sslot);
      }
    }

    ReleaseVariableStats(vardata);
  }

  qualSelectivity = clauselist_selectivity(root, indexQuals, baserel->relid, JOIN_INNER, NULL);

     
                                                                             
                                                             
     
  minimalRanges = ceil(indexRanges * qualSelectivity);

     
                                                                     
                                                                          
                                                         
     
  if (*indexCorrelation < 1.0e-10)
  {
    estimatedRanges = indexRanges;
  }
  else
  {
    estimatedRanges = Min(minimalRanges / *indexCorrelation, indexRanges);
  }

                                                    
  selec = estimatedRanges / indexRanges;

  CLAMP_PROBABILITY(selec);

  *indexSelectivity = selec;

     
                                                                             
                                                                          
                    
     
  qual_arg_cost = index_other_operands_eval_cost(root, indexQuals);

     
                                                                   
                                                                  
     
  *indexStartupCost = spc_seq_page_cost * statsData.revmapNumPages * loop_count;
  *indexStartupCost += qual_arg_cost;

     
                                                                      
                                                                           
                                                                          
     
  *indexTotalCost = *indexStartupCost + spc_random_page_cost * (numPages - statsData.revmapNumPages) * loop_count;

     
                                                                             
                                                                             
                                                                       
                                                                         
            
     
  *indexTotalCost += 0.1 * cpu_operator_cost * estimatedRanges * statsData.pagesPerRange;

  *indexPages = index->pages;
}
