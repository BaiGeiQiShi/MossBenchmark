                                                                            
   
                 
                                                                 
   
                                                                         
   
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/pg_statistic.h"
#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "nodes/nodes.h"
#include "tsearch/ts_type.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/selfuncs.h"
#include "utils/syscache.h"

   
                                                                       
                                                                         
                               
   
#define DEFAULT_TS_MATCH_SEL 0.005

                                                            
typedef struct
{
  text *element;
  float4 frequency;
} TextFreq;

                                                                
typedef struct
{
  char *lexeme;
  int length;
} LexemeKey;

static Selectivity
tsquerysel(VariableStatData *vardata, Datum constval);
static Selectivity
mcelem_tsquery_selec(TSQuery query, Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers);
static Selectivity
tsquery_opr_selec(QueryItem *item, char *operand, TextFreq *lookup, int length, float4 minfreq);
static int
compare_lexeme_textfreq(const void *e1, const void *e2);

#define tsquery_opr_selec_no_stats(query) tsquery_opr_selec(GETQUERY(query), GETOPERAND(query), NULL, 0, 0)

   
                                     
   
                                                                
                       
   
Datum
tsmatchsel(PG_FUNCTION_ARGS)
{
  PlannerInfo *root = (PlannerInfo *)PG_GETARG_POINTER(0);

#ifdef NOT_USED
  Oid operator= PG_GETARG_OID(1);
#endif
  List *args = (List *)PG_GETARG_POINTER(2);
  int varRelid = PG_GETARG_INT32(3);
  VariableStatData vardata;
  Node *other;
  bool varonleft;
  Selectivity selec;

     
                                                                             
                                         
     
  if (!get_restriction_variable(root, args, varRelid, &vardata, &other, &varonleft))
  {
    PG_RETURN_FLOAT8(DEFAULT_TS_MATCH_SEL);
  }

     
                                                                          
     
  if (!IsA(other, Const))
  {
    ReleaseVariableStats(vardata);
    PG_RETURN_FLOAT8(DEFAULT_TS_MATCH_SEL);
  }

     
                                                                      
     
  if (((Const *)other)->constisnull)
  {
    ReleaseVariableStats(vardata);
    PG_RETURN_FLOAT8(0.0);
  }

     
                                                                         
                                                                          
                                                                       
     
  if (((Const *)other)->consttype == TSQUERYOID)
  {
                                                     
    Assert(vardata.vartype == TSVECTOROID);

    selec = tsquerysel(&vardata, ((Const *)other)->constvalue);
  }
  else
  {
                                                        
    selec = DEFAULT_TS_MATCH_SEL;
  }

  ReleaseVariableStats(vardata);

  CLAMP_PROBABILITY(selec);

  PG_RETURN_FLOAT8((float8)selec);
}

   
                                              
   
                                                                             
   
Datum
tsmatchjoinsel(PG_FUNCTION_ARGS)
{
                                   
  PG_RETURN_FLOAT8(DEFAULT_TS_MATCH_SEL);
}

   
                                                       
   
static Selectivity
tsquerysel(VariableStatData *vardata, Datum constval)
{
  Selectivity selec;
  TSQuery query;

                                                                  
  query = DatumGetTSQuery(constval);

                                   
  if (query->size == 0)
  {
    return (Selectivity)0.0;
  }

  if (HeapTupleIsValid(vardata->statsTuple))
  {
    Form_pg_statistic stats;
    AttStatsSlot sslot;

    stats = (Form_pg_statistic)GETSTRUCT(vardata->statsTuple);

                                                                        
    if (get_attstatsslot(&sslot, vardata->statsTuple, STATISTIC_KIND_MCELEM, InvalidOid, ATTSTATSSLOT_VALUES | ATTSTATSSLOT_NUMBERS))
    {
         
                                                                       
                   
         
      selec = mcelem_tsquery_selec(query, sslot.values, sslot.nvalues, sslot.numbers, sslot.nnumbers);
      free_attstatsslot(&sslot);
    }
    else
    {
                                                       
      selec = tsquery_opr_selec_no_stats(query);
    }

       
                                                                    
       
    selec *= (1.0 - stats->stanullfrac);
  }
  else
  {
                                        
    selec = tsquery_opr_selec_no_stats(query);
                                                               
  }

  return selec;
}

   
                                                                 
   
static Selectivity
mcelem_tsquery_selec(TSQuery query, Datum *mcelem, int nmcelem, float4 *numbers, int nnumbers)
{
  float4 minfreq;
  TextFreq *lookup;
  Selectivity selec;
  int i;

     
                                                                        
                                                                      
     
                                                                           
                                                                            
                                       
     
  if (nnumbers != nmcelem + 2)
  {
    return tsquery_opr_selec_no_stats(query);
  }

     
                                                                     
     
  lookup = (TextFreq *)palloc(sizeof(TextFreq) * nmcelem);
  for (i = 0; i < nmcelem; i++)
  {
       
                                                                         
                                                            
       
    Assert(!VARATT_IS_COMPRESSED(mcelem[i]) && !VARATT_IS_EXTERNAL(mcelem[i]));
    lookup[i].element = (text *)DatumGetPointer(mcelem[i]);
    lookup[i].frequency = numbers[i];
  }

     
                                                                             
                                                                            
     
  minfreq = numbers[nnumbers - 2];

  selec = tsquery_opr_selec(GETQUERY(query), GETOPERAND(query), lookup, nmcelem, minfreq);

  pfree(lookup);

  return selec;
}

   
                                                                 
   
                                                                
   
                                           
                                                      
   
                                  
   
                                                   
   
                                                            
                                                     
   
                                                                           
                                               
   
                                                                     
                                                                         
                      
   
static Selectivity
tsquery_opr_selec(QueryItem *item, char *operand, TextFreq *lookup, int length, float4 minfreq)
{
  Selectivity selec;

                                                                          
  check_stack_depth();

  if (item->type == QI_VAL)
  {
    QueryOperand *oper = (QueryOperand *)item;
    LexemeKey key;

       
                                      
       
    key.lexeme = operand + oper->distance;
    key.length = oper->length;

    if (oper->prefix)
    {
                                                       
      Selectivity matched, allmces;
      int i, n_matched;

         
                                                                         
                                                                 
                                                                       
                                                                         
                                                      
                                                                    
                                                                  
                                                                 
         
                                                                     
                                                                         
                                                                     
         
      if (lookup == NULL || length < 100)
      {
        return (Selectivity)(DEFAULT_TS_MATCH_SEL * 4);
      }

      matched = allmces = 0;
      n_matched = 0;
      for (i = 0; i < length; i++)
      {
        TextFreq *t = lookup + i;
        int tlen = VARSIZE_ANY_EXHDR(t->element);

        if (tlen >= key.length && strncmp(key.lexeme, VARDATA_ANY(t->element), key.length) == 0)
        {
          matched += t->frequency - matched * t->frequency;
          n_matched++;
        }
        allmces += t->frequency - allmces * t->frequency;
      }

                                                                
      CLAMP_PROBABILITY(matched);
      CLAMP_PROBABILITY(allmces);

      selec = matched + (1.0 - allmces) * ((double)n_matched / length);

         
                                                                        
                                                                  
                                                                     
                                                         
         
      selec = Max(Min(DEFAULT_TS_MATCH_SEL, minfreq / 2), selec);
    }
    else
    {
                                      
      TextFreq *searchres;

                                                                  
      if (lookup == NULL)
      {
        return (Selectivity)DEFAULT_TS_MATCH_SEL;
      }

      searchres = (TextFreq *)bsearch(&key, lookup, length, sizeof(TextFreq), compare_lexeme_textfreq);

      if (searchres)
      {
           
                                                                     
                                                           
           
        selec = searchres->frequency;
      }
      else
      {
           
                                                                    
                                                        
           
        selec = Min(DEFAULT_TS_MATCH_SEL, minfreq / 2);
      }
    }
  }
  else
  {
                                             
    Selectivity s1, s2;

    switch (item->qoperator.oper)
    {
    case OP_NOT:
      selec = 1.0 - tsquery_opr_selec(item + 1, operand, lookup, length, minfreq);
      break;

    case OP_PHRASE:
    case OP_AND:
      s1 = tsquery_opr_selec(item + 1, operand, lookup, length, minfreq);
      s2 = tsquery_opr_selec(item + item->qoperator.left, operand, lookup, length, minfreq);
      selec = s1 * s2;
      break;

    case OP_OR:
      s1 = tsquery_opr_selec(item + 1, operand, lookup, length, minfreq);
      s2 = tsquery_opr_selec(item + item->qoperator.left, operand, lookup, length, minfreq);
      selec = s1 + s2 - s1 * s2;
      break;

    default:
      elog(ERROR, "unrecognized operator: %d", item->qoperator.oper);
      selec = 0;                          
      break;
    }
  }

                                                                      
  CLAMP_PROBABILITY(selec);

  return selec;
}

   
                                                                              
                                                                             
                                                                        
                                    
   
static int
compare_lexeme_textfreq(const void *e1, const void *e2)
{
  const LexemeKey *key = (const LexemeKey *)e1;
  const TextFreq *t = (const TextFreq *)e2;
  int len1, len2;

  len1 = key->length;
  len2 = VARSIZE_ANY_EXHDR(t->element);

                                                               
  if (len1 > len2)
  {
    return 1;
  }
  else if (len1 < len2)
  {
    return -1;
  }

                                             
  return strncmp(key->lexeme, VARDATA_ANY(t->element), len1);
}
