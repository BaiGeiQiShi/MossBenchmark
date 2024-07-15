                                                                            
   
                
                                                       
   
                                                                         
                                                                        
   
                  
                                            
   
                                                                            
   

#include "postgres.h"

#include "access/relation.h"
#include "access/table.h"
#include "access/tableam.h"
#include "catalog/partition.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_type.h"
#include "commands/tablecmds.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_coerce.h"
#include "partitioning/partbounds.h"
#include "partitioning/partdesc.h"
#include "partitioning/partprune.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/hashutils.h"
#include "utils/lsyscache.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/ruleutils.h"
#include "utils/syscache.h"

   
                                                                              
                                                     
   

                                   
typedef struct PartitionHashBound
{
  int modulus;
  int remainder;
  int index;
} PartitionHashBound;

                                                          
typedef struct PartitionListValue
{
  int index;
  Datum value;
} PartitionListValue;

                                    
typedef struct PartitionRangeBound
{
  int index;
  Datum *datums;                                         
  PartitionRangeDatumKind *kind;                             
  bool lower;                                                            
} PartitionRangeBound;

static int32
qsort_partition_hbound_cmp(const void *a, const void *b);
static int32
qsort_partition_list_value_cmp(const void *a, const void *b, void *arg);
static int32
qsort_partition_rbound_cmp(const void *a, const void *b, void *arg);
static PartitionBoundInfo
create_hash_bounds(PartitionBoundSpec **boundspecs, int nparts, PartitionKey key, int **mapping);
static PartitionBoundInfo
create_list_bounds(PartitionBoundSpec **boundspecs, int nparts, PartitionKey key, int **mapping);
static PartitionBoundInfo
create_range_bounds(PartitionBoundSpec **boundspecs, int nparts, PartitionKey key, int **mapping);
static PartitionRangeBound *
make_one_partition_rbound(PartitionKey key, int index, List *datums, bool lower);
static int32
partition_hbound_cmp(int modulus1, int remainder1, int modulus2, int remainder2);
static int32
partition_rbound_cmp(int partnatts, FmgrInfo *partsupfunc, Oid *partcollation, Datum *datums1, PartitionRangeDatumKind *kind1, bool lower1, PartitionRangeBound *b2);
static int
partition_range_bsearch(int partnatts, FmgrInfo *partsupfunc, Oid *partcollation, PartitionBoundInfo boundinfo, PartitionRangeBound *probe, bool *is_equal);
static Expr *
make_partition_op_expr(PartitionKey key, int keynum, uint16 strategy, Expr *arg1, Expr *arg2);
static Oid
get_partition_operator(PartitionKey key, int col, StrategyNumber strategy, bool *need_relabel);
static List *
get_qual_for_hash(Relation parent, PartitionBoundSpec *spec);
static List *
get_qual_for_list(Relation parent, PartitionBoundSpec *spec);
static List *
get_qual_for_range(Relation parent, PartitionBoundSpec *spec, bool for_default);
static void
get_range_key_properties(PartitionKey key, int keynum, PartitionRangeDatum *ldatum, PartitionRangeDatum *udatum, ListCell **partexprs_item, Expr **keyCol, Const **lower_val, Const **upper_val);
static List *
get_range_nulltest(PartitionKey key);

   
                           
                                                                           
                                        
   
List *
get_qual_from_partbound(Relation rel, Relation parent, PartitionBoundSpec *spec)
{
  PartitionKey key = RelationGetPartitionKey(parent);
  List *my_qual = NIL;

  Assert(key != NULL);

  switch (key->strategy)
  {
  case PARTITION_STRATEGY_HASH:
    Assert(spec->strategy == PARTITION_STRATEGY_HASH);
    my_qual = get_qual_for_hash(parent, spec);
    break;

  case PARTITION_STRATEGY_LIST:
    Assert(spec->strategy == PARTITION_STRATEGY_LIST);
    my_qual = get_qual_for_list(parent, spec);
    break;

  case PARTITION_STRATEGY_RANGE:
    Assert(spec->strategy == PARTITION_STRATEGY_RANGE);
    my_qual = get_qual_for_range(parent, spec, false);
    break;

  default:
    elog(ERROR, "unexpected partition strategy: %d", (int)key->strategy);
  }

  return my_qual;
}

   
                           
                                                                        
          
   
                                                                          
                                                                              
                                                                     
                                                                           
                                                                              
                                                                               
                                                                             
                                                  
   
                                                                  
                                                                              
                                       
   
                                                                           
                           
   
PartitionBoundInfo
partition_bounds_create(PartitionBoundSpec **boundspecs, int nparts, PartitionKey key, int **mapping)
{
  int i;

  Assert(nparts > 0);

     
                                                                         
                                                                           
                                                                           
                                                                           
                                                                             
                                                                
     
                                                                           
                                                           
     

     
                                                                         
                                                         
     
  *mapping = (int *)palloc(sizeof(int) * nparts);
  for (i = 0; i < nparts; i++)
  {
    (*mapping)[i] = -1;
  }

  switch (key->strategy)
  {
  case PARTITION_STRATEGY_HASH:
    return create_hash_bounds(boundspecs, nparts, key, mapping);

  case PARTITION_STRATEGY_LIST:
    return create_list_bounds(boundspecs, nparts, key, mapping);

  case PARTITION_STRATEGY_RANGE:
    return create_range_bounds(boundspecs, nparts, key, mapping);

  default:
    elog(ERROR, "unexpected partition strategy: %d", (int)key->strategy);
    break;
  }

  Assert(false);
  return NULL;                          
}

   
                      
                                                             
   
static PartitionBoundInfo
create_hash_bounds(PartitionBoundSpec **boundspecs, int nparts, PartitionKey key, int **mapping)
{
  PartitionBoundInfo boundinfo;
  PartitionHashBound **hbounds = NULL;
  int i;
  int ndatums = 0;
  int greatest_modulus;

  boundinfo = (PartitionBoundInfoData *)palloc0(sizeof(PartitionBoundInfoData));
  boundinfo->strategy = key->strategy;
                                   
  boundinfo->null_index = -1;
  boundinfo->default_index = -1;

  ndatums = nparts;
  hbounds = (PartitionHashBound **)palloc(nparts * sizeof(PartitionHashBound *));

                                                        
  for (i = 0; i < nparts; i++)
  {
    PartitionBoundSpec *spec = boundspecs[i];

    if (spec->strategy != PARTITION_STRATEGY_HASH)
    {
      elog(ERROR, "invalid strategy in partition bound spec");
    }

    hbounds[i] = (PartitionHashBound *)palloc(sizeof(PartitionHashBound));
    hbounds[i]->modulus = spec->modulus;
    hbounds[i]->remainder = spec->remainder;
    hbounds[i]->index = i;
  }

                                              
  qsort(hbounds, nparts, sizeof(PartitionHashBound *), qsort_partition_hbound_cmp);

                                                                
  greatest_modulus = hbounds[ndatums - 1]->modulus;

  boundinfo->ndatums = ndatums;
  boundinfo->datums = (Datum **)palloc0(ndatums * sizeof(Datum *));
  boundinfo->nindexes = greatest_modulus;
  boundinfo->indexes = (int *)palloc(greatest_modulus * sizeof(int));
  for (i = 0; i < greatest_modulus; i++)
  {
    boundinfo->indexes[i] = -1;
  }

     
                                                                            
                                                                             
                        
     
  for (i = 0; i < nparts; i++)
  {
    int modulus = hbounds[i]->modulus;
    int remainder = hbounds[i]->remainder;

    boundinfo->datums[i] = (Datum *)palloc(2 * sizeof(Datum));
    boundinfo->datums[i][0] = Int32GetDatum(modulus);
    boundinfo->datums[i][1] = Int32GetDatum(remainder);

    while (remainder < greatest_modulus)
    {
                    
      Assert(boundinfo->indexes[remainder] == -1);
      boundinfo->indexes[remainder] = i;
      remainder += modulus;
    }

    (*mapping)[hbounds[i]->index] = i;
    pfree(hbounds[i]);
  }
  pfree(hbounds);

  return boundinfo;
}

   
                      
                                                             
   
static PartitionBoundInfo
create_list_bounds(PartitionBoundSpec **boundspecs, int nparts, PartitionKey key, int **mapping)
{
  PartitionBoundInfo boundinfo;
  PartitionListValue **all_values = NULL;
  ListCell *cell;
  int i = 0;
  int ndatums = 0;
  int next_index = 0;
  int default_index = -1;
  int null_index = -1;
  List *non_null_values = NIL;

  boundinfo = (PartitionBoundInfoData *)palloc0(sizeof(PartitionBoundInfoData));
  boundinfo->strategy = key->strategy;
                                    
  boundinfo->null_index = -1;
  boundinfo->default_index = -1;

                                                                       
  for (i = 0; i < nparts; i++)
  {
    PartitionBoundSpec *spec = boundspecs[i];
    ListCell *c;

    if (spec->strategy != PARTITION_STRATEGY_LIST)
    {
      elog(ERROR, "invalid strategy in partition bound spec");
    }

       
                                                                  
                                                                          
                           
       
    if (spec->is_default)
    {
      default_index = i;
      continue;
    }

    foreach (c, spec->listdatums)
    {
      Const *val = castNode(Const, lfirst(c));
      PartitionListValue *list_value = NULL;

      if (!val->constisnull)
      {
        list_value = (PartitionListValue *)palloc0(sizeof(PartitionListValue));
        list_value->index = i;
        list_value->value = val->constvalue;
      }
      else
      {
           
                                                                     
                                                     
           
        if (null_index != -1)
        {
          elog(ERROR, "found null more than once");
        }
        null_index = i;
      }

      if (list_value)
      {
        non_null_values = lappend(non_null_values, list_value);
      }
    }
  }

  ndatums = list_length(non_null_values);

     
                                                                             
                                                  
     
  all_values = (PartitionListValue **)palloc(ndatums * sizeof(PartitionListValue *));
  i = 0;
  foreach (cell, non_null_values)
  {
    PartitionListValue *src = lfirst(cell);

    all_values[i] = (PartitionListValue *)palloc(sizeof(PartitionListValue));
    all_values[i]->value = src->value;
    all_values[i]->index = src->index;
    i++;
  }

  qsort_arg(all_values, ndatums, sizeof(PartitionListValue *), qsort_partition_list_value_cmp, (void *)key);

  boundinfo->ndatums = ndatums;
  boundinfo->datums = (Datum **)palloc0(ndatums * sizeof(Datum *));
  boundinfo->nindexes = ndatums;
  boundinfo->indexes = (int *)palloc(ndatums * sizeof(int));

     
                                                                            
                                                                             
                                                                             
                                                                
     
  for (i = 0; i < ndatums; i++)
  {
    int orig_index = all_values[i]->index;

    boundinfo->datums[i] = (Datum *)palloc(sizeof(Datum));
    boundinfo->datums[i][0] = datumCopy(all_values[i]->value, key->parttypbyval[0], key->parttyplen[0]);

                                                     
    if ((*mapping)[orig_index] == -1)
    {
      (*mapping)[orig_index] = next_index++;
    }

    boundinfo->indexes[i] = (*mapping)[orig_index];
  }

     
                                                     
     
                                                                            
                                                                          
                                                                           
             
     
  if (null_index != -1)
  {
    Assert(null_index >= 0);
    if ((*mapping)[null_index] == -1)
    {
      (*mapping)[null_index] = next_index++;
    }
    boundinfo->null_index = (*mapping)[null_index];
  }

                                                          
  if (default_index != -1)
  {
       
                                                                          
                                                                       
                                            
       
    Assert(default_index >= 0);
    Assert((*mapping)[default_index] == -1);
    (*mapping)[default_index] = next_index++;
    boundinfo->default_index = (*mapping)[default_index];
  }

                                                                     
  Assert(next_index == nparts);
  return boundinfo;
}

   
                       
                                                              
   
static PartitionBoundInfo
create_range_bounds(PartitionBoundSpec **boundspecs, int nparts, PartitionKey key, int **mapping)
{
  PartitionBoundInfo boundinfo;
  PartitionRangeBound **rbounds = NULL;
  PartitionRangeBound **all_bounds, *prev;
  int i, k;
  int ndatums = 0;
  int default_index = -1;
  int next_index = 0;

  boundinfo = (PartitionBoundInfoData *)palloc0(sizeof(PartitionBoundInfoData));
  boundinfo->strategy = key->strategy;
                                                           
  boundinfo->null_index = -1;
                                    
  boundinfo->default_index = -1;

  all_bounds = (PartitionRangeBound **)palloc0(2 * nparts * sizeof(PartitionRangeBound *));

                                                                        
  ndatums = 0;
  for (i = 0; i < nparts; i++)
  {
    PartitionBoundSpec *spec = boundspecs[i];
    PartitionRangeBound *lower, *upper;

    if (spec->strategy != PARTITION_STRATEGY_RANGE)
    {
      elog(ERROR, "invalid strategy in partition bound spec");
    }

       
                                                                  
                                                                       
                       
       
    if (spec->is_default)
    {
      default_index = i;
      continue;
    }

    lower = make_one_partition_rbound(key, i, spec->lowerdatums, true);
    upper = make_one_partition_rbound(key, i, spec->upperdatums, false);
    all_bounds[ndatums++] = lower;
    all_bounds[ndatums++] = upper;
  }

  Assert(ndatums == nparts * 2 || (default_index != -1 && ndatums == (nparts - 1) * 2));

                                              
  qsort_arg(all_bounds, ndatums, sizeof(PartitionRangeBound *), qsort_partition_rbound_cmp, (void *)key);

                                                          
  rbounds = (PartitionRangeBound **)palloc(ndatums * sizeof(PartitionRangeBound *));
  k = 0;
  prev = NULL;
  for (i = 0; i < ndatums; i++)
  {
    PartitionRangeBound *cur = all_bounds[i];
    bool is_distinct = false;
    int j;

                                                              
    for (j = 0; j < key->partnatts; j++)
    {
      Datum cmpval;

      if (prev == NULL || cur->kind[j] != prev->kind[j])
      {
        is_distinct = true;
        break;
      }

         
                                                                         
                                                                  
                  
         
      if (cur->kind[j] != PARTITION_RANGE_DATUM_VALUE)
      {
        break;
      }

      cmpval = FunctionCall2Coll(&key->partsupfunc[j], key->partcollation[j], cur->datums[j], prev->datums[j]);
      if (DatumGetInt32(cmpval) != 0)
      {
        is_distinct = true;
        break;
      }
    }

       
                                                                          
                                                                  
       
    if (is_distinct)
    {
      rbounds[k++] = all_bounds[i];
    }

    prev = cur;
  }

                                                            
  ndatums = k;

     
                                                                           
                                                                            
                                                                     
                                                                             
                                                                       
            
     
  boundinfo->ndatums = ndatums;
  boundinfo->datums = (Datum **)palloc0(ndatums * sizeof(Datum *));
  boundinfo->kind = (PartitionRangeDatumKind **)palloc(ndatums * sizeof(PartitionRangeDatumKind *));

     
                                                                             
                                     
     
  boundinfo->nindexes = ndatums + 1;
  boundinfo->indexes = (int *)palloc((ndatums + 1) * sizeof(int));

  for (i = 0; i < ndatums; i++)
  {
    int j;

    boundinfo->datums[i] = (Datum *)palloc(key->partnatts * sizeof(Datum));
    boundinfo->kind[i] = (PartitionRangeDatumKind *)palloc(key->partnatts * sizeof(PartitionRangeDatumKind));
    for (j = 0; j < key->partnatts; j++)
    {
      if (rbounds[i]->kind[j] == PARTITION_RANGE_DATUM_VALUE)
      {
        boundinfo->datums[i][j] = datumCopy(rbounds[i]->datums[j], key->parttypbyval[j], key->parttyplen[j]);
      }
      boundinfo->kind[i][j] = rbounds[i]->kind[j];
    }

       
                                                
       
                                                                  
                                                                         
                                                                       
                           
       
    if (rbounds[i]->lower)
    {
      boundinfo->indexes[i] = -1;
    }
    else
    {
      int orig_index = rbounds[i]->index;

                                                       
      if ((*mapping)[orig_index] == -1)
      {
        (*mapping)[orig_index] = next_index++;
      }

      boundinfo->indexes[i] = (*mapping)[orig_index];
    }
  }

                                                          
  if (default_index != -1)
  {
    Assert(default_index >= 0 && (*mapping)[default_index] == -1);
    (*mapping)[default_index] = next_index++;
    boundinfo->default_index = (*mapping)[default_index];
  }

                             
  Assert(i == ndatums);
  boundinfo->indexes[i] = -1;

                                                                     
  Assert(next_index == nparts);
  return boundinfo;
}

   
                                                        
   
                                                                          
                                                                            
                                                                      
                                       
   
bool
partition_bounds_equal(int partnatts, int16 *parttyplen, bool *parttypbyval, PartitionBoundInfo b1, PartitionBoundInfo b2)
{
  int i;

  if (b1->strategy != b2->strategy)
  {
    return false;
  }

  if (b1->ndatums != b2->ndatums)
  {
    return false;
  }

  if (b1->nindexes != b2->nindexes)
  {
    return false;
  }

  if (b1->null_index != b2->null_index)
  {
    return false;
  }

  if (b1->default_index != b2->default_index)
  {
    return false;
  }

                                                                        
  for (i = 0; i < b1->nindexes; i++)
  {
    if (b1->indexes[i] != b2->indexes[i])
    {
      return false;
    }
  }

                                            
  if (b1->strategy == PARTITION_STRATEGY_HASH)
  {
       
                                                                        
                                                                    
                                                                          
                                                                         
                                                                         
                                                                           
                                                                        
                                                                          
                       
       
                                                                          
                                                                         
                                                                       
       
#ifdef USE_ASSERT_CHECKING
    for (i = 0; i < b1->ndatums; i++)
    {
      Assert((b1->datums[i][0] == b2->datums[i][0] && b1->datums[i][1] == b2->datums[i][1]));
    }
#endif
  }
  else
  {
    for (i = 0; i < b1->ndatums; i++)
    {
      int j;

      for (j = 0; j < partnatts; j++)
      {
                                                                   
        if (b1->kind != NULL)
        {
                                                                       
          if (b1->kind[i][j] != b2->kind[i][j])
          {
            return false;
          }

             
                                                         
                          
             
          if (b1->kind[i][j] != PARTITION_RANGE_DATUM_VALUE)
          {
            continue;
          }
        }

           
                                                                 
                                                                  
                                                                       
                                                                 
                                                                    
                                                           
                                                                   
                                                                      
                                                                     
                                                                  
                 
           
        if (!datumIsEqual(b1->datums[i][j], b2->datums[i][j], parttypbyval[j], parttyplen[j]))
        {
          return false;
        }
      }
    }
  }
  return true;
}

   
                                                                                 
                                                       
   
PartitionBoundInfo
partition_bounds_copy(PartitionBoundInfo src, PartitionKey key)
{
  PartitionBoundInfo dest;
  int i;
  int ndatums;
  int nindexes;
  int partnatts;

  dest = (PartitionBoundInfo)palloc(sizeof(PartitionBoundInfoData));

  dest->strategy = src->strategy;
  ndatums = dest->ndatums = src->ndatums;
  nindexes = dest->nindexes = src->nindexes;
  partnatts = key->partnatts;

                                                                 
  Assert(key->strategy != PARTITION_STRATEGY_LIST || partnatts == 1);

  dest->datums = (Datum **)palloc(sizeof(Datum *) * ndatums);

  if (src->kind != NULL)
  {
    dest->kind = (PartitionRangeDatumKind **)palloc(ndatums * sizeof(PartitionRangeDatumKind *));
    for (i = 0; i < ndatums; i++)
    {
      dest->kind[i] = (PartitionRangeDatumKind *)palloc(partnatts * sizeof(PartitionRangeDatumKind));

      memcpy(dest->kind[i], src->kind[i], sizeof(PartitionRangeDatumKind) * key->partnatts);
    }
  }
  else
  {
    dest->kind = NULL;
  }

  for (i = 0; i < ndatums; i++)
  {
    int j;

       
                                                                      
                                         
       
    bool hash_part = (key->strategy == PARTITION_STRATEGY_HASH);
    int natts = hash_part ? 2 : partnatts;

    dest->datums[i] = (Datum *)palloc(sizeof(Datum) * natts);

    for (j = 0; j < natts; j++)
    {
      bool byval;
      int typlen;

      if (hash_part)
      {
        typlen = sizeof(int32);                  
        byval = true;                                      
      }
      else
      {
        byval = key->parttypbyval[j];
        typlen = key->parttyplen[j];
      }

      if (dest->kind == NULL || dest->kind[i][j] == PARTITION_RANGE_DATUM_VALUE)
      {
        dest->datums[i][j] = datumCopy(src->datums[i][j], byval, typlen);
      }
    }
  }

  dest->indexes = (int *)palloc(sizeof(int) * nindexes);
  memcpy(dest->indexes, src->indexes, sizeof(int) * nindexes);

  dest->null_index = src->null_index;
  dest->default_index = src->default_index;

  return dest;
}

   
                          
                                                                           
                                                                       
                                                                     
                                                                  
                                            
   
                                                                     
                         
   
bool
partitions_are_ordered(PartitionBoundInfo boundinfo, int nparts)
{
  Assert(boundinfo != NULL);

  switch (boundinfo->strategy)
  {
  case PARTITION_STRATEGY_RANGE:

       
                                                                     
                                                                      
                                                                
                                                                       
                                                                
                                                                      
       
    if (!partition_bound_has_default(boundinfo))
    {
      return true;
    }
    break;

  case PARTITION_STRATEGY_LIST:

       
                                                                      
                                                                    
                                                                   
                                                                     
                                                                  
                                                                      
                                                                      
                                                            
                                                                
                                                                     
                                                                     
                                    
       
    if (partition_bound_has_default(boundinfo))
    {
      return false;
    }

    if (boundinfo->ndatums + partition_bound_accepts_nulls(boundinfo) == nparts)
    {
      return true;
    }
    break;

  default:
                                      
    break;
  }

  return false;
}

   
                             
   
                                                                               
                                                                          
   
void
check_new_partition_bound(char *relname, Relation parent, PartitionBoundSpec *spec)
{
  PartitionKey key = RelationGetPartitionKey(parent);
  PartitionDesc partdesc = RelationGetPartitionDesc(parent);
  PartitionBoundInfo boundinfo = partdesc->boundinfo;
  ParseState *pstate = make_parsestate(NULL);
  int with = -1;
  bool overlap = false;

  if (spec->is_default)
  {
       
                                                                  
                                                                      
                                                                       
             
       
    if (boundinfo == NULL || !partition_bound_has_default(boundinfo))
    {
      return;
    }

                                                      
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("partition \"%s\" conflicts with existing default partition \"%s\"", relname, get_rel_name(partdesc->oids[boundinfo->default_index])), parser_errposition(pstate, spec->location)));
  }

  switch (key->strategy)
  {
  case PARTITION_STRATEGY_HASH:
  {
    Assert(spec->strategy == PARTITION_STRATEGY_HASH);
    Assert(spec->remainder >= 0 && spec->remainder < spec->modulus);

    if (partdesc->nparts > 0)
    {
      Datum **datums = boundinfo->datums;
      int ndatums = boundinfo->ndatums;
      int greatest_modulus;
      int remainder;
      int offset;
      bool valid_modulus = true;
      int prev_modulus,                               
          next_modulus;                           

         
                                                               
                                                                
                                                              
                                                               
                                                              
                                                                
                                
         
                                                                 
                                                             
                                                
         
      offset = partition_hash_bsearch(boundinfo, spec->modulus, spec->remainder);
      if (offset < 0)
      {
        next_modulus = DatumGetInt32(datums[0][0]);
        valid_modulus = (next_modulus % spec->modulus) == 0;
      }
      else
      {
        prev_modulus = DatumGetInt32(datums[offset][0]);
        valid_modulus = (spec->modulus % prev_modulus) == 0;

        if (valid_modulus && (offset + 1) < ndatums)
        {
          next_modulus = DatumGetInt32(datums[offset + 1][0]);
          valid_modulus = (next_modulus % spec->modulus) == 0;
        }
      }

      if (!valid_modulus)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("every hash partition modulus must be a factor of the next larger modulus")));
      }

      greatest_modulus = boundinfo->nindexes;
      remainder = spec->remainder;

         
                                                                 
                                                               
                                                                 
                                                                 
         
      if (remainder >= greatest_modulus)
      {
        remainder = remainder % greatest_modulus;
      }

                                                          
      do
      {
        if (boundinfo->indexes[remainder] != -1)
        {
          overlap = true;
          with = boundinfo->indexes[remainder];
          break;
        }
        remainder += spec->modulus;
      } while (remainder < greatest_modulus);
    }

    break;
  }

  case PARTITION_STRATEGY_LIST:
  {
    Assert(spec->strategy == PARTITION_STRATEGY_LIST);

    if (partdesc->nparts > 0)
    {
      ListCell *cell;

      Assert(boundinfo && boundinfo->strategy == PARTITION_STRATEGY_LIST && (boundinfo->ndatums > 0 || partition_bound_accepts_nulls(boundinfo) || partition_bound_has_default(boundinfo)));

      foreach (cell, spec->listdatums)
      {
        Const *val = castNode(Const, lfirst(cell));

        if (!val->constisnull)
        {
          int offset;
          bool equal;

          offset = partition_list_bsearch(&key->partsupfunc[0], key->partcollation, boundinfo, val->constvalue, &equal);
          if (offset >= 0 && equal)
          {
            overlap = true;
            with = boundinfo->indexes[offset];
            break;
          }
        }
        else if (partition_bound_accepts_nulls(boundinfo))
        {
          overlap = true;
          with = boundinfo->null_index;
          break;
        }
      }
    }

    break;
  }

  case PARTITION_STRATEGY_RANGE:
  {
    PartitionRangeBound *lower, *upper;

    Assert(spec->strategy == PARTITION_STRATEGY_RANGE);
    lower = make_one_partition_rbound(key, -1, spec->lowerdatums, true);
    upper = make_one_partition_rbound(key, -1, spec->upperdatums, false);

       
                                                              
                                        
       
    if (partition_rbound_cmp(key->partnatts, key->partsupfunc, key->partcollation, lower->datums, lower->kind, true, upper) >= 0)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("empty range bound specified for partition \"%s\"", relname), errdetail("Specified lower bound %s is greater than or equal to upper bound %s.", get_range_partbound_string(spec->lowerdatums), get_range_partbound_string(spec->upperdatums)), parser_errposition(pstate, spec->location)));
    }

    if (partdesc->nparts > 0)
    {
      int offset;
      bool equal;

      Assert(boundinfo && boundinfo->strategy == PARTITION_STRATEGY_RANGE && (boundinfo->ndatums > 0 || partition_bound_has_default(boundinfo)));

         
                                                            
                                                               
                                             
         
                                                            
                                                               
                                     
         
                                                               
                                                               
                                                                 
                                                               
                     
         
      offset = partition_range_bsearch(key->partnatts, key->partsupfunc, key->partcollation, boundinfo, lower, &equal);

      if (boundinfo->indexes[offset + 1] < 0)
      {
           
                                                             
                                                           
                                                        
                                       
           
        if (offset + 1 < boundinfo->ndatums)
        {
          int32 cmpval;
          Datum *datums;
          PartitionRangeDatumKind *kind;
          bool is_lower;

          datums = boundinfo->datums[offset + 1];
          kind = boundinfo->kind[offset + 1];
          is_lower = (boundinfo->indexes[offset + 1] == -1);

          cmpval = partition_rbound_cmp(key->partnatts, key->partsupfunc, key->partcollation, datums, kind, is_lower, upper);
          if (cmpval < 0)
          {
               
                                                   
                                                         
                           
               
            overlap = true;
            with = boundinfo->indexes[offset + 2];
          }
        }
      }
      else
      {
           
                                                        
                                                    
           
        overlap = true;
        with = boundinfo->indexes[offset + 1];
      }
    }

    break;
  }

  default:
    elog(ERROR, "unexpected partition strategy: %d", (int)key->strategy);
  }

  if (overlap)
  {
    Assert(with >= 0);
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("partition \"%s\" would overlap partition \"%s\"", relname, get_rel_name(partdesc->oids[with])), parser_errposition(pstate, spec->location)));
  }
}

   
                                    
   
                                                                            
                                                                             
                       
   
void
check_default_partition_contents(Relation parent, Relation default_rel, PartitionBoundSpec *new_spec)
{
  List *new_part_constraints;
  List *def_part_constraints;
  List *all_parts;
  ListCell *lc;

  new_part_constraints = (new_spec->strategy == PARTITION_STRATEGY_LIST) ? get_qual_for_list(parent, new_spec) : get_qual_for_range(parent, new_spec, false);
  def_part_constraints = get_proposed_default_constraint(new_part_constraints);

     
                                                                       
                    
     
  def_part_constraints = map_partition_varattnos(def_part_constraints, 1, default_rel, parent, NULL);

     
                                                                             
                                                                        
                                           
     
  if (PartConstraintImpliedByRelConstraint(default_rel, def_part_constraints))
  {
    ereport(DEBUG1, (errmsg("updated partition constraint for default partition \"%s\" is implied by existing constraints", RelationGetRelationName(default_rel))));
    return;
  }

     
                                                                          
                                                            
     
  if (default_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    all_parts = find_all_inheritors(RelationGetRelid(default_rel), AccessExclusiveLock, NULL);
  }
  else
  {
    all_parts = list_make1_oid(RelationGetRelid(default_rel));
  }

  foreach (lc, all_parts)
  {
    Oid part_relid = lfirst_oid(lc);
    Relation part_rel;
    Expr *partition_constraint;
    EState *estate;
    ExprState *partqualstate = NULL;
    Snapshot snapshot;
    ExprContext *econtext;
    TableScanDesc scan;
    MemoryContext oldCxt;
    TupleTableSlot *tupslot;

                                   
    if (part_relid != RelationGetRelid(default_rel))
    {
      part_rel = table_open(part_relid, NoLock);

         
                                                                      
                              
         
      partition_constraint = make_ands_explicit(def_part_constraints);
      partition_constraint = (Expr *)map_partition_varattnos((List *)partition_constraint, 1, part_rel, default_rel, NULL);

         
                                                                       
                                                                       
                                                           
         
      if (PartConstraintImpliedByRelConstraint(part_rel, def_part_constraints))
      {
        ereport(DEBUG1, (errmsg("updated partition constraint for default partition \"%s\" is implied by existing constraints", RelationGetRelationName(part_rel))));

        table_close(part_rel, NoLock);
        continue;
      }
    }
    else
    {
      part_rel = default_rel;
      partition_constraint = make_ands_explicit(def_part_constraints);
    }

       
                                                                         
                
       
    if (part_rel->rd_rel->relkind != RELKIND_RELATION)
    {
      if (part_rel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
      {
        ereport(WARNING, (errcode(ERRCODE_CHECK_VIOLATION), errmsg("skipped scanning foreign table \"%s\" which is a partition of default partition \"%s\"", RelationGetRelationName(part_rel), RelationGetRelationName(default_rel))));
      }

      if (RelationGetRelid(default_rel) != RelationGetRelid(part_rel))
      {
        table_close(part_rel, NoLock);
      }

      continue;
    }

    estate = CreateExecutorState();

                                                                     
    partqualstate = ExecPrepareExpr(partition_constraint, estate);

    econtext = GetPerTupleExprContext(estate);
    snapshot = RegisterSnapshot(GetLatestSnapshot());
    tupslot = table_slot_create(part_rel, &estate->es_tupleTable);
    scan = table_beginscan(part_rel, snapshot, 0, NULL);

       
                                                                      
                                          
       
    oldCxt = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

    while (table_scan_getnextslot(scan, ForwardScanDirection, tupslot))
    {
      econtext->ecxt_scantuple = tupslot;

      if (!ExecCheck(partqualstate, econtext))
      {
        ereport(ERROR, (errcode(ERRCODE_CHECK_VIOLATION), errmsg("updated partition constraint for default partition \"%s\" would be violated by some row", RelationGetRelationName(default_rel))));
      }

      ResetExprContext(econtext);
      CHECK_FOR_INTERRUPTS();
    }

    MemoryContextSwitchTo(oldCxt);
    table_endscan(scan);
    UnregisterSnapshot(snapshot);
    ExecDropSingleTupleTableSlot(tupslot);
    FreeExecutorState(estate);

    if (RelationGetRelid(default_rel) != RelationGetRelid(part_rel))
    {
      table_close(part_rel, NoLock);                                 
    }
  }
}

   
                                       
   
                                                             
                                                                  
                                          
   
int
get_hash_partition_greatest_modulus(PartitionBoundInfo bound)
{
  Assert(bound && bound->strategy == PARTITION_STRATEGY_HASH);
  return bound->nindexes;
}

   
                             
   
                                                                             
                                                                               
                                                                    
   
static PartitionRangeBound *
make_one_partition_rbound(PartitionKey key, int index, List *datums, bool lower)
{
  PartitionRangeBound *bound;
  ListCell *lc;
  int i;

  Assert(datums != NIL);

  bound = (PartitionRangeBound *)palloc0(sizeof(PartitionRangeBound));
  bound->index = index;
  bound->datums = (Datum *)palloc0(key->partnatts * sizeof(Datum));
  bound->kind = (PartitionRangeDatumKind *)palloc0(key->partnatts * sizeof(PartitionRangeDatumKind));
  bound->lower = lower;

  i = 0;
  foreach (lc, datums)
  {
    PartitionRangeDatum *datum = castNode(PartitionRangeDatum, lfirst(lc));

                                               
    bound->kind[i] = datum->kind;

    if (datum->kind == PARTITION_RANGE_DATUM_VALUE)
    {
      Const *val = castNode(Const, datum->value);

      if (val->constisnull)
      {
        elog(ERROR, "invalid range bound datum");
      }
      bound->datums[i] = val->constvalue;
    }

    i++;
  }

  return bound;
}

   
                        
   
                                                                          
                                                                
   
                                                                                 
                                                                               
                             
   
                                                                               
                                                                              
                                                                             
                                                                       
                                                                             
                              
   
static int32
partition_rbound_cmp(int partnatts, FmgrInfo *partsupfunc, Oid *partcollation, Datum *datums1, PartitionRangeDatumKind *kind1, bool lower1, PartitionRangeBound *b2)
{
  int32 cmpval = 0;                       
  int i;
  Datum *datums2 = b2->datums;
  PartitionRangeDatumKind *kind2 = b2->kind;
  bool lower2 = b2->lower;

  for (i = 0; i < partnatts; i++)
  {
       
                                                                           
                                                                          
                                                                    
                                                          
       
    if (kind1[i] < kind2[i])
    {
      return -1;
    }
    else if (kind1[i] > kind2[i])
    {
      return 1;
    }
    else if (kind1[i] != PARTITION_RANGE_DATUM_VALUE)
    {

         
                                                                        
                                                                    
                                                 
         
      break;
    }

    cmpval = DatumGetInt32(FunctionCall2Coll(&partsupfunc[i], partcollation[i], datums1[i], datums2[i]));
    if (cmpval != 0)
    {
      break;
    }
  }

     
                                                                         
                                                                            
                                                                             
          
     
  if (cmpval == 0 && lower1 != lower2)
  {
    cmpval = lower1 ? 1 : -1;
  }

  return cmpval;
}

   
                              
   
                                                                              
                                                       
   
                                                                              
                                                                                
                       
   
   
int32
partition_rbound_datum_cmp(FmgrInfo *partsupfunc, Oid *partcollation, Datum *rb_datums, PartitionRangeDatumKind *rb_kind, Datum *tuple_datums, int n_tuple_datums)
{
  int i;
  int32 cmpval = -1;

  for (i = 0; i < n_tuple_datums; i++)
  {
    if (rb_kind[i] == PARTITION_RANGE_DATUM_MINVALUE)
    {
      return -1;
    }
    else if (rb_kind[i] == PARTITION_RANGE_DATUM_MAXVALUE)
    {
      return 1;
    }

    cmpval = DatumGetInt32(FunctionCall2Coll(&partsupfunc[i], partcollation[i], rb_datums[i], tuple_datums[i]));
    if (cmpval != 0)
    {
      break;
    }
  }

  return cmpval;
}

   
                        
   
                                                               
   
static int32
partition_hbound_cmp(int modulus1, int remainder1, int modulus2, int remainder2)
{
  if (modulus1 < modulus2)
  {
    return -1;
  }
  if (modulus1 > modulus2)
  {
    return 1;
  }
  if (modulus1 == modulus2 && remainder1 != remainder2)
  {
    return (remainder1 > remainder2) ? 1 : -1;
  }
  return 0;
}

   
                          
                                                                          
                                                                     
   
                                                                              
                       
   
int
partition_list_bsearch(FmgrInfo *partsupfunc, Oid *partcollation, PartitionBoundInfo boundinfo, Datum value, bool *is_equal)
{
  int lo, hi, mid;

  lo = -1;
  hi = boundinfo->ndatums - 1;
  while (lo < hi)
  {
    int32 cmpval;

    mid = (lo + hi + 1) / 2;
    cmpval = DatumGetInt32(FunctionCall2Coll(&partsupfunc[0], partcollation[0], boundinfo->datums[mid][0], value));
    if (cmpval <= 0)
    {
      lo = mid;
      *is_equal = (cmpval == 0);
      if (*is_equal)
      {
        break;
      }
    }
    else
    {
      hi = mid - 1;
    }
  }

  return lo;
}

   
                           
                                                                       
                                                                        
            
   
                                                                              
                            
   
static int
partition_range_bsearch(int partnatts, FmgrInfo *partsupfunc, Oid *partcollation, PartitionBoundInfo boundinfo, PartitionRangeBound *probe, bool *is_equal)
{
  int lo, hi, mid;

  lo = -1;
  hi = boundinfo->ndatums - 1;
  while (lo < hi)
  {
    int32 cmpval;

    mid = (lo + hi + 1) / 2;
    cmpval = partition_rbound_cmp(partnatts, partsupfunc, partcollation, boundinfo->datums[mid], boundinfo->kind[mid], (boundinfo->indexes[mid] == -1), probe);
    if (cmpval <= 0)
    {
      lo = mid;
      *is_equal = (cmpval == 0);

      if (*is_equal)
      {
        break;
      }
    }
    else
    {
      hi = mid - 1;
    }
  }

  return lo;
}

   
                           
                                                                       
                                                                          
   
                                                                              
                       
   
int
partition_range_datum_bsearch(FmgrInfo *partsupfunc, Oid *partcollation, PartitionBoundInfo boundinfo, int nvalues, Datum *values, bool *is_equal)
{
  int lo, hi, mid;

  lo = -1;
  hi = boundinfo->ndatums - 1;
  while (lo < hi)
  {
    int32 cmpval;

    mid = (lo + hi + 1) / 2;
    cmpval = partition_rbound_datum_cmp(partsupfunc, partcollation, boundinfo->datums[mid], boundinfo->kind[mid], values, nvalues);
    if (cmpval <= 0)
    {
      lo = mid;
      *is_equal = (cmpval == 0);

      if (*is_equal)
      {
        break;
      }
    }
    else
    {
      hi = mid - 1;
    }
  }

  return lo;
}

   
                          
                                                                        
                                                                       
                            
   
int
partition_hash_bsearch(PartitionBoundInfo boundinfo, int modulus, int remainder)
{
  int lo, hi, mid;

  lo = -1;
  hi = boundinfo->ndatums - 1;
  while (lo < hi)
  {
    int32 cmpval, bound_modulus, bound_remainder;

    mid = (lo + hi + 1) / 2;
    bound_modulus = DatumGetInt32(boundinfo->datums[mid][0]);
    bound_remainder = DatumGetInt32(boundinfo->datums[mid][1]);
    cmpval = partition_hbound_cmp(bound_modulus, bound_remainder, modulus, remainder);
    if (cmpval <= 0)
    {
      lo = mid;

      if (cmpval == 0)
      {
        break;
      }
    }
    else
    {
      hi = mid - 1;
    }
  }

  return lo;
}

   
                              
   
                                                         
   
static int32
qsort_partition_hbound_cmp(const void *a, const void *b)
{
  PartitionHashBound *h1 = (*(PartitionHashBound *const *)a);
  PartitionHashBound *h2 = (*(PartitionHashBound *const *)b);

  return partition_hbound_cmp(h1->modulus, h1->remainder, h2->modulus, h2->remainder);
}

   
                                  
   
                                            
   
static int32
qsort_partition_list_value_cmp(const void *a, const void *b, void *arg)
{
  Datum val1 = (*(PartitionListValue *const *)a)->value, val2 = (*(PartitionListValue *const *)b)->value;
  PartitionKey key = (PartitionKey)arg;

  return DatumGetInt32(FunctionCall2Coll(&key->partsupfunc[0], key->partcollation[0], val1, val2));
}

   
                              
   
                                                               
   
static int32
qsort_partition_rbound_cmp(const void *a, const void *b, void *arg)
{
  PartitionRangeBound *b1 = (*(PartitionRangeBound *const *)a);
  PartitionRangeBound *b2 = (*(PartitionRangeBound *const *)b);
  PartitionKey key = (PartitionKey)arg;

  return partition_rbound_cmp(key->partnatts, key->partsupfunc, key->partcollation, b1->datums, b1->kind, b1->lower, b2);
}

   
                          
   
                                                                            
                                                                               
                                                                           
                                                                        
                                                                             
          
   
static Oid
get_partition_operator(PartitionKey key, int col, StrategyNumber strategy, bool *need_relabel)
{
  Oid operoid;

     
                                                                      
                                                      
     
  operoid = get_opfamily_member(key->partopfamily[col], key->partopcintype[col], key->partopcintype[col], strategy);
  if (!OidIsValid(operoid))
  {
    elog(ERROR, "missing operator %d(%u,%u) in partition opfamily %u", strategy, key->partopcintype[col], key->partopcintype[col], key->partopfamily[col]);
  }

     
                                                                         
                                                                             
                                                               
     
  *need_relabel = (key->parttypid[col] != key->partopcintype[col] && key->partopcintype[col] != RECORDOID && !IsPolymorphicType(key->partopcintype[col]));

  return operoid;
}

   
                          
                                                                     
                                                 
   
static Expr *
make_partition_op_expr(PartitionKey key, int keynum, uint16 strategy, Expr *arg1, Expr *arg2)
{
  Oid operoid;
  bool need_relabel = false;
  Expr *result = NULL;

                                                                   
  operoid = get_partition_operator(key, keynum, strategy, &need_relabel);

     
                                                                        
                                                    
                               
     
  if (!IsA(arg1, Const) && (need_relabel || key->partcollation[keynum] != key->parttypcoll[keynum]))
  {
    arg1 = (Expr *)makeRelabelType(arg1, key->partopcintype[keynum], -1, key->partcollation[keynum], COERCE_EXPLICIT_CAST);
  }

                                      
  switch (key->strategy)
  {
  case PARTITION_STRATEGY_LIST:
  {
    List *elems = (List *)arg2;
    int nelems = list_length(elems);

    Assert(nelems >= 1);
    Assert(keynum == 0);

    if (nelems > 1 && !type_is_array(key->parttypid[keynum]))
    {
      ArrayExpr *arrexpr;
      ScalarArrayOpExpr *saopexpr;

                                                            
      arrexpr = makeNode(ArrayExpr);
      arrexpr->array_typeid = get_array_type(key->parttypid[keynum]);
      arrexpr->array_collid = key->parttypcoll[keynum];
      arrexpr->element_typeid = key->parttypid[keynum];
      arrexpr->elements = elems;
      arrexpr->multidims = false;
      arrexpr->location = -1;

                                        
      saopexpr = makeNode(ScalarArrayOpExpr);
      saopexpr->opno = operoid;
      saopexpr->opfuncid = get_opcode(operoid);
      saopexpr->useOr = true;
      saopexpr->inputcollid = key->partcollation[keynum];
      saopexpr->args = list_make2(arg1, arrexpr);
      saopexpr->location = -1;

      result = (Expr *)saopexpr;
    }
    else
    {
      List *elemops = NIL;
      ListCell *lc;

      foreach (lc, elems)
      {
        Expr *elem = lfirst(lc), *elemop;

        elemop = make_opclause(operoid, BOOLOID, false, arg1, elem, InvalidOid, key->partcollation[keynum]);
        elemops = lappend(elemops, elemop);
      }

      result = nelems > 1 ? makeBoolExpr(OR_EXPR, elemops, -1) : linitial(elemops);
    }
    break;
  }

  case PARTITION_STRATEGY_RANGE:
    result = make_opclause(operoid, BOOLOID, false, arg1, arg2, InvalidOid, key->partcollation[keynum]);
    break;

  default:
    elog(ERROR, "invalid partitioning strategy");
    break;
  }

  return result;
}

   
                     
   
                                                                      
                                                                        
   
                                                                         
                                                 
   
static List *
get_qual_for_hash(Relation parent, PartitionBoundSpec *spec)
{
  PartitionKey key = RelationGetPartitionKey(parent);
  FuncExpr *fexpr;
  Node *relidConst;
  Node *modulusConst;
  Node *remainderConst;
  List *args;
  ListCell *partexprs_item;
  int i;

                        
  relidConst = (Node *)makeConst(OIDOID, -1, InvalidOid, sizeof(Oid), ObjectIdGetDatum(RelationGetRelid(parent)), false, true);

  modulusConst = (Node *)makeConst(INT4OID, -1, InvalidOid, sizeof(int32), Int32GetDatum(spec->modulus), false, true);

  remainderConst = (Node *)makeConst(INT4OID, -1, InvalidOid, sizeof(int32), Int32GetDatum(spec->remainder), false, true);

  args = list_make3(relidConst, modulusConst, remainderConst);
  partexprs_item = list_head(key->partexprs);

                                            
  for (i = 0; i < key->partnatts; i++)
  {
    Node *keyCol;

                      
    if (key->partattrs[i] != 0)
    {
      keyCol = (Node *)makeVar(1, key->partattrs[i], key->parttypid[i], key->parttypmod[i], key->parttypcoll[i], 0);
    }
    else
    {
      keyCol = (Node *)copyObject(lfirst(partexprs_item));
      partexprs_item = lnext(partexprs_item);
    }

    args = lappend(args, keyCol);
  }

  fexpr = makeFuncExpr(F_SATISFIES_HASH_PARTITION, BOOLOID, args, InvalidOid, InvalidOid, COERCE_EXPLICIT_CALL);

  return list_make1(fexpr);
}

   
                     
   
                                                                            
                                                                        
   
                                                                       
                                                        
   
static List *
get_qual_for_list(Relation parent, PartitionBoundSpec *spec)
{
  PartitionKey key = RelationGetPartitionKey(parent);
  List *result;
  Expr *keyCol;
  Expr *opexpr;
  NullTest *nulltest;
  ListCell *cell;
  List *elems = NIL;
  bool list_has_null = false;

     
                                                                          
                                                
     
  Assert(key->partnatts == 1);

                                                                     
  if (key->partattrs[0] != 0)
  {
    keyCol = (Expr *)makeVar(1, key->partattrs[0], key->parttypid[0], key->parttypmod[0], key->parttypcoll[0], 0);
  }
  else
  {
    keyCol = (Expr *)copyObject(linitial(key->partexprs));
  }

     
                                                                            
                                                                         
                             
     
  if (spec->is_default)
  {
    int i;
    int ndatums = 0;
    PartitionDesc pdesc = RelationGetPartitionDesc(parent);
    PartitionBoundInfo boundinfo = pdesc->boundinfo;

    if (boundinfo)
    {
      ndatums = boundinfo->ndatums;

      if (partition_bound_accepts_nulls(boundinfo))
      {
        list_has_null = true;
      }
    }

       
                                                                         
                         
       
    if (ndatums == 0 && !list_has_null)
    {
      return NIL;
    }

    for (i = 0; i < ndatums; i++)
    {
      Const *val;

         
                                                                        
                                                                         
                                                
         
      val = makeConst(key->parttypid[0], key->parttypmod[0], key->parttypcoll[0], key->parttyplen[0], datumCopy(*boundinfo->datums[i], key->parttypbyval[0], key->parttyplen[0]), false,             
          key->parttypbyval[0]);

      elems = lappend(elems, val);
    }
  }
  else
  {
       
                                                                          
       
    foreach (cell, spec->listdatums)
    {
      Const *val = castNode(Const, lfirst(cell));

      if (val->constisnull)
      {
        list_has_null = true;
      }
      else
      {
        elems = lappend(elems, copyObject(val));
      }
    }
  }

  if (elems)
  {
       
                                                                    
               
       
    opexpr = make_partition_op_expr(key, 0, BTEqualStrategyNumber, keyCol, (Expr *)elems);
  }
  else
  {
       
                                                                   
                   
       
    opexpr = NULL;
  }

  if (!list_has_null)
  {
       
                                                                        
                                                                         
                           
       
    nulltest = makeNode(NullTest);
    nulltest->arg = keyCol;
    nulltest->nulltesttype = IS_NOT_NULL;
    nulltest->argisrow = false;
    nulltest->location = -1;

    result = opexpr ? list_make2(nulltest, opexpr) : list_make1(nulltest);
  }
  else
  {
       
                                                                   
                   
       
    nulltest = makeNode(NullTest);
    nulltest->arg = keyCol;
    nulltest->nulltesttype = IS_NULL;
    nulltest->argisrow = false;
    nulltest->location = -1;

    if (opexpr)
    {
      Expr * or ;

      or = makeBoolExpr(OR_EXPR, list_make2(nulltest, opexpr), -1);
      result = list_make1(or);
    }
    else
    {
      result = list_make1(nulltest);
    }
  }

     
                                                                            
                                                                          
                                                                       
                                                          
     
  if (spec->is_default)
  {
    result = list_make1(make_ands_explicit(result));
    result = list_make1(makeBoolExpr(NOT_EXPR, result, -1));
  }

  return result;
}

   
                      
   
                                                                             
                                                                        
   
                                                                            
                                                                          
                                                      
   
                                                           
        
                                                                      
        
                                                                     
   
                                                                               
                                                                           
                                          
   
                                                           
        
            
        
                                    
        
                                   
   
                                                                          
                                                                              
                                                                             
                                                                               
   
                                                          
   
                                                                             
                                                                           
   
                                                                            
                         
   
                                                                             
                   
   
static List *
get_qual_for_range(Relation parent, PartitionBoundSpec *spec, bool for_default)
{
  List *result = NIL;
  ListCell *cell1, *cell2, *partexprs_item, *partexprs_item_saved;
  int i, j;
  PartitionRangeDatum *ldatum, *udatum;
  PartitionKey key = RelationGetPartitionKey(parent);
  Expr *keyCol;
  Const *lower_val, *upper_val;
  List *lower_or_arms, *upper_or_arms;
  int num_or_arms, current_or_arm;
  ListCell *lower_or_start_datum, *upper_or_start_datum;
  bool need_next_lower_arm, need_next_upper_arm;

  if (spec->is_default)
  {
    List *or_expr_args = NIL;
    PartitionDesc pdesc = RelationGetPartitionDesc(parent);
    Oid *inhoids = pdesc->oids;
    int nparts = pdesc->nparts, i;

    for (i = 0; i < nparts; i++)
    {
      Oid inhrelid = inhoids[i];
      HeapTuple tuple;
      Datum datum;
      bool isnull;
      PartitionBoundSpec *bspec;

      tuple = SearchSysCache1(RELOID, inhrelid);
      if (!HeapTupleIsValid(tuple))
      {
        elog(ERROR, "cache lookup failed for relation %u", inhrelid);
      }

      datum = SysCacheGetAttr(RELOID, tuple, Anum_pg_class_relpartbound, &isnull);
      if (isnull)
      {
        elog(ERROR, "null relpartbound for relation %u", inhrelid);
      }

      bspec = (PartitionBoundSpec *)stringToNode(TextDatumGetCString(datum));
      if (!IsA(bspec, PartitionBoundSpec))
      {
        elog(ERROR, "expected PartitionBoundSpec");
      }

      if (!bspec->is_default)
      {
        List *part_qual;

        part_qual = get_qual_for_range(parent, bspec, true);

           
                                                           
                        
           
        or_expr_args = lappend(or_expr_args, list_length(part_qual) > 1 ? makeBoolExpr(AND_EXPR, part_qual, -1) : linitial(part_qual));
      }
      ReleaseSysCache(tuple);
    }

    if (or_expr_args != NIL)
    {
      Expr *other_parts_constr;

         
                                                                     
                                                                        
                                                                     
                                                 
         
      other_parts_constr = makeBoolExpr(AND_EXPR, lappend(get_range_nulltest(key), list_length(or_expr_args) > 1 ? makeBoolExpr(OR_EXPR, or_expr_args, -1) : linitial(or_expr_args)), -1);

         
                                                                  
                                                  
         
      result = list_make1(makeBoolExpr(NOT_EXPR, list_make1(other_parts_constr), -1));
    }

    return result;
  }

  lower_or_start_datum = list_head(spec->lowerdatums);
  upper_or_start_datum = list_head(spec->upperdatums);
  num_or_arms = key->partnatts;

     
                                                                             
                                                                             
     
  if (!for_default)
  {
    result = get_range_nulltest(key);
  }

     
                                                                           
                                                                      
                                                                    
                                                                             
                                                                            
                                                
     
  i = 0;
  partexprs_item = list_head(key->partexprs);
  partexprs_item_saved = partexprs_item;                       
  forboth(cell1, spec->lowerdatums, cell2, spec->upperdatums)
  {
    EState *estate;
    MemoryContext oldcxt;
    Expr *test_expr;
    ExprState *test_exprstate;
    Datum test_result;
    bool isNull;

    ldatum = castNode(PartitionRangeDatum, lfirst(cell1));
    udatum = castNode(PartitionRangeDatum, lfirst(cell2));

       
                                                                        
                                                                          
                                                           
       
    partexprs_item_saved = partexprs_item;

    get_range_key_properties(key, i, ldatum, udatum, &partexprs_item, &keyCol, &lower_val, &upper_val);

       
                                                                     
                                                                          
                                                                        
                        
       
    if (!lower_val || !upper_val)
    {
      break;
    }

                                    
    estate = CreateExecutorState();
    oldcxt = MemoryContextSwitchTo(estate->es_query_cxt);
    test_expr = make_partition_op_expr(key, i, BTEqualStrategyNumber, (Expr *)lower_val, (Expr *)upper_val);
    fix_opfuncids((Node *)test_expr);
    test_exprstate = ExecInitExpr(test_expr, NULL);
    test_result = ExecEvalExprSwitchContext(test_exprstate, GetPerTupleExprContext(estate), &isNull);
    MemoryContextSwitchTo(oldcxt);
    FreeExecutorState(estate);

                                                      
    if (!DatumGetBool(test_result))
    {
      break;
    }

       
                                                                         
                                                                           
                                  
       
    if (i == key->partnatts - 1)
    {
      elog(ERROR, "invalid range bound specification");
    }

                                                          
    result = lappend(result, make_partition_op_expr(key, i, BTEqualStrategyNumber, keyCol, (Expr *)lower_val));

    i++;
  }

                                                                 
  lower_or_start_datum = cell1;
  upper_or_start_datum = cell2;

                                                                
  num_or_arms = key->partnatts - i;
  current_or_arm = 0;
  lower_or_arms = upper_or_arms = NIL;
  need_next_lower_arm = need_next_upper_arm = true;
  while (current_or_arm < num_or_arms)
  {
    List *lower_or_arm_args = NIL, *upper_or_arm_args = NIL;

                                                   
    j = i;
    partexprs_item = partexprs_item_saved;

    for_both_cell(cell1, lower_or_start_datum, cell2, upper_or_start_datum)
    {
      PartitionRangeDatum *ldatum_next = NULL, *udatum_next = NULL;

      ldatum = castNode(PartitionRangeDatum, lfirst(cell1));
      if (lnext(cell1))
      {
        ldatum_next = castNode(PartitionRangeDatum, lfirst(lnext(cell1)));
      }
      udatum = castNode(PartitionRangeDatum, lfirst(cell2));
      if (lnext(cell2))
      {
        udatum_next = castNode(PartitionRangeDatum, lfirst(lnext(cell2)));
      }
      get_range_key_properties(key, j, ldatum, udatum, &partexprs_item, &keyCol, &lower_val, &upper_val);

      if (need_next_lower_arm && lower_val)
      {
        uint16 strategy;

           
                                                                      
                                                                       
                                                                   
                                                    
           
        if (j - i < current_or_arm)
        {
          strategy = BTEqualStrategyNumber;
        }
        else if (j == key->partnatts - 1 || (ldatum_next && ldatum_next->kind == PARTITION_RANGE_DATUM_MINVALUE))
        {
          strategy = BTGreaterEqualStrategyNumber;
        }
        else
        {
          strategy = BTGreaterStrategyNumber;
        }

        lower_or_arm_args = lappend(lower_or_arm_args, make_partition_op_expr(key, j, strategy, keyCol, (Expr *)lower_val));
      }

      if (need_next_upper_arm && upper_val)
      {
        uint16 strategy;

           
                                                                      
                                                                    
                                                          
           
        if (j - i < current_or_arm)
        {
          strategy = BTEqualStrategyNumber;
        }
        else if (udatum_next && udatum_next->kind == PARTITION_RANGE_DATUM_MAXVALUE)
        {
          strategy = BTLessEqualStrategyNumber;
        }
        else
        {
          strategy = BTLessStrategyNumber;
        }

        upper_or_arm_args = lappend(upper_or_arm_args, make_partition_op_expr(key, j, strategy, keyCol, (Expr *)upper_val));
      }

         
                                                                        
                                                                        
                                                  
         
      ++j;
      if (j - i > current_or_arm)
      {
           
                                                                      
                                                        
           
        if (!lower_val || !ldatum_next || ldatum_next->kind != PARTITION_RANGE_DATUM_VALUE)
        {
          need_next_lower_arm = false;
        }
        if (!upper_val || !udatum_next || udatum_next->kind != PARTITION_RANGE_DATUM_VALUE)
        {
          need_next_upper_arm = false;
        }
        break;
      }
    }

    if (lower_or_arm_args != NIL)
    {
      lower_or_arms = lappend(lower_or_arms, list_length(lower_or_arm_args) > 1 ? makeBoolExpr(AND_EXPR, lower_or_arm_args, -1) : linitial(lower_or_arm_args));
    }

    if (upper_or_arm_args != NIL)
    {
      upper_or_arms = lappend(upper_or_arms, list_length(upper_or_arm_args) > 1 ? makeBoolExpr(AND_EXPR, upper_or_arm_args, -1) : linitial(upper_or_arm_args));
    }

                                                             
    if (!need_next_lower_arm && !need_next_upper_arm)
    {
      break;
    }

    ++current_or_arm;
  }

     
                                                                        
                                                                   
                  
     
  if (lower_or_arms != NIL)
  {
    result = lappend(result, list_length(lower_or_arms) > 1 ? makeBoolExpr(OR_EXPR, lower_or_arms, -1) : linitial(lower_or_arms));
  }
  if (upper_or_arms != NIL)
  {
    result = lappend(result, list_length(upper_or_arms) > 1 ? makeBoolExpr(OR_EXPR, upper_or_arms, -1) : linitial(upper_or_arms));
  }

     
                                                                            
                                                                         
                                                                            
                                                                       
     
  if (result == NIL)
  {
    result = for_default ? get_range_nulltest(key) : list_make1(makeBoolConst(true, false));
  }

  return result;
}

   
                            
                                                               
   
                                                                      
                               
   
                                                                          
                                                                    
                                                                           
                                                           
   
                                                                        
                                                                      
   
static void
get_range_key_properties(PartitionKey key, int keynum, PartitionRangeDatum *ldatum, PartitionRangeDatum *udatum, ListCell **partexprs_item, Expr **keyCol, Const **lower_val, Const **upper_val)
{
                                                    
  if (key->partattrs[keynum] != 0)
  {
    *keyCol = (Expr *)makeVar(1, key->partattrs[keynum], key->parttypid[keynum], key->parttypmod[keynum], key->parttypcoll[keynum], 0);
  }
  else
  {
    if (*partexprs_item == NULL)
    {
      elog(ERROR, "wrong number of partition key expressions");
    }
    *keyCol = copyObject(lfirst(*partexprs_item));
    *partexprs_item = lnext(*partexprs_item);
  }

                                                  
  if (ldatum->kind == PARTITION_RANGE_DATUM_VALUE)
  {
    *lower_val = castNode(Const, copyObject(ldatum->value));
  }
  else
  {
    *lower_val = NULL;
  }

  if (udatum->kind == PARTITION_RANGE_DATUM_VALUE)
  {
    *upper_val = castNode(Const, copyObject(udatum->value));
  }
  else
  {
    *upper_val = NULL;
  }
}

   
                      
   
                                                                          
                                                                           
   
static List *
get_range_nulltest(PartitionKey key)
{
  List *result = NIL;
  NullTest *nulltest;
  ListCell *partexprs_item;
  int i;

  partexprs_item = list_head(key->partexprs);
  for (i = 0; i < key->partnatts; i++)
  {
    Expr *keyCol;

    if (key->partattrs[i] != 0)
    {
      keyCol = (Expr *)makeVar(1, key->partattrs[i], key->parttypid[i], key->parttypmod[i], key->parttypcoll[i], 0);
    }
    else
    {
      if (partexprs_item == NULL)
      {
        elog(ERROR, "wrong number of partition key expressions");
      }
      keyCol = copyObject(lfirst(partexprs_item));
      partexprs_item = lnext(partexprs_item);
    }

    nulltest = makeNode(NullTest);
    nulltest->arg = keyCol;
    nulltest->nulltesttype = IS_NOT_NULL;
    nulltest->argisrow = false;
    nulltest->location = -1;
    result = lappend(result, nulltest);
  }

  return result;
}

   
                                
   
                                                          
   
uint64
compute_partition_hash_value(int partnatts, FmgrInfo *partsupfunc, Oid *partcollation, Datum *values, bool *isnull)
{
  int i;
  uint64 rowHash = 0;
  Datum seed = UInt64GetDatum(HASH_PARTITION_SEED);

  for (i = 0; i < partnatts; i++)
  {
                                
    if (!isnull[i])
    {
      Datum hash;

      Assert(OidIsValid(partsupfunc[i].fn_oid));

         
                                                                 
                                                                
                    
         
      hash = FunctionCall2Coll(&partsupfunc[i], partcollation[i], values[i], seed);

                                           
      rowHash = hash_combine64(rowHash, DatumGetUInt64(hash));
    }
  }

  return rowHash;
}

   
                            
   
                                                                           
                                                                               
                                                                         
                                                                             
                                         
   
                                                                              
                                                                              
                                                                               
                                       
   
                                      
   
Datum
satisfies_hash_partition(PG_FUNCTION_ARGS)
{
  typedef struct ColumnsHashData
  {
    Oid relid;
    int nkeys;
    Oid variadic_type;
    int16 variadic_typlen;
    bool variadic_typbyval;
    char variadic_typalign;
    Oid partcollid[PARTITION_MAX_KEYS];
    FmgrInfo partsupfunc[FLEXIBLE_ARRAY_MEMBER];
  } ColumnsHashData;
  Oid parentId;
  int modulus;
  int remainder;
  Datum seed = UInt64GetDatum(HASH_PARTITION_SEED);
  ColumnsHashData *my_extra;
  uint64 rowHash = 0;

                                                                      
  if (PG_ARGISNULL(0) || PG_ARGISNULL(1) || PG_ARGISNULL(2))
  {
    PG_RETURN_BOOL(false);
  }
  parentId = PG_GETARG_OID(0);
  modulus = PG_GETARG_INT32(1);
  remainder = PG_GETARG_INT32(2);

                                           
  if (modulus <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("modulus for hash partition must be an integer value greater than zero")));
  }
  if (remainder < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("remainder for hash partition must be an integer value greater than or equal to zero")));
  }
  if (remainder >= modulus)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("remainder for hash partition must be less than modulus")));
  }

     
                                      
     
  my_extra = (ColumnsHashData *)fcinfo->flinfo->fn_extra;
  if (my_extra == NULL || my_extra->relid != parentId)
  {
    Relation parent;
    PartitionKey key;
    int j;

                                                           
    parent = relation_open(parentId, AccessShareLock);
    key = RelationGetPartitionKey(parent);

                                                           
    if (key == NULL || key->strategy != PARTITION_STRATEGY_HASH)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("\"%s\" is not a hash partitioned table", get_rel_name(parentId))));
    }

    if (!get_fn_expr_variadic(fcinfo->flinfo))
    {
      int nargs = PG_NARGS() - 3;

                                                     
      if (key->partnatts != nargs)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("number of partitioning columns (%d) does not match number of partition keys provided (%d)", key->partnatts, nargs)));
      }

                                        
      fcinfo->flinfo->fn_extra = MemoryContextAllocZero(fcinfo->flinfo->fn_mcxt, offsetof(ColumnsHashData, partsupfunc) + sizeof(FmgrInfo) * nargs);
      my_extra = (ColumnsHashData *)fcinfo->flinfo->fn_extra;
      my_extra->relid = parentId;
      my_extra->nkeys = key->partnatts;
      memcpy(my_extra->partcollid, key->partcollation, key->partnatts * sizeof(Oid));

                                                    
      for (j = 0; j < key->partnatts; ++j)
      {
        Oid argtype = get_fn_expr_argtype(fcinfo->flinfo, j + 3);

        if (argtype != key->parttypid[j] && !IsBinaryCoercible(argtype, key->parttypid[j]))
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("column %d of the partition key has type \"%s\", but supplied value is of type \"%s\"", j + 1, format_type_be(key->parttypid[j]), format_type_be(argtype))));
        }

        fmgr_info_copy(&my_extra->partsupfunc[j], &key->partsupfunc[j], fcinfo->flinfo->fn_mcxt);
      }
    }
    else
    {
      ArrayType *variadic_array = PG_GETARG_ARRAYTYPE_P(3);

                                                                          
      fcinfo->flinfo->fn_extra = MemoryContextAllocZero(fcinfo->flinfo->fn_mcxt, offsetof(ColumnsHashData, partsupfunc) + sizeof(FmgrInfo));
      my_extra = (ColumnsHashData *)fcinfo->flinfo->fn_extra;
      my_extra->relid = parentId;
      my_extra->nkeys = key->partnatts;
      my_extra->variadic_type = ARR_ELEMTYPE(variadic_array);
      get_typlenbyvalalign(my_extra->variadic_type, &my_extra->variadic_typlen, &my_extra->variadic_typbyval, &my_extra->variadic_typalign);
      my_extra->partcollid[0] = key->partcollation[0];

                                
      for (j = 0; j < key->partnatts; ++j)
      {
        if (key->parttypid[j] != my_extra->variadic_type)
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("column %d of the partition key has type \"%s\", but supplied value is of type \"%s\"", j + 1, format_type_be(key->parttypid[j]), format_type_be(my_extra->variadic_type))));
        }
      }

      fmgr_info_copy(&my_extra->partsupfunc[0], &key->partsupfunc[0], fcinfo->flinfo->fn_mcxt);
    }

                                
    relation_close(parent, NoLock);
  }

  if (!OidIsValid(my_extra->variadic_type))
  {
    int nkeys = my_extra->nkeys;
    int i;

       
                                                                          
                                                                         
             
       

    for (i = 0; i < nkeys; i++)
    {
      Datum hash;

                                                        
      int argno = i + 3;

      if (PG_ARGISNULL(argno))
      {
        continue;
      }

      hash = FunctionCall2Coll(&my_extra->partsupfunc[i], my_extra->partcollid[i], PG_GETARG_DATUM(argno), seed);

                                           
      rowHash = hash_combine64(rowHash, DatumGetUInt64(hash));
    }
  }
  else
  {
    ArrayType *variadic_array = PG_GETARG_ARRAYTYPE_P(3);
    int i;
    int nelems;
    Datum *datum;
    bool *isnull;

    deconstruct_array(variadic_array, my_extra->variadic_type, my_extra->variadic_typlen, my_extra->variadic_typbyval, my_extra->variadic_typalign, &datum, &isnull, &nelems);

                                                   
    if (nelems != my_extra->nkeys)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("number of partitioning columns (%d) does not match number of partition keys provided (%d)", my_extra->nkeys, nelems)));
    }

    for (i = 0; i < nelems; i++)
    {
      Datum hash;

      if (isnull[i])
      {
        continue;
      }

      hash = FunctionCall2Coll(&my_extra->partsupfunc[0], my_extra->partcollid[0], datum[i], seed);

                                           
      rowHash = hash_combine64(rowHash, DatumGetUInt64(hash));
    }
  }

  PG_RETURN_BOOL(rowHash % modulus == remainder);
}
