                                                                            
   
                      
                                                                 
   
                                                                        
                                                                         
                                       
   
                                                                         
                                                                        
   
   
                  
                                              
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "access/htup_details.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_statistic.h"
#include "utils/builtins.h"
#include "utils/inet.h"
#include "utils/lsyscache.h"
#include "utils/selfuncs.h"

                                                       
#define DEFAULT_OVERLAP_SEL 0.01

                                                             
#define DEFAULT_INCLUSION_SEL 0.005

                                                
#define DEFAULT_SEL(operator) ((operator) == OID_INET_OVERLAP_OP ? DEFAULT_OVERLAP_SEL : DEFAULT_INCLUSION_SEL)

                                                                          
#define MAX_CONSIDERED_ELEMS 1024

static Selectivity
networkjoinsel_inner(Oid operator, VariableStatData * vardata1, VariableStatData *vardata2);
static Selectivity
networkjoinsel_semi(Oid operator, VariableStatData * vardata1, VariableStatData *vardata2);
static Selectivity
mcv_population(float4 *mcv_numbers, int mcv_nvalues);
static Selectivity
inet_hist_value_sel(Datum *values, int nvalues, Datum constvalue, int opr_codenum);
static Selectivity
inet_mcv_join_sel(Datum *mcv1_values, float4 *mcv1_numbers, int mcv1_nvalues, Datum *mcv2_values, float4 *mcv2_numbers, int mcv2_nvalues, Oid operator);
static Selectivity
inet_mcv_hist_sel(Datum *mcv_values, float4 *mcv_numbers, int mcv_nvalues, Datum *hist_values, int hist_nvalues, int opr_codenum);
static Selectivity
inet_hist_inclusion_join_sel(Datum *hist1_values, int hist1_nvalues, Datum *hist2_values, int hist2_nvalues, int opr_codenum);
static Selectivity
inet_semi_join_sel(Datum lhs_value, bool mcv_exists, Datum *mcv_values, int mcv_nvalues, bool hist_exists, Datum *hist_values, int hist_nvalues, double hist_weight, FmgrInfo *proc, int opr_codenum);
static int inet_opr_codenum(Oid
    operator);
static int
inet_inclusion_cmp(inet *left, inet *right, int opr_codenum);
static int
inet_masklen_inclusion_cmp(inet *left, inet *right, int opr_codenum);
static int
inet_hist_match_divider(inet *boundary, inet *query, int opr_codenum);

   
                                                                     
   
Datum
networksel(PG_FUNCTION_ARGS)
{
  PlannerInfo *root = (PlannerInfo *)PG_GETARG_POINTER(0);
  Oid operator= PG_GETARG_OID(1);
  List *args = (List *)PG_GETARG_POINTER(2);
  int varRelid = PG_GETARG_INT32(3);
  VariableStatData vardata;
  Node *other;
  bool varonleft;
  Selectivity selec, mcv_selec, non_mcv_selec;
  Datum constvalue;
  Form_pg_statistic stats;
  AttStatsSlot hslot;
  double sumcommon, nullfrac;
  FmgrInfo proc;

     
                                                                   
                                                         
     
  if (!get_restriction_variable(root, args, varRelid, &vardata, &other, &varonleft))
  {
    PG_RETURN_FLOAT8(DEFAULT_SEL(operator));
  }

     
                                                                          
     
  if (!IsA(other, Const))
  {
    ReleaseVariableStats(vardata);
    PG_RETURN_FLOAT8(DEFAULT_SEL(operator));
  }

                                                     
  if (((Const *)other)->constisnull)
  {
    ReleaseVariableStats(vardata);
    PG_RETURN_FLOAT8(0.0);
  }
  constvalue = ((Const *)other)->constvalue;

                                                                            
  if (!HeapTupleIsValid(vardata.statsTuple))
  {
    ReleaseVariableStats(vardata);
    PG_RETURN_FLOAT8(DEFAULT_SEL(operator));
  }

  stats = (Form_pg_statistic)GETSTRUCT(vardata.statsTuple);
  nullfrac = stats->stanullfrac;

     
                                                                         
                                                                             
                                                                            
                     
     
  fmgr_info(get_opcode(operator), &proc);
  mcv_selec = mcv_selectivity_ext(&vardata, &proc, InvalidOid, constvalue, varonleft, &sumcommon);

     
                                                                      
                                                                           
                                             
     
  if (get_attstatsslot(&hslot, vardata.statsTuple, STATISTIC_KIND_HISTOGRAM, InvalidOid, ATTSTATSSLOT_VALUES))
  {
    int opr_codenum = inet_opr_codenum(operator);

                                                                           
    if (!varonleft)
    {
      opr_codenum = -opr_codenum;
    }
    non_mcv_selec = inet_hist_value_sel(hslot.values, hslot.nvalues, constvalue, opr_codenum);

    free_attstatsslot(&hslot);
  }
  else
  {
    non_mcv_selec = DEFAULT_SEL(operator);
  }

                                                             
  selec = mcv_selec + (1.0 - nullfrac - sumcommon) * non_mcv_selec;

                                                   
  CLAMP_PROBABILITY(selec);

  ReleaseVariableStats(vardata);

  PG_RETURN_FLOAT8(selec);
}

   
                                                                          
   
                                                                      
   
                                                                              
                                                                            
                                                                            
                                                                              
                                                                               
                                                                             
                                                                             
                                                                           
                                                                        
                                                                               
                                                                            
                                      
   
Datum
networkjoinsel(PG_FUNCTION_ARGS)
{
  PlannerInfo *root = (PlannerInfo *)PG_GETARG_POINTER(0);
  Oid operator= PG_GETARG_OID(1);
  List *args = (List *)PG_GETARG_POINTER(2);
#ifdef NOT_USED
  JoinType jointype = (JoinType)PG_GETARG_INT16(3);
#endif
  SpecialJoinInfo *sjinfo = (SpecialJoinInfo *)PG_GETARG_POINTER(4);
  double selec;
  VariableStatData vardata1;
  VariableStatData vardata2;
  bool join_is_reversed;

  get_join_variables(root, args, sjinfo, &vardata1, &vardata2, &join_is_reversed);

  switch (sjinfo->jointype)
  {
  case JOIN_INNER:
  case JOIN_LEFT:
  case JOIN_FULL:

       
                                                                       
                                                               
       
    selec = networkjoinsel_inner(operator, & vardata1, &vardata2);
    break;
  case JOIN_SEMI:
  case JOIN_ANTI:
                                                                      
    if (!join_is_reversed)
    {
      selec = networkjoinsel_semi(operator, & vardata1, &vardata2);
    }
    else
    {
      selec = networkjoinsel_semi(get_commutator(operator), &vardata2, &vardata1);
    }
    break;
  default:
                                        
    elog(ERROR, "unrecognized join type: %d", (int)sjinfo->jointype);
    selec = 0;                          
    break;
  }

  ReleaseVariableStats(vardata1);
  ReleaseVariableStats(vardata2);

  CLAMP_PROBABILITY(selec);

  PG_RETURN_FLOAT8((float8)selec);
}

   
                                                                            
   
                                                                      
                                                                          
                                                                           
                                                                       
                                                                           
                                                                           
                                                                          
                                                                                
   
static Selectivity
networkjoinsel_inner(Oid operator, VariableStatData * vardata1, VariableStatData *vardata2)
{
  Form_pg_statistic stats;
  double nullfrac1 = 0.0, nullfrac2 = 0.0;
  Selectivity selec = 0.0, sumcommon1 = 0.0, sumcommon2 = 0.0;
  bool mcv1_exists = false, mcv2_exists = false, hist1_exists = false, hist2_exists = false;
  int opr_codenum;
  int mcv1_length = 0, mcv2_length = 0;
  AttStatsSlot mcv1_slot;
  AttStatsSlot mcv2_slot;
  AttStatsSlot hist1_slot;
  AttStatsSlot hist2_slot;

  if (HeapTupleIsValid(vardata1->statsTuple))
  {
    stats = (Form_pg_statistic)GETSTRUCT(vardata1->statsTuple);
    nullfrac1 = stats->stanullfrac;

    mcv1_exists = get_attstatsslot(&mcv1_slot, vardata1->statsTuple, STATISTIC_KIND_MCV, InvalidOid, ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS);
    hist1_exists = get_attstatsslot(&hist1_slot, vardata1->statsTuple, STATISTIC_KIND_HISTOGRAM, InvalidOid, ATTSTATSSLOT_VALUES);
                                                     
    mcv1_length = Min(mcv1_slot.nvalues, MAX_CONSIDERED_ELEMS);
    if (mcv1_exists)
    {
      sumcommon1 = mcv_population(mcv1_slot.numbers, mcv1_length);
    }
  }
  else
  {
    memset(&mcv1_slot, 0, sizeof(mcv1_slot));
    memset(&hist1_slot, 0, sizeof(hist1_slot));
  }

  if (HeapTupleIsValid(vardata2->statsTuple))
  {
    stats = (Form_pg_statistic)GETSTRUCT(vardata2->statsTuple);
    nullfrac2 = stats->stanullfrac;

    mcv2_exists = get_attstatsslot(&mcv2_slot, vardata2->statsTuple, STATISTIC_KIND_MCV, InvalidOid, ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS);
    hist2_exists = get_attstatsslot(&hist2_slot, vardata2->statsTuple, STATISTIC_KIND_HISTOGRAM, InvalidOid, ATTSTATSSLOT_VALUES);
                                                     
    mcv2_length = Min(mcv2_slot.nvalues, MAX_CONSIDERED_ELEMS);
    if (mcv2_exists)
    {
      sumcommon2 = mcv_population(mcv2_slot.numbers, mcv2_length);
    }
  }
  else
  {
    memset(&mcv2_slot, 0, sizeof(mcv2_slot));
    memset(&hist2_slot, 0, sizeof(hist2_slot));
  }

  opr_codenum = inet_opr_codenum(operator);

     
                                                   
     
  if (mcv1_exists && mcv2_exists)
  {
    selec += inet_mcv_join_sel(mcv1_slot.values, mcv1_slot.numbers, mcv1_length, mcv2_slot.values, mcv2_slot.numbers, mcv2_length, operator);
  }

     
                                                                             
                                                                          
                                                         
     
  if (mcv1_exists && hist2_exists)
  {
    selec += (1.0 - nullfrac2 - sumcommon2) * inet_mcv_hist_sel(mcv1_slot.values, mcv1_slot.numbers, mcv1_length, hist2_slot.values, hist2_slot.nvalues, opr_codenum);
  }
  if (mcv2_exists && hist1_exists)
  {
    selec += (1.0 - nullfrac1 - sumcommon1) * inet_mcv_hist_sel(mcv2_slot.values, mcv2_slot.numbers, mcv2_length, hist1_slot.values, hist1_slot.nvalues, -opr_codenum);
  }

     
                                                                          
                    
     
  if (hist1_exists && hist2_exists)
  {
    selec += (1.0 - nullfrac1 - sumcommon1) * (1.0 - nullfrac2 - sumcommon2) * inet_hist_inclusion_join_sel(hist1_slot.values, hist1_slot.nvalues, hist2_slot.values, hist2_slot.nvalues, opr_codenum);
  }

     
                                                                           
                                                   
     
  if ((!mcv1_exists && !hist1_exists) || (!mcv2_exists && !hist2_exists))
  {
    selec = (1.0 - nullfrac1) * (1.0 - nullfrac2) * DEFAULT_SEL(operator);
  }

                      
  free_attstatsslot(&mcv1_slot);
  free_attstatsslot(&mcv2_slot);
  free_attstatsslot(&hist1_slot);
  free_attstatsslot(&hist2_slot);

  return selec;
}

   
                                                                           
   
                                                                               
                                                   
   
static Selectivity
networkjoinsel_semi(Oid operator, VariableStatData * vardata1, VariableStatData *vardata2)
{
  Form_pg_statistic stats;
  Selectivity selec = 0.0, sumcommon1 = 0.0, sumcommon2 = 0.0;
  double nullfrac1 = 0.0, nullfrac2 = 0.0, hist2_weight = 0.0;
  bool mcv1_exists = false, mcv2_exists = false, hist1_exists = false, hist2_exists = false;
  int opr_codenum;
  FmgrInfo proc;
  int i, mcv1_length = 0, mcv2_length = 0;
  AttStatsSlot mcv1_slot;
  AttStatsSlot mcv2_slot;
  AttStatsSlot hist1_slot;
  AttStatsSlot hist2_slot;

  if (HeapTupleIsValid(vardata1->statsTuple))
  {
    stats = (Form_pg_statistic)GETSTRUCT(vardata1->statsTuple);
    nullfrac1 = stats->stanullfrac;

    mcv1_exists = get_attstatsslot(&mcv1_slot, vardata1->statsTuple, STATISTIC_KIND_MCV, InvalidOid, ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS);
    hist1_exists = get_attstatsslot(&hist1_slot, vardata1->statsTuple, STATISTIC_KIND_HISTOGRAM, InvalidOid, ATTSTATSSLOT_VALUES);
                                                     
    mcv1_length = Min(mcv1_slot.nvalues, MAX_CONSIDERED_ELEMS);
    if (mcv1_exists)
    {
      sumcommon1 = mcv_population(mcv1_slot.numbers, mcv1_length);
    }
  }
  else
  {
    memset(&mcv1_slot, 0, sizeof(mcv1_slot));
    memset(&hist1_slot, 0, sizeof(hist1_slot));
  }

  if (HeapTupleIsValid(vardata2->statsTuple))
  {
    stats = (Form_pg_statistic)GETSTRUCT(vardata2->statsTuple);
    nullfrac2 = stats->stanullfrac;

    mcv2_exists = get_attstatsslot(&mcv2_slot, vardata2->statsTuple, STATISTIC_KIND_MCV, InvalidOid, ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS);
    hist2_exists = get_attstatsslot(&hist2_slot, vardata2->statsTuple, STATISTIC_KIND_HISTOGRAM, InvalidOid, ATTSTATSSLOT_VALUES);
                                                     
    mcv2_length = Min(mcv2_slot.nvalues, MAX_CONSIDERED_ELEMS);
    if (mcv2_exists)
    {
      sumcommon2 = mcv_population(mcv2_slot.numbers, mcv2_length);
    }
  }
  else
  {
    memset(&mcv2_slot, 0, sizeof(mcv2_slot));
    memset(&hist2_slot, 0, sizeof(hist2_slot));
  }

  opr_codenum = inet_opr_codenum(operator);
  fmgr_info(get_opcode(operator), &proc);

                                                                   
  if (hist2_exists && vardata2->rel)
  {
    hist2_weight = (1.0 - nullfrac2 - sumcommon2) * vardata2->rel->rows;
  }

     
                                                                            
                                                                        
     
  if (mcv1_exists && (mcv2_exists || hist2_exists))
  {
    for (i = 0; i < mcv1_length; i++)
    {
      selec += mcv1_slot.numbers[i] * inet_semi_join_sel(mcv1_slot.values[i], mcv2_exists, mcv2_slot.values, mcv2_length, hist2_exists, hist2_slot.values, hist2_slot.nvalues, hist2_weight, &proc, opr_codenum);
    }
  }

     
                                                                          
                                                                          
                                                                          
                                                                           
                                                                             
                                                                            
                 
     
                                                                          
     
  if (hist1_exists && hist1_slot.nvalues > 2 && (mcv2_exists || hist2_exists))
  {
    double hist_selec_sum = 0.0;
    int k, n;

    k = (hist1_slot.nvalues - 3) / MAX_CONSIDERED_ELEMS + 1;

    n = 0;
    for (i = 1; i < hist1_slot.nvalues - 1; i += k)
    {
      hist_selec_sum += inet_semi_join_sel(hist1_slot.values[i], mcv2_exists, mcv2_slot.values, mcv2_length, hist2_exists, hist2_slot.values, hist2_slot.nvalues, hist2_weight, &proc, opr_codenum);
      n++;
    }

    selec += (1.0 - nullfrac1 - sumcommon1) * hist_selec_sum / n;
  }

     
                                                                           
                                                   
     
  if ((!mcv1_exists && !hist1_exists) || (!mcv2_exists && !hist2_exists))
  {
    selec = (1.0 - nullfrac1) * (1.0 - nullfrac2) * DEFAULT_SEL(operator);
  }

                      
  free_attstatsslot(&mcv1_slot);
  free_attstatsslot(&mcv2_slot);
  free_attstatsslot(&hist1_slot);
  free_attstatsslot(&hist2_slot);

  return selec;
}

   
                                                                       
                    
   
static Selectivity
mcv_population(float4 *mcv_numbers, int mcv_nvalues)
{
  Selectivity sumcommon = 0.0;
  int i;

  for (i = 0; i < mcv_nvalues; i++)
  {
    sumcommon += mcv_numbers[i];
  }

  return sumcommon;
}

   
                                                         
   
                                                                    
                                                                     
                                                                     
   
                                                                        
                                                                               
                                                                               
                                                                             
                                                                         
                                                                         
                                                                            
                                                         
   
                                                                          
                                                                            
                                                                            
                                                                        
                                                                            
                                                                         
                                                                            
                                                    
   
                                                                       
                                                                            
                                                                           
                                                                             
                                                       
   
                                                                     
                                                                             
                                                                            
                                                                          
                                                                             
                                                                          
                                                                         
                                                                       
                                                                               
                                                                              
                                                                               
         
   
                                                                            
                                                                           
                                                                             
                                                                              
                                                                            
                                                            
   
static Selectivity
inet_hist_value_sel(Datum *values, int nvalues, Datum constvalue, int opr_codenum)
{
  Selectivity match = 0.0;
  inet *query, *left, *right;
  int i, k, n;
  int left_order, right_order, left_divider, right_divider;

                                       
  if (nvalues <= 1)
  {
    return 0.0;
  }

                                                                           
  k = (nvalues - 2) / MAX_CONSIDERED_ELEMS + 1;

  query = DatumGetInetPP(constvalue);

                                                                   
  left = DatumGetInetPP(values[0]);
  left_order = inet_inclusion_cmp(left, query, opr_codenum);

  n = 0;
  for (i = k; i < nvalues; i += k)
  {
                                                     
    right = DatumGetInetPP(values[i]);
    right_order = inet_inclusion_cmp(right, query, opr_codenum);

    if (left_order == 0 && right_order == 0)
    {
                                                              
      match += 1.0;
    }
    else if ((left_order <= 0 && right_order >= 0) || (left_order >= 0 && right_order <= 0))
    {
                                 
      left_divider = inet_hist_match_divider(left, query, opr_codenum);
      right_divider = inet_hist_match_divider(right, query, opr_codenum);

      if (left_divider >= 0 || right_divider >= 0)
      {
        match += 1.0 / pow(2.0, Max(left_divider, right_divider));
      }
    }

                              
    left = right;
    left_order = right_order;

                                                 
    n++;
  }

  return match / n;
}

   
                                               
   
                                                                              
                                                               
   
static Selectivity
inet_mcv_join_sel(Datum *mcv1_values, float4 *mcv1_numbers, int mcv1_nvalues, Datum *mcv2_values, float4 *mcv2_numbers, int mcv2_nvalues, Oid operator)
{
  Selectivity selec = 0.0;
  FmgrInfo proc;
  int i, j;

  fmgr_info(get_opcode(operator), &proc);

  for (i = 0; i < mcv1_nvalues; i++)
  {
    for (j = 0; j < mcv2_nvalues; j++)
    {
      if (DatumGetBool(FunctionCall2(&proc, mcv1_values[i], mcv2_values[j])))
      {
        selec += mcv1_numbers[i] * mcv2_numbers[j];
      }
    }
  }
  return selec;
}

   
                                                     
   
                                                                               
                                                                          
                                                                        
                                                                          
                  
   
static Selectivity
inet_mcv_hist_sel(Datum *mcv_values, float4 *mcv_numbers, int mcv_nvalues, Datum *hist_values, int hist_nvalues, int opr_codenum)
{
  Selectivity selec = 0.0;
  int i;

     
                                                                            
                                
     
  opr_codenum = -opr_codenum;

  for (i = 0; i < mcv_nvalues; i++)
  {
    selec += mcv_numbers[i] * inet_hist_value_sel(hist_values, hist_nvalues, mcv_values[i], opr_codenum);
  }
  return selec;
}

   
                                                           
   
                                                                           
                                                                          
                                                                        
                                                                     
                                                                     
               
   
                                                                            
                                                                           
                                                                           
   
static Selectivity
inet_hist_inclusion_join_sel(Datum *hist1_values, int hist1_nvalues, Datum *hist2_values, int hist2_nvalues, int opr_codenum)
{
  double match = 0.0;
  int i, k, n;

  if (hist2_nvalues <= 2)
  {
    return 0.0;                                     
  }

                                                                           
  k = (hist2_nvalues - 3) / MAX_CONSIDERED_ELEMS + 1;

  n = 0;
  for (i = 1; i < hist2_nvalues - 1; i += k)
  {
    match += inet_hist_value_sel(hist1_values, hist1_nvalues, hist2_values[i], opr_codenum);
    n++;
  }

  return match / n;
}

   
                                                       
   
                                                                          
                                                                        
                                                                           
               
   
                                                                              
                                                                            
                                                                         
                                                                               
                                                              
   
                                                                          
                                                                           
            
   
                                                                           
                                                                          
                                                                              
                                                                           
                                                                
   
static Selectivity
inet_semi_join_sel(Datum lhs_value, bool mcv_exists, Datum *mcv_values, int mcv_nvalues, bool hist_exists, Datum *hist_values, int hist_nvalues, double hist_weight, FmgrInfo *proc, int opr_codenum)
{
  if (mcv_exists)
  {
    int i;

    for (i = 0; i < mcv_nvalues; i++)
    {
      if (DatumGetBool(FunctionCall2(proc, lhs_value, mcv_values[i])))
      {
        return 1.0;
      }
    }
  }

  if (hist_exists && hist_weight > 0)
  {
    Selectivity hist_selec;

                                                                      
    hist_selec = inet_hist_value_sel(hist_values, hist_nvalues, lhs_value, -opr_codenum);

    if (hist_selec > 0)
    {
      return Min(1.0, hist_weight * hist_selec);
    }
  }

  return 0.0;
}

   
                                                                         
   
                                                                          
                                                                        
                                                                          
             
   
static int
inet_opr_codenum(Oid operator)
{
  switch (operator)
  {
  case OID_INET_SUP_OP:
    return -2;
  case OID_INET_SUPEQ_OP:
    return -1;
  case OID_INET_OVERLAP_OP:
    return 0;
  case OID_INET_SUBEQ_OP:
    return 1;
  case OID_INET_SUB_OP:
    return 2;
  default:
    elog(ERROR, "unrecognized operator %u for inet selectivity", operator);
  }
  return 0;                                         
}

   
                                                                  
   
                                                                              
                                                                             
                                           
   
                                                                            
                                                                           
                                                                        
                                                                              
                  
   
                                                                           
                                                                         
                                                                           
                                                                              
                                                                             
                                                                            
                                                        
   
static int
inet_inclusion_cmp(inet *left, inet *right, int opr_codenum)
{
  if (ip_family(left) == ip_family(right))
  {
    int order;

    order = bitncmp(ip_addr(left), ip_addr(right), Min(ip_bits(left), ip_bits(right)));
    if (order != 0)
    {
      return order;
    }

    return inet_masklen_inclusion_cmp(left, right, opr_codenum);
  }

  return ip_family(left) - ip_family(right);
}

   
                                                                          
   
                                                                               
                                                                             
                                                                     
                                 
   
static int
inet_masklen_inclusion_cmp(inet *left, inet *right, int opr_codenum)
{
  int order;

  order = (int)ip_bits(left) - (int)ip_bits(right);

     
                                                                         
                                                                  
     
  if ((order > 0 && opr_codenum >= 0) || (order == 0 && opr_codenum >= -1 && opr_codenum <= 1) || (order < 0 && opr_codenum <= 0))
  {
    return 0;
  }

     
                                                                           
                                                                          
                                                             
     
  return opr_codenum;
}

   
                                                    
   
                                                                              
                                                                             
                                                                            
                                                                   
   
                                                                         
   
static int
inet_hist_match_divider(inet *boundary, inet *query, int opr_codenum)
{
  if (ip_family(boundary) == ip_family(query) && inet_masklen_inclusion_cmp(boundary, query, opr_codenum) == 0)
  {
    int min_bits, decisive_bits;

    min_bits = Min(ip_bits(boundary), ip_bits(query));

       
                                                                           
                                        
       
    if (opr_codenum < 0)
    {
      decisive_bits = ip_bits(boundary);
    }
    else if (opr_codenum > 0)
    {
      decisive_bits = ip_bits(query);
    }
    else
    {
      decisive_bits = min_bits;
    }

       
                                                                         
                                                                     
       
    if (min_bits > 0)
    {
      return decisive_bits - bitncommon(ip_addr(boundary), ip_addr(query), min_bits);
    }
    return decisive_bits;
  }

  return -1;
}
