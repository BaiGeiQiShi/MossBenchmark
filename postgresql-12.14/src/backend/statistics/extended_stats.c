                                                                            
   
                    
                                  
   
                                                                             
   
   
                                                                         
                                                                        
   
                  
                                             
   
                                                                            
   
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/tuptoaster.h"
#include "catalog/indexing.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_statistic_ext.h"
#include "catalog/pg_statistic_ext_data.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/optimizer.h"
#include "parser/parsetree.h"
#include "postmaster/autovacuum.h"
#include "statistics/extended_stats_internal.h"
#include "statistics/statistics.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/selfuncs.h"
#include "utils/syscache.h"

   
                                                                            
                                                                               
                                                                         
                                                                        
                                                                              
                                                                       
                         
   
#define WIDTH_THRESHOLD 1024

   
                                                                      
                             
   
typedef struct StatExtEntry
{
  Oid statOid;                                           
  char *schema;                                       
  char *name;                                       
  Bitmapset *columns;                                              
  List *types;                                                     
} StatExtEntry;

static List *
fetch_statentries_for_relation(Relation pg_statext, Oid relid);
static VacAttrStats **
lookup_var_attr_stats(Relation rel, Bitmapset *attrs, int nvacatts, VacAttrStats **vacatts);
static void
statext_store(Oid relid, MVNDistinct *ndistinct, MVDependencies *dependencies, MCVList *mcv, VacAttrStats **stats);

   
                                                                          
                          
   
                                                                          
                                                               
   
void
BuildRelationExtStatistics(Relation onerel, double totalrows, int numrows, HeapTuple *rows, int natts, VacAttrStats **vacattrstats)
{
  Relation pg_stext;
  ListCell *lc;
  List *stats;
  MemoryContext cxt;
  MemoryContext oldcxt;

  pg_stext = table_open(StatisticExtRelationId, RowExclusiveLock);
  stats = fetch_statentries_for_relation(pg_stext, RelationGetRelid(onerel));

                                                          
  cxt = AllocSetContextCreate(CurrentMemoryContext, "BuildRelationExtStatistics", ALLOCSET_DEFAULT_SIZES);
  oldcxt = MemoryContextSwitchTo(cxt);

  foreach (lc, stats)
  {
    StatExtEntry *stat = (StatExtEntry *)lfirst(lc);
    MVNDistinct *ndistinct = NULL;
    MVDependencies *dependencies = NULL;
    MCVList *mcv = NULL;
    VacAttrStats **stats;
    ListCell *lc2;

       
                                                                          
                                                                 
       
    stats = lookup_var_attr_stats(onerel, stat->columns, natts, vacattrstats);
    if (!stats)
    {
      if (!IsAutoVacuumWorkerProcess())
      {
        ereport(WARNING, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("statistics object \"%s.%s\" could not be computed for relation \"%s.%s\"", stat->schema, stat->name, get_namespace_name(onerel->rd_rel->relnamespace), RelationGetRelationName(onerel)), errtable(onerel)));
      }
      continue;
    }

                                            
    Assert(bms_num_members(stat->columns) >= 2 && bms_num_members(stat->columns) <= STATS_MAX_DIMENSIONS);

                                                  
    foreach (lc2, stat->types)
    {
      char t = (char)lfirst_int(lc2);

      if (t == STATS_EXT_NDISTINCT)
      {
        ndistinct = statext_ndistinct_build(totalrows, numrows, rows, stat->columns, stats);
      }
      else if (t == STATS_EXT_DEPENDENCIES)
      {
        dependencies = statext_dependencies_build(numrows, rows, stat->columns, stats);
      }
      else if (t == STATS_EXT_MCV)
      {
        mcv = statext_mcv_build(numrows, rows, stat->columns, stats, totalrows);
      }
    }

                                             
    statext_store(stat->statOid, ndistinct, dependencies, mcv, stats);

                                                                
    MemoryContextReset(cxt);
  }

  MemoryContextSwitchTo(oldcxt);
  MemoryContextDelete(cxt);

  list_free(stats);

  table_close(pg_stext, RowExclusiveLock);
}

   
                         
                                                                      
   
bool
statext_is_kind_built(HeapTuple htup, char type)
{
  AttrNumber attnum;

  switch (type)
  {
  case STATS_EXT_NDISTINCT:
    attnum = Anum_pg_statistic_ext_data_stxdndistinct;
    break;

  case STATS_EXT_DEPENDENCIES:
    attnum = Anum_pg_statistic_ext_data_stxddependencies;
    break;

  case STATS_EXT_MCV:
    attnum = Anum_pg_statistic_ext_data_stxdmcv;
    break;

  default:
    elog(ERROR, "unexpected statistics type requested: %d", type);
  }

  return !heap_attisnull(htup, attnum, NULL);
}

   
                                                                                 
   
static List *
fetch_statentries_for_relation(Relation pg_statext, Oid relid)
{
  SysScanDesc scan;
  ScanKeyData skey;
  HeapTuple htup;
  List *result = NIL;

     
                                                                         
          
     
  ScanKeyInit(&skey, Anum_pg_statistic_ext_stxrelid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(relid));

  scan = systable_beginscan(pg_statext, StatisticExtRelidIndexId, true, NULL, 1, &skey);

  while (HeapTupleIsValid(htup = systable_getnext(scan)))
  {
    StatExtEntry *entry;
    Datum datum;
    bool isnull;
    int i;
    ArrayType *arr;
    char *enabled;
    Form_pg_statistic_ext staForm;

    entry = palloc0(sizeof(StatExtEntry));
    staForm = (Form_pg_statistic_ext)GETSTRUCT(htup);
    entry->statOid = staForm->oid;
    entry->schema = get_namespace_name(staForm->stxnamespace);
    entry->name = pstrdup(NameStr(staForm->stxname));
    for (i = 0; i < staForm->stxkeys.dim1; i++)
    {
      entry->columns = bms_add_member(entry->columns, staForm->stxkeys.values[i]);
    }

                                                            
    datum = SysCacheGetAttr(STATEXTOID, htup, Anum_pg_statistic_ext_stxkind, &isnull);
    Assert(!isnull);
    arr = DatumGetArrayTypeP(datum);
    if (ARR_NDIM(arr) != 1 || ARR_HASNULL(arr) || ARR_ELEMTYPE(arr) != CHAROID)
    {
      elog(ERROR, "stxkind is not a 1-D char array");
    }
    enabled = (char *)ARR_DATA_PTR(arr);
    for (i = 0; i < ARR_DIMS(arr)[0]; i++)
    {
      Assert((enabled[i] == STATS_EXT_NDISTINCT) || (enabled[i] == STATS_EXT_DEPENDENCIES) || (enabled[i] == STATS_EXT_MCV));
      entry->types = lappend_int(entry->types, (int)enabled[i]);
    }

    result = lappend(result, entry);
  }

  systable_endscan(scan);

  return result;
}

   
                                                                          
                                                                     
                                                                             
                                                                                  
                                                     
   
static VacAttrStats **
lookup_var_attr_stats(Relation rel, Bitmapset *attrs, int nvacatts, VacAttrStats **vacatts)
{
  int i = 0;
  int x = -1;
  VacAttrStats **stats;

  stats = (VacAttrStats **)palloc(bms_num_members(attrs) * sizeof(VacAttrStats *));

                                                                        
  while ((x = bms_next_member(attrs, x)) >= 0)
  {
    int j;

    stats[i] = NULL;
    for (j = 0; j < nvacatts; j++)
    {
      if (x == vacatts[j]->tupattnum)
      {
        stats[i] = vacatts[j];
        break;
      }
    }

    if (!stats[i])
    {
         
                                                                   
                                                                       
                      
         
      pfree(stats);
      return NULL;
    }

       
                                                                       
                                  
       
    Assert(!stats[i]->attr->attisdropped);

    i++;
  }

  return stats;
}

   
                 
                                                                            
          
   
static void
statext_store(Oid statOid, MVNDistinct *ndistinct, MVDependencies *dependencies, MCVList *mcv, VacAttrStats **stats)
{
  Relation pg_stextdata;
  HeapTuple stup, oldtup;
  Datum values[Natts_pg_statistic_ext_data];
  bool nulls[Natts_pg_statistic_ext_data];
  bool replaces[Natts_pg_statistic_ext_data];

  pg_stextdata = table_open(StatisticExtDataRelationId, RowExclusiveLock);

  memset(nulls, true, sizeof(nulls));
  memset(replaces, false, sizeof(replaces));
  memset(values, 0, sizeof(values));

     
                                                                           
            
     
  if (ndistinct != NULL)
  {
    bytea *data = statext_ndistinct_serialize(ndistinct);

    nulls[Anum_pg_statistic_ext_data_stxdndistinct - 1] = (data == NULL);
    values[Anum_pg_statistic_ext_data_stxdndistinct - 1] = PointerGetDatum(data);
  }

  if (dependencies != NULL)
  {
    bytea *data = statext_dependencies_serialize(dependencies);

    nulls[Anum_pg_statistic_ext_data_stxddependencies - 1] = (data == NULL);
    values[Anum_pg_statistic_ext_data_stxddependencies - 1] = PointerGetDatum(data);
  }
  if (mcv != NULL)
  {
    bytea *data = statext_mcv_serialize(mcv, stats);

    nulls[Anum_pg_statistic_ext_data_stxdmcv - 1] = (data == NULL);
    values[Anum_pg_statistic_ext_data_stxdmcv - 1] = PointerGetDatum(data);
  }

                                                          
  replaces[Anum_pg_statistic_ext_data_stxdndistinct - 1] = true;
  replaces[Anum_pg_statistic_ext_data_stxddependencies - 1] = true;
  replaces[Anum_pg_statistic_ext_data_stxdmcv - 1] = true;

                                                             
  oldtup = SearchSysCache1(STATEXTDATASTXOID, ObjectIdGetDatum(statOid));
  if (!HeapTupleIsValid(oldtup))
  {
    elog(ERROR, "cache lookup failed for statistics object %u", statOid);
  }

                  
  stup = heap_modify_tuple(oldtup, RelationGetDescr(pg_stextdata), values, nulls, replaces);
  ReleaseSysCache(oldtup);
  CatalogTupleUpdate(pg_stextdata, &stup->t_self, stup);

  heap_freetuple(stup);

  table_close(pg_stextdata, RowExclusiveLock);
}

                                       
MultiSortSupport
multi_sort_init(int ndims)
{
  MultiSortSupport mss;

  Assert(ndims >= 2);

  mss = (MultiSortSupport)palloc0(offsetof(MultiSortSupportData, ssup) + sizeof(SortSupportData) * ndims);

  mss->ndims = ndims;

  return mss;
}

   
                                                                         
                             
   
void
multi_sort_add_dimension(MultiSortSupport mss, int sortdim, Oid oper, Oid collation)
{
  SortSupport ssup = &mss->ssup[sortdim];

  ssup->ssup_cxt = CurrentMemoryContext;
  ssup->ssup_collation = collation;
  ssup->ssup_nulls_first = false;

  PrepareSortSupportFromOrderingOp(oper, ssup);
}

                                                      
int
multi_sort_compare(const void *a, const void *b, void *arg)
{
  MultiSortSupport mss = (MultiSortSupport)arg;
  SortItem *ia = (SortItem *)a;
  SortItem *ib = (SortItem *)b;
  int i;

  for (i = 0; i < mss->ndims; i++)
  {
    int compare;

    compare = ApplySortComparator(ia->values[i], ia->isnull[i], ib->values[i], ib->isnull[i], &mss->ssup[i]);

    if (compare != 0)
    {
      return compare;
    }
  }

                        
  return 0;
}

                                
int
multi_sort_compare_dim(int dim, const SortItem *a, const SortItem *b, MultiSortSupport mss)
{
  return ApplySortComparator(a->values[dim], a->isnull[dim], b->values[dim], b->isnull[dim], &mss->ssup[dim]);
}

int
multi_sort_compare_dims(int start, int end, const SortItem *a, const SortItem *b, MultiSortSupport mss)
{
  int dim;

  for (dim = start; dim <= end; dim++)
  {
    int r = ApplySortComparator(a->values[dim], a->isnull[dim], b->values[dim], b->isnull[dim], &mss->ssup[dim]);

    if (r != 0)
    {
      return r;
    }
  }

  return 0;
}

int
compare_scalars_simple(const void *a, const void *b, void *arg)
{
  return compare_datums_simple(*(Datum *)a, *(Datum *)b, (SortSupport)arg);
}

int
compare_datums_simple(Datum a, Datum b, SortSupport ssup)
{
  return ApplySortComparator(a, false, b, false, ssup);
}

                                     
void *
bsearch_arg(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *, void *), void *arg)
{
  size_t l, u, idx;
  const void *p;
  int comparison;

  l = 0;
  u = nmemb;
  while (l < u)
  {
    idx = (l + u) / 2;
    p = (void *)(((const char *)base) + (idx * size));
    comparison = (*compar)(key, p, arg);

    if (comparison < 0)
    {
      u = idx;
    }
    else if (comparison > 0)
    {
      l = idx + 1;
    }
    else
    {
      return (void *)p;
    }
  }

  return NULL;
}

   
                       
                                                            
   
                                                                           
                                                                             
                                                         
   
AttrNumber *
build_attnums_array(Bitmapset *attrs, int *numattrs)
{
  int i, j;
  AttrNumber *attnums;
  int num = bms_num_members(attrs);

  if (numattrs)
  {
    *numattrs = num;
  }

                                        
  attnums = (AttrNumber *)palloc(sizeof(AttrNumber) * num);
  i = 0;
  j = -1;
  while ((j = bms_next_member(attrs, j)) >= 0)
  {
       
                                                                      
                                                                          
                                                                           
                                                             
       
    Assert(AttrNumberIsForUserDefinedAttr(j));
    Assert(j <= MaxAttrNumber);

    attnums[i++] = (AttrNumber)j;

                                   
    Assert(i <= num);
  }

  return attnums;
}

   
                      
                                                           
   
                                                                           
                                                           
   
SortItem *
build_sorted_items(int numrows, int *nitems, HeapTuple *rows, TupleDesc tdesc, MultiSortSupport mss, int numattrs, AttrNumber *attnums)
{
  int i, j, len, idx;
  int nvalues = numrows * numattrs;

  SortItem *items;
  Datum *values;
  bool *isnull;
  char *ptr;

                                                                           
  len = numrows * sizeof(SortItem) + nvalues * (sizeof(Datum) + sizeof(bool));

                                                         
  ptr = palloc0(len);

                     
  items = (SortItem *)ptr;
  ptr += numrows * sizeof(SortItem);

                             
  values = (Datum *)ptr;
  ptr += nvalues * sizeof(Datum);

  isnull = (bool *)ptr;
  ptr += nvalues * sizeof(bool);

                                                      
  Assert((ptr - (char *)items) == len);

                                                 
  idx = 0;
  for (i = 0; i < numrows; i++)
  {
    bool toowide = false;

    items[idx].values = &values[idx * numattrs];
    items[idx].isnull = &isnull[idx * numattrs];

                                                     
    for (j = 0; j < numattrs; j++)
    {
      Datum value;
      bool isnull;

      value = heap_getattr(rows[i], attnums[j], tdesc, &isnull);

         
                                                                       
                                                                
         
                                                                       
                                                                       
                                                                        
                                                      
         
      if ((!isnull) && (TupleDescAttr(tdesc, attnums[j] - 1)->attlen == -1))
      {
        if (toast_raw_datum_size(value) > WIDTH_THRESHOLD)
        {
          toowide = true;
          break;
        }

        value = PointerGetDatum(PG_DETOAST_DATUM(value));
      }

      items[idx].values[j] = value;
      items[idx].isnull[j] = isnull;
    }

    if (toowide)
    {
      continue;
    }

    idx++;
  }

                                                                     
  *nitems = idx;

                               
  if (idx == 0)
  {
                                                   
    pfree(items);
    return NULL;
  }

                                         
  qsort_arg((void *)items, idx, sizeof(SortItem), multi_sort_compare, mss);

  return items;
}

   
                     
                                                              
   
bool
has_stats_of_kind(List *stats, char requiredkind)
{
  ListCell *l;

  foreach (l, stats)
  {
    StatisticExtInfo *stat = (StatisticExtInfo *)lfirst(l);

    if (stat->kind == requiredkind)
    {
      return true;
    }
  }

  return false;
}

   
                          
                                                                           
                                                                            
                      
   
                                                                            
                                                                            
                                                                        
   
                                                                             
                                                                              
              
   
                                                                              
                                                                              
                                   
   
StatisticExtInfo *
choose_best_statistics(List *stats, char requiredkind, Bitmapset **clause_attnums, int nclauses)
{
  ListCell *lc;
  StatisticExtInfo *best_match = NULL;
  int best_num_matched = 2;                                                
  int best_match_keys = (STATS_MAX_DIMENSIONS + 1);                        

  foreach (lc, stats)
  {
    int i;
    StatisticExtInfo *info = (StatisticExtInfo *)lfirst(lc);
    Bitmapset *matched = NULL;
    int num_matched;
    int numkeys;

                                                          
    if (info->kind != requiredkind)
    {
      continue;
    }

       
                                                                           
                                 
       
    for (i = 0; i < nclauses; i++)
    {
                                                 
      if (!clause_attnums[i])
      {
        continue;
      }

                                                              
      if (!bms_is_subset(clause_attnums[i], info->keys))
      {
        continue;
      }

      matched = bms_add_members(matched, clause_attnums[i]);
    }

    num_matched = bms_num_members(matched);
    bms_free(matched);

       
                                                                         
                                                        
       
    numkeys = bms_num_members(info->keys);

       
                                                                          
                                                                          
                                           
       
    if (num_matched > best_num_matched || (num_matched == best_num_matched && numkeys < best_match_keys))
    {
      best_match = info;
      best_num_matched = num_matched;
      best_match_keys = numkeys;
    }
  }

  return best_match;
}

   
                                         
                                                           
   
                                                                 
                                                                        
                                                                             
                                                                
   
static bool
statext_is_compatible_clause_internal(PlannerInfo *root, Node *clause, Index relid, Bitmapset **attnums)
{
                                                                             
  if (IsA(clause, RelabelType))
  {
    clause = (Node *)((RelabelType *)clause)->arg;
  }

                                                               
  if (IsA(clause, Var))
  {
    Var *var = (Var *)clause;

                                                 
    if (var->varno != relid)
    {
      return false;
    }

                                                                 
    if (var->varlevelsup > 0)
    {
      return false;
    }

                                                                      
    if (!AttrNumberIsForUserDefinedAttr(var->varattno))
    {
      return false;
    }

    *attnums = bms_add_member(*attnums, var->varattno);

    return true;
  }

                                        
  if (is_opclause(clause))
  {
    RangeTblEntry *rte = root->simple_rte_array[relid];
    OpExpr *expr = (OpExpr *)clause;
    Var *var;

                                                                        
    if (list_length(expr->args) != 2)
    {
      return false;
    }

                                                                      
    if (!examine_opclause_expression(expr, &var, NULL, NULL))
    {
      return false;
    }

       
                                                                         
                                                                      
       
                                                                           
                                               
       
    switch (get_oprrest(expr->opno))
    {
    case F_EQSEL:
    case F_NEQSEL:
    case F_SCALARLTSEL:
    case F_SCALARLESEL:
    case F_SCALARGTSEL:
    case F_SCALARGESEL:
                                                               
      break;

    default:
                                                               
      return false;
    }

       
                                                                       
                                                                           
                                                                        
       
                                                                         
                                                                           
                                                                  
                          
       
    if (rte->securityQuals != NIL && !get_func_leakproof(get_opcode(expr->opno)))
    {
      return false;
    }

    return statext_is_compatible_clause_internal(root, (Node *)var, relid, attnums);
  }

                         
  if (is_andclause(clause) || is_orclause(clause) || is_notclause(clause))
  {
       
                                                                         
       
                                                                           
                                                                       
                                                                         
                                                                          
                                             
       
                                                                        
                                                                          
                                                                           
                                                            
       
    BoolExpr *expr = (BoolExpr *)clause;
    ListCell *lc;

    foreach (lc, expr->args)
    {
         
                                                                      
                                       
         
      if (!statext_is_compatible_clause_internal(root, (Node *)lfirst(lc), relid, attnums))
      {
        return false;
      }
    }

    return true;
  }

                   
  if (IsA(clause, NullTest))
  {
    NullTest *nt = (NullTest *)clause;

       
                                                                         
                                               
       
    if (!IsA(nt->arg, Var))
    {
      return false;
    }

    return statext_is_compatible_clause_internal(root, (Node *)(nt->arg), relid, attnums);
  }

  return false;
}

   
                                
                                                           
   
                                                      
   
                                                                           
                                         
   
                           
   
                                     
   
                                                                         
                                            
   
static bool
statext_is_compatible_clause(PlannerInfo *root, Node *clause, Index relid, Bitmapset **attnums)
{
  RangeTblEntry *rte = root->simple_rte_array[relid];
  RestrictInfo *rinfo = (RestrictInfo *)clause;
  Oid userid;

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

                                                                     
  if (!statext_is_compatible_clause_internal(root, (Node *)rinfo->clause, relid, attnums))
  {
    return false;
  }

     
                                                                           
                                                                            
     
  userid = rte->checkAsUser ? rte->checkAsUser : GetUserId();

  if (pg_class_aclcheck(rte->relid, userid, ACL_SELECT) != ACLCHECK_OK)
  {
                                                                   
    if (bms_is_member(InvalidAttrNumber, *attnums))
    {
                                                                       
      if (pg_attribute_aclcheck_all(rte->relid, userid, ACL_SELECT, ACLMASK_ALL) != ACLCHECK_OK)
      {
        return false;
      }
    }
    else
    {
                                                      
      int attnum = -1;

      while ((attnum = bms_next_member(*attnums, attnum)) >= 0)
      {
        if (pg_attribute_aclcheck(rte->relid, attnum, userid, ACL_SELECT) != ACLCHECK_OK)
        {
          return false;
        }
      }
    }
  }

                                          
  return true;
}

   
                                      
                                                             
   
                                                                              
                                                                               
                                                      
   
                                                                             
                                                                            
                                                                            
                                                                  
   
                                                                              
                                    
   
                                                                             
                                                                             
                                                       
   
                                                                   
   
                                                                               
                                                                            
                 
   
                                                                            
                                                                             
                                                                              
                                                                              
                                                                             
                                                                             
                                                      
   
                                                                             
                                                                             
                                      
   
                                                                           
                                                                             
                                                                           
                                       
   
                                                                           
                                                                           
                                                                  
   
                                                                         
                                                                             
                           
   
                                                                             
                                                                              
                                                                               
                                           
   
static Selectivity
statext_mcv_clauselist_selectivity(PlannerInfo *root, List *clauses, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo, RelOptInfo *rel, Bitmapset **estimatedclauses)
{
  ListCell *l;
  Bitmapset **list_attnums;
  int listidx;
  StatisticExtInfo *stat;
  List *stat_clauses;
  Selectivity simple_sel, mcv_sel, mcv_basesel, mcv_totalsel, other_sel, sel;
  RangeTblEntry *rte = planner_rt_fetch(rel->relid, root);

     
                                                                        
                                                                     
                                                                         
                                                                          
                                                              
     
  if (rte->inh && rte->relkind != RELKIND_PARTITIONED_TABLE)
  {
    return 1.0;
  }

                                                               
  if (!has_stats_of_kind(rel->statlist, STATS_EXT_MCV))
  {
    return 1.0;
  }

  list_attnums = (Bitmapset **)palloc(sizeof(Bitmapset *) * list_length(clauses));

     
                                                                            
                                                                          
                                                                             
                                                                        
                                                                           
                                
     
                                                                             
                                                 
     
  listidx = 0;
  foreach (l, clauses)
  {
    Node *clause = (Node *)lfirst(l);
    Bitmapset *attnums = NULL;

    if (!bms_is_member(listidx, *estimatedclauses) && statext_is_compatible_clause(root, clause, rel->relid, &attnums))
    {
      list_attnums[listidx] = attnums;
    }
    else
    {
      list_attnums[listidx] = NULL;
    }

    listidx++;
  }

                                                                
  stat = choose_best_statistics(rel->statlist, STATS_EXT_MCV, list_attnums, list_length(clauses));

                                                                    
  if (!stat)
  {
    return 1.0;
  }

                                                                      
  Assert(stat->kind == STATS_EXT_MCV);

                                                                     
  stat_clauses = NIL;

  listidx = 0;
  foreach (l, clauses)
  {
       
                                                                         
                                                        
       
    if (list_attnums[listidx] != NULL && bms_is_subset(list_attnums[listidx], stat->keys))
    {
      stat_clauses = lappend(stat_clauses, (Node *)lfirst(l));
      *estimatedclauses = bms_add_member(*estimatedclauses, listidx);
    }

    listidx++;
  }

     
                                                                   
                                                              
                                                                             
                             
     
  simple_sel = clauselist_selectivity_simple(root, stat_clauses, varRelid, jointype, sjinfo, NULL);

     
                                                                             
                                                     
     
  mcv_sel = mcv_clauselist_selectivity(root, stat, stat_clauses, varRelid, jointype, sjinfo, rel, &mcv_basesel, &mcv_totalsel);

                                                                  
  other_sel = simple_sel - mcv_basesel;
  CLAMP_PROBABILITY(other_sel);

                                                                  
  if (other_sel > 1.0 - mcv_totalsel)
  {
    other_sel = 1.0 - mcv_totalsel;
  }

                                                                            
  sel = mcv_sel + other_sel;
  CLAMP_PROBABILITY(sel);

  return sel;
}

   
                                  
                                                             
   
Selectivity
statext_clauselist_selectivity(PlannerInfo *root, List *clauses, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo, RelOptInfo *rel, Bitmapset **estimatedclauses)
{
  Selectivity sel;

                                                                    
  sel = statext_mcv_clauselist_selectivity(root, clauses, varRelid, jointype, sjinfo, rel, estimatedclauses);

     
                                                                             
                                                                          
                                                                 
     
                                                                           
                                                                           
                                         
     
                                                                          
                                                                             
                                                   
     
  sel *= dependencies_clauselist_selectivity(root, clauses, varRelid, jointype, sjinfo, rel, estimatedclauses);

  return sel;
}

   
                               
                                               
   
                                                                               
                                                                              
                                          
   
                                                                             
                                                                                
                                                        
   
bool
examine_opclause_expression(OpExpr *expr, Var **varp, Const **cstp, bool *varonleftp)
{
  Var *var;
  Const *cst;
  bool varonleft;
  Node *leftop, *rightop;

                                                         
  Assert(list_length(expr->args) == 2);

  leftop = linitial(expr->args);
  rightop = lsecond(expr->args);

                                                            
  if (IsA(leftop, RelabelType))
  {
    leftop = (Node *)((RelabelType *)leftop)->arg;
  }

  if (IsA(rightop, RelabelType))
  {
    rightop = (Node *)((RelabelType *)rightop)->arg;
  }

  if (IsA(leftop, Var) && IsA(rightop, Const))
  {
    var = (Var *)leftop;
    cst = (Const *)rightop;
    varonleft = true;
  }
  else if (IsA(leftop, Const) && IsA(rightop, Var))
  {
    var = (Var *)rightop;
    cst = (Const *)leftop;
    varonleft = false;
  }
  else
  {
    return false;
  }

                                                           
  if (varp)
  {
    *varp = var;
  }

  if (cstp)
  {
    *cstp = cst;
  }

  if (varonleftp)
  {
    *varonleftp = varonleft;
  }

  return true;
}
