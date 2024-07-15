                                                                            
   
                  
                                      
   
                                                                         
                                                                        
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "access/htup_details.h"
#include "access/sysattr.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_statistic_ext_data.h"
#include "lib/stringinfo.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "nodes/nodes.h"
#include "nodes/pathnodes.h"
#include "parser/parsetree.h"
#include "statistics/extended_stats_internal.h"
#include "statistics/statistics.h"
#include "utils/bytea.h"
#include "utils/fmgroids.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/selfuncs.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

                                                           
#define SizeOfHeader (3 * sizeof(uint32))

                                                           
#define SizeOfItem(natts) (sizeof(double) + sizeof(AttrNumber) * (1 + (natts)))

                                                        
#define MinSizeOfItem SizeOfItem(2)

                                                             
#define MinSizeOfItems(ndeps) (SizeOfHeader + (ndeps) * MinSizeOfItem)

   
                                                                                       
                                                                               
                                                                        
   
typedef struct DependencyGeneratorData
{
  int k;                                                
  int n;                                                       
  int current;                                                     
  AttrNumber ndependencies;                                       
  AttrNumber *dependencies;                                          
} DependencyGeneratorData;

typedef DependencyGeneratorData *DependencyGenerator;

static void
generate_dependencies_recurse(DependencyGenerator state, int index, AttrNumber start, AttrNumber *current);
static void
generate_dependencies(DependencyGenerator state);
static DependencyGenerator
DependencyGenerator_init(int n, int k);
static void
DependencyGenerator_free(DependencyGenerator state);
static AttrNumber *
DependencyGenerator_next(DependencyGenerator state);
static double
dependency_degree(int numrows, HeapTuple *rows, int k, AttrNumber *dependency, VacAttrStats **stats, Bitmapset *attrs);
static bool
dependency_is_fully_matched(MVDependency *dependency, Bitmapset *attnums);
static bool
dependency_implies_attribute(MVDependency *dependency, AttrNumber attnum);
static bool
dependency_is_compatible_clause(Node *clause, Index relid, AttrNumber *attnum);
static MVDependency *
find_strongest_dependency(StatisticExtInfo *stats, MVDependencies *dependencies, Bitmapset *attnums);

static void
generate_dependencies_recurse(DependencyGenerator state, int index, AttrNumber start, AttrNumber *current)
{
     
                                                                         
                   
     
  if (index < (state->k - 1))
  {
    AttrNumber i;

       
                                                                      
                             
       

    for (i = start; i < state->n; i++)
    {
      current[index] = i;
      generate_dependencies_recurse(state, (index + 1), (i + 1), current);
    }
  }
  else
  {
    int i;

       
                                                                         
                                                                           
                             
       

    for (i = 0; i < state->n; i++)
    {
      int j;
      bool match = false;

      current[index] = i;

      for (j = 0; j < index; j++)
      {
        if (current[j] == i)
        {
          match = true;
          break;
        }
      }

         
                                                                        
                     
         
      if (!match)
      {
        state->dependencies = (AttrNumber *)repalloc(state->dependencies, state->k * (state->ndependencies + 1) * sizeof(AttrNumber));
        memcpy(&state->dependencies[(state->k * state->ndependencies)], current, state->k * sizeof(AttrNumber));
        state->ndependencies++;
      }
    }
  }
}

                                                              
static void
generate_dependencies(DependencyGenerator state)
{
  AttrNumber *current = (AttrNumber *)palloc0(sizeof(AttrNumber) * state->k);

  generate_dependencies_recurse(state, 0, 0, current);

  pfree(current);
}

   
                                                                                 
   
                                                                      
                                                       
   
static DependencyGenerator
DependencyGenerator_init(int n, int k)
{
  DependencyGenerator state;

  Assert((n >= k) && (k > 0));

                                              
  state = (DependencyGenerator)palloc0(sizeof(DependencyGeneratorData));
  state->dependencies = (AttrNumber *)palloc(k * sizeof(AttrNumber));

  state->ndependencies = 0;
  state->current = 0;
  state->k = k;
  state->n = n;

                                                    
  generate_dependencies(state);

  return state;
}

                                        
static void
DependencyGenerator_free(DependencyGenerator state)
{
  pfree(state->dependencies);
  pfree(state);
}

                               
static AttrNumber *
DependencyGenerator_next(DependencyGenerator state)
{
  if (state->current == state->ndependencies)
  {
    return NULL;
  }

  return &state->dependencies[state->k * state->current++];
}

   
                                               
   
                                                                                
                                                                               
                 
   
static double
dependency_degree(int numrows, HeapTuple *rows, int k, AttrNumber *dependency, VacAttrStats **stats, Bitmapset *attrs)
{
  int i, nitems;
  MultiSortSupport mss;
  SortItem *items;
  AttrNumber *attnums;
  AttrNumber *attnums_dep;
  int numattrs;

                                     
  int group_size = 0;
  int n_violations = 0;

                                                                        
  int n_supporting_rows = 0;

                                                        
  Assert(k >= 2);

                                            
  mss = multi_sort_init(k);

     
                                                                            
                                                                            
                                               
     
  attnums = build_attnums_array(attrs, &numattrs);

  attnums_dep = (AttrNumber *)palloc(k * sizeof(AttrNumber));
  for (i = 0; i < k; i++)
  {
    attnums_dep[i] = attnums[dependency[i]];
  }

     
                                                                          
     
                                         
     
                                                           
     
                                                                  
     
                                                                          
                                                                           
     

                                                    
  for (i = 0; i < k; i++)
  {
    VacAttrStats *colstat = stats[dependency[i]];
    TypeCacheEntry *type;

    type = lookup_type_cache(colstat->attrtypid, TYPECACHE_LT_OPR);
    if (type->lt_opr == InvalidOid)                       
    {
      elog(ERROR, "cache lookup failed for ordering operator for type %u", colstat->attrtypid);
    }

                                                      
    multi_sort_add_dimension(mss, i, type->lt_opr, colstat->attrcollid);
  }

     
                                                                       
     
                                                                     
                                                                            
                                                                     
     
  items = build_sorted_items(numrows, &nitems, rows, stats[0]->tupDesc, mss, k, attnums_dep);

     
                                                                        
                                                                           
                                                                             
                                
     

                                                
  group_size = 1;

                                                                           
  for (i = 1; i <= nitems; i++)
  {
       
                                                                          
                                                                           
                          
       
    if (i == nitems || multi_sort_compare_dims(0, k - 2, &items[i - 1], &items[i], mss) != 0)
    {
         
                                                                         
                                                            
         
      if (n_violations == 0)
      {
        n_supporting_rows += group_size;
      }

                                            
      n_violations = 0;
      group_size = 1;
      continue;
    }
                                                                           
    else if (multi_sort_compare_dim(k - 1, &items[i - 1], &items[i], mss) != 0)
    {
      n_violations++;
    }

    group_size++;
  }

                                                               
  return (n_supporting_rows * 1.0 / numrows);
}

   
                                                             
   
                                                                       
                                                                             
                                                              
   
                                    
                                    
                               
                               
                               
               
               
               
   
MVDependencies *
statext_dependencies_build(int numrows, HeapTuple *rows, Bitmapset *attrs, VacAttrStats **stats)
{
  int i, k;
  int numattrs;
  AttrNumber *attnums;

              
  MVDependencies *dependencies = NULL;
  MemoryContext cxt;

     
                                                                            
     
  attnums = build_attnums_array(attrs, &numattrs);

  Assert(numattrs >= 2);

                                                          
  cxt = AllocSetContextCreate(CurrentMemoryContext, "dependency_degree cxt", ALLOCSET_DEFAULT_SIZES);

     
                                                                             
                                                                        
                                                                         
                                                              
     
  for (k = 2; k <= numattrs; k++)
  {
    AttrNumber *dependency;                            

                                                    
    DependencyGenerator DependencyGenerator = DependencyGenerator_init(numattrs, k);

                                                                 
    while ((dependency = DependencyGenerator_next(DependencyGenerator)))
    {
      double degree;
      MVDependency *d;
      MemoryContext oldcxt;

                                                                
      oldcxt = MemoryContextSwitchTo(cxt);

                                                  
      degree = dependency_degree(numrows, rows, k, dependency, stats, attrs);

      MemoryContextSwitchTo(oldcxt);
      MemoryContextReset(cxt);

         
                                                                  
         
      if (degree == 0.0)
      {
        continue;
      }

      d = (MVDependency *)palloc0(offsetof(MVDependency, attributes) + k * sizeof(AttrNumber));

                                                                   
      d->degree = degree;
      d->nattributes = k;
      for (i = 0; i < k; i++)
      {
        d->attributes[i] = attnums[dependency[i]];
      }

                                               
      if (dependencies == NULL)
      {
        dependencies = (MVDependencies *)palloc0(sizeof(MVDependencies));

        dependencies->magic = STATS_DEPS_MAGIC;
        dependencies->type = STATS_DEPS_TYPE_BASIC;
        dependencies->ndeps = 0;
      }

      dependencies->ndeps++;
      dependencies = (MVDependencies *)repalloc(dependencies, offsetof(MVDependencies, deps) + dependencies->ndeps * sizeof(MVDependency *));

      dependencies->deps[dependencies->ndeps - 1] = d;
    }

       
                                                             
                           
       
    DependencyGenerator_free(DependencyGenerator);
  }

  MemoryContextDelete(cxt);

  return dependencies;
}

   
                                                      
   
bytea *
statext_dependencies_serialize(MVDependencies *dependencies)
{
  int i;
  bytea *output;
  char *tmp;
  Size len;

                                                                        
  len = VARHDRSZ + SizeOfHeader;

                                                                           
  for (i = 0; i < dependencies->ndeps; i++)
  {
    len += SizeOfItem(dependencies->deps[i]->nattributes);
  }

  output = (bytea *)palloc0(len);
  SET_VARSIZE(output, len);

  tmp = VARDATA(output);

                                                         
  memcpy(tmp, &dependencies->magic, sizeof(uint32));
  tmp += sizeof(uint32);
  memcpy(tmp, &dependencies->type, sizeof(uint32));
  tmp += sizeof(uint32);
  memcpy(tmp, &dependencies->ndeps, sizeof(uint32));
  tmp += sizeof(uint32);

                                                                            
  for (i = 0; i < dependencies->ndeps; i++)
  {
    MVDependency *d = dependencies->deps[i];

    memcpy(tmp, &d->degree, sizeof(double));
    tmp += sizeof(double);

    memcpy(tmp, &d->nattributes, sizeof(AttrNumber));
    tmp += sizeof(AttrNumber);

    memcpy(tmp, d->attributes, sizeof(AttrNumber) * d->nattributes);
    tmp += sizeof(AttrNumber) * d->nattributes;

                                  
    Assert(tmp <= ((char *)output + len));
  }

                                                                 
  Assert(tmp == ((char *)output + len));

  return output;
}

   
                                                                
   
MVDependencies *
statext_dependencies_deserialize(bytea *data)
{
  int i;
  Size min_expected_size;
  MVDependencies *dependencies;
  char *tmp;

  if (data == NULL)
  {
    return NULL;
  }

  if (VARSIZE_ANY_EXHDR(data) < SizeOfHeader)
  {
    elog(ERROR, "invalid MVDependencies size %zd (expected at least %zd)", VARSIZE_ANY_EXHDR(data), SizeOfHeader);
  }

                                      
  dependencies = (MVDependencies *)palloc0(sizeof(MVDependencies));

                                                                     
  tmp = VARDATA_ANY(data);

                                                              
  memcpy(&dependencies->magic, tmp, sizeof(uint32));
  tmp += sizeof(uint32);
  memcpy(&dependencies->type, tmp, sizeof(uint32));
  tmp += sizeof(uint32);
  memcpy(&dependencies->ndeps, tmp, sizeof(uint32));
  tmp += sizeof(uint32);

  if (dependencies->magic != STATS_DEPS_MAGIC)
  {
    elog(ERROR, "invalid dependency magic %d (expected %d)", dependencies->magic, STATS_DEPS_MAGIC);
  }

  if (dependencies->type != STATS_DEPS_TYPE_BASIC)
  {
    elog(ERROR, "invalid dependency type %d (expected %d)", dependencies->type, STATS_DEPS_TYPE_BASIC);
  }

  if (dependencies->ndeps == 0)
  {
    elog(ERROR, "invalid zero-length item array in MVDependencies");
  }

                                                                 
  min_expected_size = SizeOfItem(dependencies->ndeps);

  if (VARSIZE_ANY_EXHDR(data) < min_expected_size)
  {
    elog(ERROR, "invalid dependencies size %zd (expected at least %zd)", VARSIZE_ANY_EXHDR(data), min_expected_size);
  }

                                        
  dependencies = repalloc(dependencies, offsetof(MVDependencies, deps) + (dependencies->ndeps * sizeof(MVDependency *)));

  for (i = 0; i < dependencies->ndeps; i++)
  {
    double degree;
    AttrNumber k;
    MVDependency *d;

                            
    memcpy(&degree, tmp, sizeof(double));
    tmp += sizeof(double);

                              
    memcpy(&k, tmp, sizeof(AttrNumber));
    tmp += sizeof(AttrNumber);

                                            
    Assert((k >= 2) && (k <= STATS_MAX_DIMENSIONS));

                                                                            
    d = (MVDependency *)palloc0(offsetof(MVDependency, attributes) + (k * sizeof(AttrNumber)));

    d->degree = degree;
    d->nattributes = k;

                                
    memcpy(d->attributes, tmp, sizeof(AttrNumber) * d->nattributes);
    tmp += sizeof(AttrNumber) * d->nattributes;

    dependencies->deps[i] = d;

                                
    Assert(tmp <= ((char *)data + VARSIZE_ANY(data)));
  }

                                                       
  Assert(tmp == ((char *)data + VARSIZE_ANY(data)));

  return dependencies;
}

   
                               
                                                                          
                                                                    
   
static bool
dependency_is_fully_matched(MVDependency *dependency, Bitmapset *attnums)
{
  int j;

     
                                                                             
                                                                 
     
  for (j = 0; j < dependency->nattributes; j++)
  {
    int attnum = dependency->attributes[j];

    if (!bms_is_member(attnum, attnums))
    {
      return false;
    }
  }

  return true;
}

   
                                
                                                                          
   
static bool
dependency_implies_attribute(MVDependency *dependency, AttrNumber attnum)
{
  if (attnum == dependency->attributes[dependency->nattributes - 1])
  {
    return true;
  }

  return false;
}

   
                             
                                                                              
   
MVDependencies *
statext_dependencies_load(Oid mvoid)
{
  MVDependencies *result;
  bool isnull;
  Datum deps;
  HeapTuple htup;

  htup = SearchSysCache1(STATEXTDATASTXOID, ObjectIdGetDatum(mvoid));
  if (!HeapTupleIsValid(htup))
  {
    elog(ERROR, "cache lookup failed for statistics object %u", mvoid);
  }

  deps = SysCacheGetAttr(STATEXTDATASTXOID, htup, Anum_pg_statistic_ext_data_stxddependencies, &isnull);
  if (isnull)
  {
    elog(ERROR, "requested statistics kind \"%c\" is not yet built for statistics object %u", STATS_EXT_DEPENDENCIES, mvoid);
  }

  result = statext_dependencies_deserialize(DatumGetByteaPP(deps));

  ReleaseSysCache(htup);

  return result;
}

   
                                                                 
   
                                                                                 
                                       
   
Datum
pg_dependencies_in(PG_FUNCTION_ARGS)
{
     
                                                                           
                                   
     
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", "pg_dependencies")));

  PG_RETURN_VOID();                          
}

   
                                                               
   
Datum
pg_dependencies_out(PG_FUNCTION_ARGS)
{
  bytea *data = PG_GETARG_BYTEA_PP(0);
  MVDependencies *dependencies = statext_dependencies_deserialize(data);
  int i, j;
  StringInfoData str;

  initStringInfo(&str);
  appendStringInfoChar(&str, '{');

  for (i = 0; i < dependencies->ndeps; i++)
  {
    MVDependency *dependency = dependencies->deps[i];

    if (i > 0)
    {
      appendStringInfoString(&str, ", ");
    }

    appendStringInfoChar(&str, '"');
    for (j = 0; j < dependency->nattributes; j++)
    {
      if (j == dependency->nattributes - 1)
      {
        appendStringInfoString(&str, " => ");
      }
      else if (j > 0)
      {
        appendStringInfoString(&str, ", ");
      }

      appendStringInfo(&str, "%d", dependency->attributes[j]);
    }
    appendStringInfo(&str, "\": %f", dependency->degree);
  }

  appendStringInfoChar(&str, '}');

  PG_RETURN_CSTRING(str.data);
}

   
                                                                          
   
Datum
pg_dependencies_recv(PG_FUNCTION_ARGS)
{
  ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot accept a value of type %s", "pg_dependencies")));

  PG_RETURN_VOID();                          
}

   
                                                                           
   
                                                                              
                                                   
   
Datum
pg_dependencies_send(PG_FUNCTION_ARGS)
{
  return byteasend(fcinfo);
}

   
                                   
                                                                        
   
                                                                              
                                                                           
                                                                      
                                                                     
   
static bool
dependency_is_compatible_clause(Node *clause, Index relid, AttrNumber *attnum)
{
  RestrictInfo *rinfo = (RestrictInfo *)clause;
  Var *var;

  if (!IsA(rinfo, RestrictInfo))
  {
    return false;
  }

                                                                         
  if (rinfo->pseudoconstant)
  {
    return false;
  }

                                                                    
  if (bms_membership(rinfo->clause_relids) != BMS_SINGLETON)
  {
    return false;
  }

  if (is_opclause(rinfo->clause))
  {
                                                                    
    OpExpr *expr = (OpExpr *)rinfo->clause;

                                                             
    if (list_length(expr->args) != 2)
    {
      return false;
    }

                                                              
    if (is_pseudo_constant_clause(lsecond(expr->args)))
    {
      var = linitial(expr->args);
    }
    else if (is_pseudo_constant_clause(linitial(expr->args)))
    {
      var = lsecond(expr->args);
    }
    else
    {
      return false;
    }

       
                                                                        
                                                
       
                                                                           
                                               
       
                                                                          
                                                                    
                                                                       
                                 
       
    if (get_oprrest(expr->opno) != F_EQSEL)
    {
      return false;
    }

                                           
  }
  else if (is_notclause(rinfo->clause))
  {
       
                                                                          
                                                   
       
    var = (Var *)get_notclausearg(rinfo->clause);
  }
  else
  {
       
                                                                     
                                                   
       
    var = (Var *)rinfo->clause;
  }

     
                                                                            
                                                                            
     
  if (IsA(var, RelabelType))
  {
    var = (Var *)((RelabelType *)var)->arg;
  }

                                          
  if (!IsA(var, Var))
  {
    return false;
  }

                                               
  if (var->varno != relid)
  {
    return false;
  }

                                                               
  if (var->varlevelsup != 0)
  {
    return false;
  }

                                                                     
  if (!AttrNumberIsForUserDefinedAttr(var->varattno))
  {
    return false;
  }

  *attnum = var->varattno;
  return true;
}

   
                             
                                                    
   
                                                                      
                                                         
   
                                                      
   
                               
   
                                          
   
                                                                         
                                                             
   
static MVDependency *
find_strongest_dependency(StatisticExtInfo *stats, MVDependencies *dependencies, Bitmapset *attnums)
{
  int i;
  MVDependency *strongest = NULL;

                                    
  int nattnums = bms_num_members(attnums);

     
                                                                             
                                                                      
                                      
     
  for (i = 0; i < dependencies->ndeps; i++)
  {
    MVDependency *dependency = dependencies->deps[i];

       
                                                                    
                                                 
       
    if (dependency->nattributes > nattnums)
    {
      continue;
    }

    if (strongest)
    {
                                                                     
      if (dependency->nattributes < strongest->nattributes)
      {
        continue;
      }

                                                                      
      if (strongest->nattributes == dependency->nattributes && strongest->degree > dependency->degree)
      {
        continue;
      }
    }

       
                                                                      
                                                                          
                                                         
       
    if (dependency_is_fully_matched(dependency, attnums))
    {
      strongest = dependency;                          
    }
  }

  return strongest;
}

   
                                       
                                                                        
                                                                           
                                 
   
                                                                      
                                                                                
                                 
   
                                                                               
                                                                               
                                                                              
   
                                       
   
                                              
   
                                                                          
                                                                             
                                      
   
                                           
   
                                                  
   
Selectivity
dependencies_clauselist_selectivity(PlannerInfo *root, List *clauses, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo, RelOptInfo *rel, Bitmapset **estimatedclauses)
{
  Selectivity s1 = 1.0;
  ListCell *l;
  Bitmapset *clauses_attnums = NULL;
  StatisticExtInfo *stat;
  MVDependencies *dependencies;
  Bitmapset **list_attnums;
  int listidx;
  RangeTblEntry *rte = planner_rt_fetch(rel->relid, root);

     
                                                                        
                                                                     
                                                                         
                                                                          
                                                              
     
  if (rte->inh && rte->relkind != RELKIND_PARTITIONED_TABLE)
  {
    return 1.0;
  }

                                                               
  if (!has_stats_of_kind(rel->statlist, STATS_EXT_DEPENDENCIES))
  {
    return 1.0;
  }

  list_attnums = (Bitmapset **)palloc(sizeof(Bitmapset *) * list_length(clauses));

     
                                                                            
                                                                          
                                                                           
                                                                             
                                                                            
                   
     
                                                                             
                                                 
     
  listidx = 0;
  foreach (l, clauses)
  {
    Node *clause = (Node *)lfirst(l);
    AttrNumber attnum;

    if (!bms_is_member(listidx, *estimatedclauses) && dependency_is_compatible_clause(clause, rel->relid, &attnum))
    {
      list_attnums[listidx] = bms_make_singleton(attnum);
      clauses_attnums = bms_add_member(clauses_attnums, attnum);
    }
    else
    {
      list_attnums[listidx] = NULL;
    }

    listidx++;
  }

     
                                                                             
                                                                             
                 
     
  if (bms_num_members(clauses_attnums) < 2)
  {
    pfree(list_attnums);
    return 1.0;
  }

                                                                
  stat = choose_best_statistics(rel->statlist, STATS_EXT_DEPENDENCIES, list_attnums, list_length(clauses));

                                                                    
  if (!stat)
  {
    pfree(list_attnums);
    return 1.0;
  }

                                                                 
  dependencies = statext_dependencies_load(stat->statOid);

     
                                                                            
                                                                         
                                                                             
                                           
     
  while (true)
  {
    Selectivity s2 = 1.0;
    MVDependency *dependency;

                                                                   
    dependency = find_strongest_dependency(stat, dependencies, clauses_attnums);

                                                         
    if (!dependency)
    {
      break;
    }

       
                                                                         
                                                                          
               
       
    listidx = -1;
    foreach (l, clauses)
    {
      Node *clause;
      AttrNumber attnum;

      listidx++;

         
                                                                         
         
      if (!list_attnums[listidx])
      {
        continue;
      }

         
                                                                      
         
      attnum = bms_singleton_member(list_attnums[listidx]);

         
                                                                    
                                                                         
                                                                        
                                                                        
                                                                        
                                                                      
                                                                 
         
      if (dependency_implies_attribute(dependency, attnum))
      {
        clause = (Node *)lfirst(l);

        s2 = clause_selectivity(root, clause, varRelid, jointype, sjinfo);

                                                                
        *estimatedclauses = bms_add_member(*estimatedclauses, listidx);

           
                                                                       
                                                               
                                       
           
        clauses_attnums = bms_del_member(clauses_attnums, attnum);
      }
    }

       
                                                                        
                                          
       
                                          
       
                                                              
       
    s1 *= (dependency->degree + (1 - dependency->degree) * s2);
  }

  pfree(dependencies);
  pfree(list_attnums);

  return s1;
}
