                                                                            
   
              
                                                                 
   
                                                                         
               
   
                                                  
                                                        
                                                               
                                                                            
                                                                         
                                                                                      
                                                                        
   
                                                                         
                                                                              
                                                                          
                                                                         
   
                                                                        
                                                                       
                                                                      
                             
   
                                                                          
                                                                           
                                                                         
                                                                     
   
                                                                               
                                                                               
                                                                               
                                                                   
   
                                                
                                                         
                                                                      
                                                                           
                                                                             
                                                                        
                                                                            
                                 
                                                                 
                                                                            
                                                                            
                                                                             
                                                                              
                                                                       
              
   
                                                                           
                                                                              
                                                                         
                                                                        
                                                                          
                                                                         
                                                                            
           
   
   
                                                                         
                                                                        
   
                  
                                           
   
                                                                            
   

#include "postgres.h"

#include <math.h>

#include "access/amapi.h"
#include "access/htup_details.h"
#include "access/tsmapi.h"
#include "executor/executor.h"
#include "executor/nodeHash.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "optimizer/paths.h"
#include "optimizer/placeholder.h"
#include "optimizer/plancat.h"
#include "optimizer/planmain.h"
#include "optimizer/restrictinfo.h"
#include "parser/parsetree.h"
#include "utils/lsyscache.h"
#include "utils/selfuncs.h"
#include "utils/spccache.h"
#include "utils/tuplesort.h"

#define LOG2(x) (log(x) / 0.693147180559945)

   
                                                                              
                                                                            
                                                              
   
#define APPEND_CPU_COST_MULTIPLIER 0.5

double seq_page_cost = DEFAULT_SEQ_PAGE_COST;
double random_page_cost = DEFAULT_RANDOM_PAGE_COST;
double cpu_tuple_cost = DEFAULT_CPU_TUPLE_COST;
double cpu_index_tuple_cost = DEFAULT_CPU_INDEX_TUPLE_COST;
double cpu_operator_cost = DEFAULT_CPU_OPERATOR_COST;
double parallel_tuple_cost = DEFAULT_PARALLEL_TUPLE_COST;
double parallel_setup_cost = DEFAULT_PARALLEL_SETUP_COST;

int effective_cache_size = DEFAULT_EFFECTIVE_CACHE_SIZE;

Cost disable_cost = 1.0e10;

int max_parallel_workers_per_gather = 2;

bool enable_seqscan = true;
bool enable_indexscan = true;
bool enable_indexonlyscan = true;
bool enable_bitmapscan = true;
bool enable_tidscan = true;
bool enable_sort = true;
bool enable_hashagg = true;
bool enable_nestloop = true;
bool enable_material = true;
bool enable_mergejoin = true;
bool enable_hashjoin = true;
bool enable_gathermerge = true;
bool enable_partitionwise_join = false;
bool enable_partitionwise_aggregate = false;
bool enable_parallel_append = true;
bool enable_parallel_hash = true;
bool enable_partition_pruning = true;

typedef struct
{
  PlannerInfo *root;
  QualCost total;
} cost_qual_eval_context;

static List *
extract_nonindex_conditions(List *qual_clauses, List *indexclauses);
static MergeScanSelCache *
cached_scansel(PlannerInfo *root, RestrictInfo *rinfo, PathKey *pathkey);
static void
cost_rescan(PlannerInfo *root, Path *path, Cost *rescan_startup_cost, Cost *rescan_total_cost);
static bool
cost_qual_eval_walker(Node *node, cost_qual_eval_context *context);
static void
get_restriction_qual_cost(PlannerInfo *root, RelOptInfo *baserel, ParamPathInfo *param_info, QualCost *qpqual_cost);
static bool
has_indexed_join_quals(NestPath *joinpath);
static double
approx_tuple_count(PlannerInfo *root, JoinPath *path, List *quals);
static double
calc_joinrel_size_estimate(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel, double outer_rows, double inner_rows, SpecialJoinInfo *sjinfo, List *restrictlist);
static Selectivity
get_foreign_key_join_selectivity(PlannerInfo *root, Relids outer_relids, Relids inner_relids, SpecialJoinInfo *sjinfo, List **restrictlist);
static Cost
append_nonpartial_cost(List *subpaths, int numpaths, int parallel_workers);
static void
set_rel_width(PlannerInfo *root, RelOptInfo *rel);
static double
relation_byte_size(double tuples, int width);
static double
page_size(double tuples, int width);
static double
get_parallel_divisor(Path *path);

   
                 
                                                
   
double
clamp_row_est(double nrows)
{
     
                                                                        
                                                                           
                              
     
  if (nrows <= 1.0)
  {
    nrows = 1.0;
  }
  else
  {
    nrows = rint(nrows);
  }

  return nrows;
}

   
                
                                                                          
   
                                           
                                                                                
   
void
cost_seqscan(Path *path, PlannerInfo *root, RelOptInfo *baserel, ParamPathInfo *param_info)
{
  Cost startup_cost = 0;
  Cost cpu_run_cost;
  Cost disk_run_cost;
  double spc_seq_page_cost;
  QualCost qpqual_cost;
  Cost cpu_per_tuple;

                                                
  Assert(baserel->relid > 0);
  Assert(baserel->rtekind == RTE_RELATION);

                                                   
  if (param_info)
  {
    path->rows = param_info->ppi_rows;
  }
  else
  {
    path->rows = baserel->rows;
  }

  if (!enable_seqscan)
  {
    startup_cost += disable_cost;
  }

                                                                 
  get_tablespace_page_costs(baserel->reltablespace, NULL, &spc_seq_page_cost);

     
                
     
  disk_run_cost = spc_seq_page_cost * baserel->pages;

                 
  get_restriction_qual_cost(root, baserel, param_info, &qpqual_cost);

  startup_cost += qpqual_cost.startup;
  cpu_per_tuple = cpu_tuple_cost + qpqual_cost.per_tuple;
  cpu_run_cost = cpu_per_tuple * baserel->tuples;
                                                                       
  startup_cost += path->pathtarget->cost.startup;
  cpu_run_cost += path->pathtarget->cost.per_tuple * path->rows;

                                                
  if (path->parallel_workers > 0)
  {
    double parallel_divisor = get_parallel_divisor(path);

                                                        
    cpu_run_cost /= parallel_divisor;

       
                                                                         
                                                                           
                                                                        
                         
       

       
                                                                        
                                                  
       
    path->rows = clamp_row_est(path->rows / parallel_divisor);
  }

  path->startup_cost = startup_cost;
  path->total_cost = startup_cost + cpu_run_cost + disk_run_cost;
}

   
                   
                                                                            
   
                                           
                                                                                
   
void
cost_samplescan(Path *path, PlannerInfo *root, RelOptInfo *baserel, ParamPathInfo *param_info)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;
  RangeTblEntry *rte;
  TableSampleClause *tsc;
  TsmRoutine *tsm;
  double spc_seq_page_cost, spc_random_page_cost, spc_page_cost;
  QualCost qpqual_cost;
  Cost cpu_per_tuple;

                                                                         
  Assert(baserel->relid > 0);
  rte = planner_rt_fetch(baserel->relid, root);
  Assert(rte->rtekind == RTE_RELATION);
  tsc = rte->tablesample;
  Assert(tsc != NULL);
  tsm = GetTsmRoutine(tsc->tsmhandler);

                                                   
  if (param_info)
  {
    path->rows = param_info->ppi_rows;
  }
  else
  {
    path->rows = baserel->rows;
  }

                                                                 
  get_tablespace_page_costs(baserel->reltablespace, &spc_random_page_cost, &spc_seq_page_cost);

                                                                         
  spc_page_cost = (tsm->NextSampleBlock != NULL) ? spc_random_page_cost : spc_seq_page_cost;

     
                                                                        
                                                     
     
  run_cost += spc_page_cost * baserel->pages;

     
                                                                        
                                                                             
                                                                           
                                                                        
                                                                     
                                                           
     
  get_restriction_qual_cost(root, baserel, param_info, &qpqual_cost);

  startup_cost += qpqual_cost.startup;
  cpu_per_tuple = cpu_tuple_cost + qpqual_cost.per_tuple;
  run_cost += cpu_per_tuple * baserel->tuples;
                                                                       
  startup_cost += path->pathtarget->cost.startup;
  run_cost += path->pathtarget->cost.per_tuple * path->rows;

  path->startup_cost = startup_cost;
  path->total_cost = startup_cost + run_cost;
}

   
               
                                                     
   
                                             
                                                                                
                                                                            
                                                                              
                                            
   
void
cost_gather(GatherPath *path, PlannerInfo *root, RelOptInfo *rel, ParamPathInfo *param_info, double *rows)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;

                                                   
  if (rows)
  {
    path->path.rows = *rows;
  }
  else if (param_info)
  {
    path->path.rows = param_info->ppi_rows;
  }
  else
  {
    path->path.rows = rel->rows;
  }

  startup_cost = path->subpath->startup_cost;

  run_cost = path->subpath->total_cost - path->subpath->startup_cost;

                                              
  startup_cost += parallel_setup_cost;
  run_cost += parallel_tuple_cost * path->path.rows;

  path->path.startup_cost = startup_cost;
  path->path.total_cost = (startup_cost + run_cost);
}

   
                     
                                                           
   
                                                                             
                                                                           
                                                                               
                                                                         
                                                                        
   
void
cost_gather_merge(GatherMergePath *path, PlannerInfo *root, RelOptInfo *rel, ParamPathInfo *param_info, Cost input_startup_cost, Cost input_total_cost, double *rows)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;
  Cost comparison_cost;
  double N;
  double logN;

                                                   
  if (rows)
  {
    path->path.rows = *rows;
  }
  else if (param_info)
  {
    path->path.rows = param_info->ppi_rows;
  }
  else
  {
    path->path.rows = rel->rows;
  }

  if (!enable_gathermerge)
  {
    startup_cost += disable_cost;
  }

     
                                                                             
                                                                           
                                                     
     
  Assert(path->num_workers > 0);
  N = (double)path->num_workers + 1;
  logN = LOG2(N);

                                         
  comparison_cost = 2.0 * cpu_operator_cost;

                          
  startup_cost += comparison_cost * N * logN;

                                       
  run_cost += path->path.rows * comparison_cost * logN;

                                                              
  run_cost += cpu_operator_cost * path->path.rows;

     
                                                                        
                                                                        
                                                                           
                                                    
     
  startup_cost += parallel_setup_cost;
  run_cost += parallel_tuple_cost * path->path.rows * 1.05;

  path->path.startup_cost = startup_cost + input_startup_cost;
  path->path.total_cost = (startup_cost + run_cost + input_total_cost);
}

   
              
                                                                            
   
                                                                       
                                                    
                                                                             
                                  
   
                                                                           
                                                                            
                                                         
   
                                                                    
                                                                           
                                                                         
                                                                        
   
void
cost_index(IndexPath *path, PlannerInfo *root, double loop_count, bool partial_path)
{
  IndexOptInfo *index = path->indexinfo;
  RelOptInfo *baserel = index->rel;
  bool indexonly = (path->path.pathtype == T_IndexOnlyScan);
  amcostestimate_function amcostestimate;
  List *qpquals;
  Cost startup_cost = 0;
  Cost run_cost = 0;
  Cost cpu_run_cost = 0;
  Cost indexStartupCost;
  Cost indexTotalCost;
  Selectivity indexSelectivity;
  double indexCorrelation, csquared;
  double spc_seq_page_cost, spc_random_page_cost;
  Cost min_IO_cost, max_IO_cost;
  QualCost qpqual_cost;
  Cost cpu_per_tuple;
  double tuples_fetched;
  double pages_fetched;
  double rand_heap_pages;
  double index_pages;

                                                
  Assert(IsA(baserel, RelOptInfo) && IsA(index, IndexOptInfo));
  Assert(baserel->relid > 0);
  Assert(baserel->rtekind == RTE_RELATION);

     
                                                                           
                                                                            
                                                                             
                                                                          
          
     
  if (path->path.param_info)
  {
    path->path.rows = path->path.param_info->ppi_rows;
                                                                         
    qpquals = list_concat(extract_nonindex_conditions(path->indexinfo->indrestrictinfo, path->indexclauses), extract_nonindex_conditions(path->path.param_info->ppi_clauses, path->indexclauses));
  }
  else
  {
    path->path.rows = baserel->rows;
                                                              
    qpquals = extract_nonindex_conditions(path->indexinfo->indrestrictinfo, path->indexclauses);
  }

  if (!enable_indexscan)
  {
    startup_cost += disable_cost;
  }
                                                                         

     
                                                                            
                                                                          
                                                                         
                                                                             
                                                                       
     
  amcostestimate = (amcostestimate_function)index->amcostestimate;
  amcostestimate(root, path, loop_count, &indexStartupCost, &indexTotalCost, &indexSelectivity, &indexCorrelation, &index_pages);

     
                                                                             
                                                                             
                                            
     
  path->indextotalcost = indexTotalCost;
  path->indexselectivity = indexSelectivity;

                                                         
  startup_cost += indexStartupCost;
  run_cost += indexTotalCost - indexStartupCost;

                                                    
  tuples_fetched = clamp_row_est(indexSelectivity * baserel->tuples);

                                                                  
  get_tablespace_page_costs(baserel->reltablespace, &spc_random_page_cost, &spc_seq_page_cost);

               
                                                                        
     
                                                                      
                                                                 
                                                                       
                                                                     
     
                                                                           
                                                                             
                                                                          
                                                                          
                                                                      
                     
                                                                     
                                                                            
                                                                         
                                                                            
     
                                                                         
                                                                      
                                                                     
                                                                          
                                                                        
                                                                      
               
     
  if (loop_count > 1)
  {
       
                                                                 
                                                                        
                                                                         
                                                                   
                                                                        
                                    
       
    pages_fetched = index_pages_fetched(tuples_fetched * loop_count, baserel->pages, (double)index->pages, root);

    if (indexonly)
    {
      pages_fetched = ceil(pages_fetched * (1.0 - baserel->allvisfrac));
    }

    rand_heap_pages = pages_fetched;

    max_IO_cost = (pages_fetched * spc_random_page_cost) / loop_count;

       
                                                                        
                                                                         
                                                                         
                                                                           
                                                                           
                                                                      
                                                                          
                                                              
       
    pages_fetched = ceil(indexSelectivity * (double)baserel->pages);

    pages_fetched = index_pages_fetched(pages_fetched * loop_count, baserel->pages, (double)index->pages, root);

    if (indexonly)
    {
      pages_fetched = ceil(pages_fetched * (1.0 - baserel->allvisfrac));
    }

    min_IO_cost = (pages_fetched * spc_random_page_cost) / loop_count;
  }
  else
  {
       
                                                                   
                                                                    
       
    pages_fetched = index_pages_fetched(tuples_fetched, baserel->pages, (double)index->pages, root);

    if (indexonly)
    {
      pages_fetched = ceil(pages_fetched * (1.0 - baserel->allvisfrac));
    }

    rand_heap_pages = pages_fetched;

                                                                         
    max_IO_cost = pages_fetched * spc_random_page_cost;

                                                                       
    pages_fetched = ceil(indexSelectivity * (double)baserel->pages);

    if (indexonly)
    {
      pages_fetched = ceil(pages_fetched * (1.0 - baserel->allvisfrac));
    }

    if (pages_fetched > 0)
    {
      min_IO_cost = spc_random_page_cost;
      if (pages_fetched > 1)
      {
        min_IO_cost += (pages_fetched - 1) * spc_seq_page_cost;
      }
    }
    else
    {
      min_IO_cost = 0;
    }
  }

  if (partial_path)
  {
       
                                                                           
                                                                          
                                                                    
       
    if (indexonly)
    {
      rand_heap_pages = -1;
    }

       
                                                                           
                                                                           
                                                                         
              
       
    path->path.parallel_workers = compute_parallel_worker(baserel, rand_heap_pages, index_pages, max_parallel_workers_per_gather);

       
                                                                           
                                                                          
                                
       
    if (path->path.parallel_workers <= 0)
    {
      return;
    }

    path->path.parallel_aware = true;
  }

     
                                                                             
                                            
     
  csquared = indexCorrelation * indexCorrelation;

  run_cost += max_IO_cost + csquared * (min_IO_cost - max_IO_cost);

     
                                   
     
                                                                          
                                                       
     
  cost_qual_eval(&qpqual_cost, qpquals, root);

  startup_cost += qpqual_cost.startup;
  cpu_per_tuple = cpu_tuple_cost + qpqual_cost.per_tuple;

  cpu_run_cost += cpu_per_tuple * tuples_fetched;

                                                                       
  startup_cost += path->path.pathtarget->cost.startup;
  cpu_run_cost += path->path.pathtarget->cost.per_tuple * path->path.rows;

                                                
  if (path->path.parallel_workers > 0)
  {
    double parallel_divisor = get_parallel_divisor(&path->path);

    path->path.rows = clamp_row_est(path->path.rows / parallel_divisor);

                                                        
    cpu_run_cost /= parallel_divisor;
  }

  run_cost += cpu_run_cost;

  path->path.startup_cost = startup_cost;
  path->path.total_cost = startup_cost + run_cost;
}

   
                               
   
                                                                               
                                                                            
                                                                           
                                                                             
                                                                               
                                                                            
                                                                             
                                                                             
                                               
   
                                                             
                                           
   
static List *
extract_nonindex_conditions(List *qual_clauses, List *indexclauses)
{
  List *result = NIL;
  ListCell *lc;

  foreach (lc, qual_clauses)
  {
    RestrictInfo *rinfo = lfirst_node(RestrictInfo, lc);

    if (rinfo->pseudoconstant)
    {
      continue;                                       
    }
    if (is_redundant_with_indexclauses(rinfo, indexclauses))
    {
      continue;                                                
    }
                                                                        
    result = lappend(result, rinfo);
  }
  return result;
}

   
                       
                                                                        
                    
   
                                                                        
                                                                       
                                                                       
                                                                    
              
        
                                       
                                                     
                                                                 
         
                         
                          
                                                      
                                                                
   
                                                                           
                                                                         
                                                                           
                                                                          
                                                                             
                                                                              
                                                                               
   
                                                                   
                                                                            
                                                                   
                                                                          
                                    
   
                                                                               
                                                                            
                                   
   
double
index_pages_fetched(double tuples_fetched, BlockNumber pages, double index_pages, PlannerInfo *root)
{
  double pages_fetched;
  double total_pages;
  double T, b;

                                                            
  T = (pages > 1) ? (double)pages : 1.0;

                                                                       
  total_pages = root->total_table_pages + index_pages;
  total_pages = Max(total_pages, 1.0);
  Assert(T <= total_pages);

                                                    
  b = (double)effective_cache_size * T / total_pages;

                                      
  if (b <= 1.0)
  {
    b = 1.0;
  }
  else
  {
    b = ceil(b);
  }

                                                   
  if (T <= b)
  {
    pages_fetched = (2.0 * T * tuples_fetched) / (2.0 * T + tuples_fetched);
    if (pages_fetched >= T)
    {
      pages_fetched = T;
    }
    else
    {
      pages_fetched = ceil(pages_fetched);
    }
  }
  else
  {
    double lim;

    lim = (2.0 * T * b) / (2.0 * T - b);
    if (tuples_fetched <= lim)
    {
      pages_fetched = (2.0 * T * tuples_fetched) / (2.0 * T + tuples_fetched);
    }
    else
    {
      pages_fetched = b + (tuples_fetched - lim) * (T - b) / T;
    }
    pages_fetched = ceil(pages_fetched);
  }
  return pages_fetched;
}

   
                       
                                                                         
   
                                                                            
                                                                          
                                                                             
            
   
static double
get_indexpath_pages(Path *bitmapqual)
{
  double result = 0;
  ListCell *l;

  if (IsA(bitmapqual, BitmapAndPath))
  {
    BitmapAndPath *apath = (BitmapAndPath *)bitmapqual;

    foreach (l, apath->bitmapquals)
    {
      result += get_indexpath_pages((Path *)lfirst(l));
    }
  }
  else if (IsA(bitmapqual, BitmapOrPath))
  {
    BitmapOrPath *opath = (BitmapOrPath *)bitmapqual;

    foreach (l, opath->bitmapquals)
    {
      result += get_indexpath_pages((Path *)lfirst(l));
    }
  }
  else if (IsA(bitmapqual, IndexPath))
  {
    IndexPath *ipath = (IndexPath *)bitmapqual;

    result = (double)ipath->indexinfo->pages;
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", nodeTag(bitmapqual));
  }

  return result;
}

   
                         
                                                                           
                           
   
                                           
                                                                                
                                                                           
                                                                             
                                  
   
                                                                        
                              
   
void
cost_bitmap_heap_scan(Path *path, PlannerInfo *root, RelOptInfo *baserel, ParamPathInfo *param_info, Path *bitmapqual, double loop_count)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;
  Cost indexTotalCost;
  QualCost qpqual_cost;
  Cost cpu_per_tuple;
  Cost cost_per_page;
  Cost cpu_run_cost;
  double tuples_fetched;
  double pages_fetched;
  double spc_seq_page_cost, spc_random_page_cost;
  double T;

                                                
  Assert(IsA(baserel, RelOptInfo));
  Assert(baserel->relid > 0);
  Assert(baserel->rtekind == RTE_RELATION);

                                                   
  if (param_info)
  {
    path->rows = param_info->ppi_rows;
  }
  else
  {
    path->rows = baserel->rows;
  }

  if (!enable_bitmapscan)
  {
    startup_cost += disable_cost;
  }

  pages_fetched = compute_bitmap_pages(root, baserel, bitmapqual, loop_count, &indexTotalCost, &tuples_fetched);

  startup_cost += indexTotalCost;
  T = (baserel->pages > 1) ? (double)baserel->pages : 1.0;

                                                                   
  get_tablespace_page_costs(baserel->reltablespace, &spc_random_page_cost, &spc_seq_page_cost);

     
                                                                      
                                                                             
                                                                    
                                                                         
                                  
     
  if (pages_fetched >= 2.0)
  {
    cost_per_page = spc_random_page_cost - (spc_random_page_cost - spc_seq_page_cost) * sqrt(pages_fetched / T);
  }
  else
  {
    cost_per_page = spc_random_page_cost;
  }

  run_cost += pages_fetched * cost_per_page;

     
                                   
     
                                                                           
                                                                             
                                                                     
                                                                          
                   
     
  get_restriction_qual_cost(root, baserel, param_info, &qpqual_cost);

  startup_cost += qpqual_cost.startup;
  cpu_per_tuple = cpu_tuple_cost + qpqual_cost.per_tuple;
  cpu_run_cost = cpu_per_tuple * tuples_fetched;

                                                
  if (path->parallel_workers > 0)
  {
    double parallel_divisor = get_parallel_divisor(path);

                                                        
    cpu_run_cost /= parallel_divisor;

    path->rows = clamp_row_est(path->rows / parallel_divisor);
  }

  run_cost += cpu_run_cost;

                                                                       
  startup_cost += path->pathtarget->cost.startup;
  run_cost += path->pathtarget->cost.per_tuple * path->rows;

  path->startup_cost = startup_cost;
  path->total_cost = startup_cost + run_cost;
}

   
                         
                                                                        
   
void
cost_bitmap_tree_node(Path *path, Cost *cost, Selectivity *selec)
{
  if (IsA(path, IndexPath))
  {
    *cost = ((IndexPath *)path)->indextotalcost;
    *selec = ((IndexPath *)path)->indexselectivity;

       
                                                                         
                                                                           
                                                                           
                     
       
    *cost += 0.1 * cpu_operator_cost * path->rows;
  }
  else if (IsA(path, BitmapAndPath))
  {
    *cost = path->total_cost;
    *selec = ((BitmapAndPath *)path)->bitmapselectivity;
  }
  else if (IsA(path, BitmapOrPath))
  {
    *cost = path->total_cost;
    *selec = ((BitmapOrPath *)path)->bitmapselectivity;
  }
  else
  {
    elog(ERROR, "unrecognized node type: %d", nodeTag(path));
    *cost = *selec = 0;                          
  }
}

   
                        
                                          
   
                                                                        
                                                                           
                                                                              
                                                                               
            
   
void
cost_bitmap_and_node(BitmapAndPath *path, PlannerInfo *root)
{
  Cost totalCost;
  Selectivity selec;
  ListCell *l;

     
                                                                       
                                                                            
                   
     
                                                                   
                                                                           
                                
     
  totalCost = 0.0;
  selec = 1.0;
  foreach (l, path->bitmapquals)
  {
    Path *subpath = (Path *)lfirst(l);
    Cost subCost;
    Selectivity subselec;

    cost_bitmap_tree_node(subpath, &subCost, &subselec);

    selec *= subselec;

    totalCost += subCost;
    if (l != list_head(path->bitmapquals))
    {
      totalCost += 100.0 * cpu_operator_cost;
    }
  }
  path->bitmapselectivity = selec;
  path->path.rows = 0;                          
  path->path.startup_cost = totalCost;
  path->path.total_cost = totalCost;
}

   
                       
                                         
   
                                          
   
void
cost_bitmap_or_node(BitmapOrPath *path, PlannerInfo *root)
{
  Cost totalCost;
  Selectivity selec;
  ListCell *l;

     
                                                                      
                                                                        
                                                         
     
                                                                  
                                                                       
                                                                      
                                                         
     
  totalCost = 0.0;
  selec = 0.0;
  foreach (l, path->bitmapquals)
  {
    Path *subpath = (Path *)lfirst(l);
    Cost subCost;
    Selectivity subselec;

    cost_bitmap_tree_node(subpath, &subCost, &subselec);

    selec += subselec;

    totalCost += subCost;
    if (l != list_head(path->bitmapquals) && !IsA(subpath, IndexPath))
    {
      totalCost += 100.0 * cpu_operator_cost;
    }
  }
  path->bitmapselectivity = Min(selec, 1.0);
  path->path.rows = 0;                          
  path->path.startup_cost = totalCost;
  path->path.total_cost = totalCost;
}

   
                
                                                                        
   
                                           
                                                 
                                                                                
   
void
cost_tidscan(Path *path, PlannerInfo *root, RelOptInfo *baserel, List *tidquals, ParamPathInfo *param_info)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;
  bool isCurrentOf = false;
  QualCost qpqual_cost;
  Cost cpu_per_tuple;
  QualCost tid_qual_cost;
  int ntuples;
  ListCell *l;
  double spc_random_page_cost;

                                                
  Assert(baserel->relid > 0);
  Assert(baserel->rtekind == RTE_RELATION);

                                                   
  if (param_info)
  {
    path->rows = param_info->ppi_rows;
  }
  else
  {
    path->rows = baserel->rows;
  }

                                                   
  ntuples = 0;
  foreach (l, tidquals)
  {
    RestrictInfo *rinfo = lfirst_node(RestrictInfo, l);
    Expr *qual = rinfo->clause;

    if (IsA(qual, ScalarArrayOpExpr))
    {
                                                    
      ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *)qual;
      Node *arraynode = (Node *)lsecond(saop->args);

      ntuples += estimate_array_length(arraynode);
    }
    else if (IsA(qual, CurrentOfExpr))
    {
                                     
      isCurrentOf = true;
      ntuples++;
    }
    else
    {
                                                     
      ntuples++;
    }
  }

     
                                                                             
                                                                          
                                                                        
                                                                          
                                                                           
                   
     
  if (isCurrentOf)
  {
    Assert(baserel->baserestrictcost.startup >= disable_cost);
    startup_cost -= disable_cost;
  }
  else if (!enable_tidscan)
  {
    startup_cost += disable_cost;
  }

     
                                                                            
                                     
     
  cost_qual_eval(&tid_qual_cost, tidquals, root);

                                                                 
  get_tablespace_page_costs(baserel->reltablespace, &spc_random_page_cost, NULL);

                                                            
  run_cost += spc_random_page_cost * ntuples;

                              
  get_restriction_qual_cost(root, baserel, param_info, &qpqual_cost);

                                                                 
  startup_cost += qpqual_cost.startup + tid_qual_cost.per_tuple;
  cpu_per_tuple = cpu_tuple_cost + qpqual_cost.per_tuple - tid_qual_cost.per_tuple;
  run_cost += cpu_per_tuple * ntuples;

                                                                       
  startup_cost += path->pathtarget->cost.startup;
  run_cost += path->pathtarget->cost.per_tuple * path->rows;

  path->startup_cost = startup_cost;
  path->total_cost = startup_cost + run_cost;
}

   
                     
                                                                 
   
                                           
                                                                                
   
void
cost_subqueryscan(SubqueryScanPath *path, PlannerInfo *root, RelOptInfo *baserel, ParamPathInfo *param_info)
{
  Cost startup_cost;
  Cost run_cost;
  QualCost qpqual_cost;
  Cost cpu_per_tuple;

                                                                    
  Assert(baserel->relid > 0);
  Assert(baserel->rtekind == RTE_SUBQUERY);

                                                   
  if (param_info)
  {
    path->path.rows = param_info->ppi_rows;
  }
  else
  {
    path->path.rows = baserel->rows;
  }

     
                                                                             
                                                                    
                                                                         
                          
     
  path->path.startup_cost = path->subpath->startup_cost;
  path->path.total_cost = path->subpath->total_cost;

  get_restriction_qual_cost(root, baserel, param_info, &qpqual_cost);

  startup_cost = qpqual_cost.startup;
  cpu_per_tuple = cpu_tuple_cost + qpqual_cost.per_tuple;
  run_cost = cpu_per_tuple * baserel->tuples;

                                                                       
  startup_cost += path->path.pathtarget->cost.startup;
  run_cost += path->path.pathtarget->cost.per_tuple * path->path.rows;

  path->path.startup_cost += startup_cost;
  path->path.total_cost += startup_cost + run_cost;
}

   
                     
                                                                 
   
                                           
                                                                                
   
void
cost_functionscan(Path *path, PlannerInfo *root, RelOptInfo *baserel, ParamPathInfo *param_info)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;
  QualCost qpqual_cost;
  Cost cpu_per_tuple;
  RangeTblEntry *rte;
  QualCost exprcost;

                                                                   
  Assert(baserel->relid > 0);
  rte = planner_rt_fetch(baserel->relid, root);
  Assert(rte->rtekind == RTE_FUNCTION);

                                                   
  if (param_info)
  {
    path->rows = param_info->ppi_rows;
  }
  else
  {
    path->rows = baserel->rows;
  }

     
                                                             
     
                                                                    
                                                                       
                                                                             
                        
     
                                                                       
                                                                     
                                                                            
                           
     
  cost_qual_eval_node(&exprcost, (Node *)rte->functions, root);

  startup_cost += exprcost.startup + exprcost.per_tuple;

                              
  get_restriction_qual_cost(root, baserel, param_info, &qpqual_cost);

  startup_cost += qpqual_cost.startup;
  cpu_per_tuple = cpu_tuple_cost + qpqual_cost.per_tuple;
  run_cost += cpu_per_tuple * baserel->tuples;

                                                                       
  startup_cost += path->pathtarget->cost.startup;
  run_cost += path->pathtarget->cost.per_tuple * path->rows;

  path->startup_cost = startup_cost;
  path->total_cost = startup_cost + run_cost;
}

   
                      
                                                                   
   
                                           
                                                                                
   
void
cost_tablefuncscan(Path *path, PlannerInfo *root, RelOptInfo *baserel, ParamPathInfo *param_info)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;
  QualCost qpqual_cost;
  Cost cpu_per_tuple;
  RangeTblEntry *rte;
  QualCost exprcost;

                                                                   
  Assert(baserel->relid > 0);
  rte = planner_rt_fetch(baserel->relid, root);
  Assert(rte->rtekind == RTE_TABLEFUNC);

                                                   
  if (param_info)
  {
    path->rows = param_info->ppi_rows;
  }
  else
  {
    path->rows = baserel->rows;
  }

     
                                                               
     
                                                                       
                                                                     
                                                                             
                           
     
  cost_qual_eval_node(&exprcost, (Node *)rte->tablefunc, root);

  startup_cost += exprcost.startup + exprcost.per_tuple;

                              
  get_restriction_qual_cost(root, baserel, param_info, &qpqual_cost);

  startup_cost += qpqual_cost.startup;
  cpu_per_tuple = cpu_tuple_cost + qpqual_cost.per_tuple;
  run_cost += cpu_per_tuple * baserel->tuples;

                                                                       
  startup_cost += path->pathtarget->cost.startup;
  run_cost += path->pathtarget->cost.per_tuple * path->rows;

  path->startup_cost = startup_cost;
  path->total_cost = startup_cost + run_cost;
}

   
                   
                                                               
   
                                           
                                                                                
   
void
cost_valuesscan(Path *path, PlannerInfo *root, RelOptInfo *baserel, ParamPathInfo *param_info)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;
  QualCost qpqual_cost;
  Cost cpu_per_tuple;

                                                                      
  Assert(baserel->relid > 0);
  Assert(baserel->rtekind == RTE_VALUES);

                                                   
  if (param_info)
  {
    path->rows = param_info->ppi_rows;
  }
  else
  {
    path->rows = baserel->rows;
  }

     
                                                                          
                                                             
     
  cpu_per_tuple = cpu_operator_cost;

                              
  get_restriction_qual_cost(root, baserel, param_info, &qpqual_cost);

  startup_cost += qpqual_cost.startup;
  cpu_per_tuple += cpu_tuple_cost + qpqual_cost.per_tuple;
  run_cost += cpu_per_tuple * baserel->tuples;

                                                                       
  startup_cost += path->pathtarget->cost.startup;
  run_cost += path->pathtarget->cost.per_tuple * path->rows;

  path->startup_cost = startup_cost;
  path->total_cost = startup_cost + run_cost;
}

   
                
                                                            
   
                                                                    
                                                                      
                                                                      
                                                                         
                                   
   
void
cost_ctescan(Path *path, PlannerInfo *root, RelOptInfo *baserel, ParamPathInfo *param_info)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;
  QualCost qpqual_cost;
  Cost cpu_per_tuple;

                                                              
  Assert(baserel->relid > 0);
  Assert(baserel->rtekind == RTE_CTE);

                                                   
  if (param_info)
  {
    path->rows = param_info->ppi_rows;
  }
  else
  {
    path->rows = baserel->rows;
  }

                                                                     
  cpu_per_tuple = cpu_tuple_cost;

                              
  get_restriction_qual_cost(root, baserel, param_info, &qpqual_cost);

  startup_cost += qpqual_cost.startup;
  cpu_per_tuple += cpu_tuple_cost + qpqual_cost.per_tuple;
  run_cost += cpu_per_tuple * baserel->tuples;

                                                                       
  startup_cost += path->pathtarget->cost.startup;
  run_cost += path->pathtarget->cost.per_tuple * path->rows;

  path->startup_cost = startup_cost;
  path->total_cost = startup_cost + run_cost;
}

   
                            
                                                                     
   
void
cost_namedtuplestorescan(Path *path, PlannerInfo *root, RelOptInfo *baserel, ParamPathInfo *param_info)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;
  QualCost qpqual_cost;
  Cost cpu_per_tuple;

                                                                     
  Assert(baserel->relid > 0);
  Assert(baserel->rtekind == RTE_NAMEDTUPLESTORE);

                                                   
  if (param_info)
  {
    path->rows = param_info->ppi_rows;
  }
  else
  {
    path->rows = baserel->rows;
  }

                                                                     
  cpu_per_tuple = cpu_tuple_cost;

                              
  get_restriction_qual_cost(root, baserel, param_info, &qpqual_cost);

  startup_cost += qpqual_cost.startup;
  cpu_per_tuple += cpu_tuple_cost + qpqual_cost.per_tuple;
  run_cost += cpu_per_tuple * baserel->tuples;

  path->startup_cost = startup_cost;
  path->total_cost = startup_cost + run_cost;
}

   
                   
                                                                         
   
void
cost_resultscan(Path *path, PlannerInfo *root, RelOptInfo *baserel, ParamPathInfo *param_info)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;
  QualCost qpqual_cost;
  Cost cpu_per_tuple;

                                                           
  Assert(baserel->relid > 0);
  Assert(baserel->rtekind == RTE_RESULT);

                                                   
  if (param_info)
  {
    path->rows = param_info->ppi_rows;
  }
  else
  {
    path->rows = baserel->rows;
  }

                                               
  get_restriction_qual_cost(root, baserel, param_info, &qpqual_cost);

  startup_cost += qpqual_cost.startup;
  cpu_per_tuple = cpu_tuple_cost + qpqual_cost.per_tuple;
  run_cost += cpu_per_tuple * baserel->tuples;

  path->startup_cost = startup_cost;
  path->total_cost = startup_cost + run_cost;
}

   
                        
                                                                      
                                         
   
                                                                
   
void
cost_recursive_union(Path *runion, Path *nrterm, Path *rterm)
{
  Cost startup_cost;
  Cost total_cost;
  double total_rows;

                                                                    
  startup_cost = nrterm->startup_cost;
  total_cost = nrterm->total_cost;
  total_rows = nrterm->rows;

     
                                                                      
                                                                             
                                                                            
                                   
     
  total_cost += 10 * rterm->total_cost;
  total_rows += 10 * rterm->rows;

     
                                                                    
                                                                   
                           
     
  total_cost += cpu_tuple_cost * total_rows;

  runion->startup_cost = startup_cost;
  runion->total_cost = total_cost;
  runion->rows = total_rows;
  runion->pathtarget->width = Max(nrterm->pathtarget->width, rterm->pathtarget->width);
}

   
             
                                                                      
                                         
   
                                                                         
                                                                      
                             
   
                                                                         
                                                                        
                                                                      
                                                                          
                                                                               
                                                                   
                                                          
                                        
   
                                                                            
                                                                            
                                                                              
   
                                                                        
                                              
   
                                                                               
                                                                         
                                                                           
                                                                               
   
                                     
                                                             
                                                    
                                               
                                                              
                                                                             
                                                                              
   
                                                                   
                                                                        
                                                                      
                                                                        
                                                                             
                                                    
   
void
cost_sort(Path *path, PlannerInfo *root, List *pathkeys, Cost input_cost, double tuples, int width, Cost comparison_cost, int sort_mem, double limit_tuples)
{
  Cost startup_cost = input_cost;
  Cost run_cost = 0;
  double input_bytes = relation_byte_size(tuples, width);
  double output_bytes;
  double output_tuples;
  long sort_mem_bytes = sort_mem * 1024L;

  if (!enable_sort)
  {
    startup_cost += disable_cost;
  }

  path->rows = tuples;

     
                                                                            
                                                                      
     
  if (tuples < 2.0)
  {
    tuples = 2.0;
  }

                                               
  comparison_cost += 2.0 * cpu_operator_cost;

                                  
  if (limit_tuples > 0 && limit_tuples < tuples)
  {
    output_tuples = limit_tuples;
    output_bytes = relation_byte_size(output_tuples, width);
  }
  else
  {
    output_tuples = tuples;
    output_bytes = input_bytes;
  }

  if (output_bytes > sort_mem_bytes)
  {
       
                                                             
       
    double npages = ceil(input_bytes / BLCKSZ);
    double nruns = input_bytes / sort_mem_bytes;
    double mergeorder = tuplesort_merge_order(sort_mem_bytes);
    double log_runs;
    double npageaccesses;

       
                 
       
                                         
       
    startup_cost += comparison_cost * tuples * LOG2(tuples);

                    

                                            
    if (nruns > mergeorder)
    {
      log_runs = ceil(log(nruns) / log(mergeorder));
    }
    else
    {
      log_runs = 1.0;
    }
    npageaccesses = 2.0 * npages * log_runs;
                                                                 
    startup_cost += npageaccesses * (seq_page_cost * 0.75 + random_page_cost * 0.25);
  }
  else if (tuples > 2 * output_tuples || input_bytes > sort_mem_bytes)
  {
       
                                                                          
                                                                         
                                                                        
                                                        
       
    startup_cost += comparison_cost * tuples * LOG2(2.0 * output_tuples);
  }
  else
  {
                                                           
    startup_cost += comparison_cost * tuples * LOG2(tuples);
  }

     
                                                                             
                                                                          
                                                                          
                                                                         
                                                                           
                                   
     
  run_cost += cpu_operator_cost * tuples;

  path->startup_cost = startup_cost;
  path->total_cost = startup_cost + run_cost;
}

   
                          
                                                                      
                                                                        
                                                                    
   
static Cost
append_nonpartial_cost(List *subpaths, int numpaths, int parallel_workers)
{
  Cost *costarr;
  int arrlen;
  ListCell *l;
  ListCell *cell;
  int i;
  int path_index;
  int min_index;
  int max_index;

  if (numpaths == 0)
  {
    return 0;
  }

     
                                                                     
                        
     
  arrlen = Min(parallel_workers, numpaths);
  costarr = (Cost *)palloc(sizeof(Cost) * arrlen);

                                                                       
  path_index = 0;
  foreach (cell, subpaths)
  {
    Path *subpath = (Path *)lfirst(cell);

    if (path_index == arrlen)
    {
      break;
    }
    costarr[path_index++] = subpath->total_cost;
  }

     
                                                                          
                       
     
  min_index = arrlen - 1;

     
                                                                           
                        
     
  for_each_cell(l, cell)
  {
    Path *subpath = (Path *)lfirst(l);
    int i;

                                             
    if (path_index++ == numpaths)
    {
      break;
    }

    costarr[min_index] += subpath->total_cost;

                                             
    for (min_index = i = 0; i < arrlen; i++)
    {
      if (costarr[i] < costarr[min_index])
      {
        min_index = i;
      }
    }
  }

                                              
  for (max_index = i = 0; i < arrlen; i++)
  {
    if (costarr[i] > costarr[max_index])
    {
      max_index = i;
    }
  }

  return costarr[max_index];
}

   
               
                                                        
   
void
cost_append(AppendPath *apath)
{
  ListCell *l;

  apath->path.startup_cost = 0;
  apath->path.total_cost = 0;
  apath->path.rows = 0;

  if (apath->subpaths == NIL)
  {
    return;
  }

  if (!apath->path.parallel_aware)
  {
    List *pathkeys = apath->path.pathkeys;

    if (pathkeys == NIL)
    {
      Path *subpath = (Path *)linitial(apath->subpaths);

         
                                                                         
                                                        
         
      apath->path.startup_cost = subpath->startup_cost;

                                                                     
      foreach (l, apath->subpaths)
      {
        Path *subpath = (Path *)lfirst(l);

        apath->path.rows += subpath->rows;
        apath->path.total_cost += subpath->total_cost;
      }
    }
    else
    {
         
                                                                       
                                                                     
                                                                     
                                                                      
                                                                        
                                                                      
                                                                       
                                                                     
                                                                    
                                                                    
                                                                       
                                      
         
                                                                       
                                                                        
                           
         
      foreach (l, apath->subpaths)
      {
        Path *subpath = (Path *)lfirst(l);
        Path sort_path;                                    

        if (!pathkeys_contained_in(pathkeys, subpath->pathkeys))
        {
             
                                                                    
                                                                   
                                                                  
                        
             
          cost_sort(&sort_path, NULL,                                  
              pathkeys, subpath->total_cost, subpath->rows, subpath->pathtarget->width, 0.0, work_mem, apath->limit_tuples);
          subpath = &sort_path;
        }

        apath->path.rows += subpath->rows;
        apath->path.startup_cost += subpath->startup_cost;
        apath->path.total_cost += subpath->total_cost;
      }
    }
  }
  else                     
  {
    int i = 0;
    double parallel_divisor = get_parallel_divisor(&apath->path);

                                                              
    Assert(apath->path.pathkeys == NIL);

                                 
    foreach (l, apath->subpaths)
    {
      Path *subpath = (Path *)lfirst(l);

         
                                                                       
                                                                      
                                                                    
         
      if (i == 0)
      {
        apath->path.startup_cost = subpath->startup_cost;
      }
      else if (i < apath->path.parallel_workers)
      {
        apath->path.startup_cost = Min(apath->path.startup_cost, subpath->startup_cost);
      }

         
                                                                       
                                                                     
                                                                        
                                                                   
                                           
         
      if (i < apath->first_partial_path)
      {
        apath->path.rows += subpath->rows / parallel_divisor;
      }
      else
      {
        double subpath_parallel_divisor;

        subpath_parallel_divisor = get_parallel_divisor(subpath);
        apath->path.rows += subpath->rows * (subpath_parallel_divisor / parallel_divisor);
        apath->path.total_cost += subpath->total_cost;
      }

      apath->path.rows = clamp_row_est(apath->path.rows);

      i++;
    }

                                            
    apath->path.total_cost += append_nonpartial_cost(apath->subpaths, apath->first_partial_path, apath->path.parallel_workers);
  }

     
                                                                             
                                     
     
  apath->path.total_cost += cpu_tuple_cost * APPEND_CPU_COST_MULTIPLIER * apath->path.rows;
}

   
                     
                                                            
   
                                                                          
                                                                         
                                                                         
                                                                      
                                         
   
                                                                          
                                                                             
   
                                                                           
                                           
   
                                                                       
   
                                     
                                              
                                                                       
                                                                   
                                                       
   
void
cost_merge_append(Path *path, PlannerInfo *root, List *pathkeys, int n_streams, Cost input_startup_cost, Cost input_total_cost, double tuples)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;
  Cost comparison_cost;
  double N;
  double logN;

     
                     
     
  N = (n_streams < 2) ? 2.0 : (double)n_streams;
  logN = LOG2(N);

                                         
  comparison_cost = 2.0 * cpu_operator_cost;

                          
  startup_cost += comparison_cost * N * logN;

                                       
  run_cost += tuples * comparison_cost * logN;

     
                                                                            
                                           
     
  run_cost += cpu_tuple_cost * APPEND_CPU_COST_MULTIPLIER * tuples;

  path->startup_cost = startup_cost + input_startup_cost;
  path->total_cost = startup_cost + run_cost + input_total_cost;
}

   
                 
                                                                            
                                         
   
                                                                             
                                                                 
   
                                                                        
                                                                         
                                                            
   
void
cost_material(Path *path, Cost input_startup_cost, Cost input_total_cost, double tuples, int width)
{
  Cost startup_cost = input_startup_cost;
  Cost run_cost = input_total_cost - input_startup_cost;
  double nbytes = relation_byte_size(tuples, width);
  long work_mem_bytes = work_mem * 1024L;

  path->rows = tuples;

     
                                                                       
                                                                      
                                                                           
                                                                     
                                                                            
                                                                
                                                                            
                                                                        
                                                                            
                      
     
  run_cost += 2 * cpu_operator_cost * tuples;

     
                                                                             
                                                                          
                                                                       
                                            
     
  if (nbytes > work_mem_bytes)
  {
    double npages = ceil(nbytes / BLCKSZ);

    run_cost += seq_page_cost * npages;
  }

  path->startup_cost = startup_cost;
  path->total_cost = startup_cost + run_cost;
}

   
            
                                                                    
                                     
   
                                                                            
                                                        
   
                                                                             
                                       
   
void
cost_agg(Path *path, PlannerInfo *root, AggStrategy aggstrategy, const AggClauseCosts *aggcosts, int numGroupCols, double numGroups, List *quals, Cost input_startup_cost, Cost input_total_cost, double input_tuples)
{
  double output_tuples;
  Cost startup_cost;
  Cost total_cost;
  AggClauseCosts dummy_aggcosts;

                                                          
  if (aggcosts == NULL)
  {
    Assert(aggstrategy == AGG_HASHED);
    MemSet(&dummy_aggcosts, 0, sizeof(AggClauseCosts));
    aggcosts = &dummy_aggcosts;
  }

     
                                                                          
                                                                             
                                                                             
                                                                     
                                                                             
     
                                                                       
                                                               
     
                                                                            
                                                                       
     
                                                                          
                                                                         
                                                                      
                                                                            
                                                                             
                                                                          
                                                                          
            
     
  if (aggstrategy == AGG_PLAIN)
  {
    startup_cost = input_total_cost;
    startup_cost += aggcosts->transCost.startup;
    startup_cost += aggcosts->transCost.per_tuple * input_tuples;
    startup_cost += aggcosts->finalCost.startup;
    startup_cost += aggcosts->finalCost.per_tuple;
                            
    total_cost = startup_cost + cpu_tuple_cost;
    output_tuples = 1;
  }
  else if (aggstrategy == AGG_SORTED || aggstrategy == AGG_MIXED)
  {
                                                       
    startup_cost = input_startup_cost;
    total_cost = input_total_cost;
    if (aggstrategy == AGG_MIXED && !enable_hashagg)
    {
      startup_cost += disable_cost;
      total_cost += disable_cost;
    }
                                                                     
    total_cost += aggcosts->transCost.startup;
    total_cost += aggcosts->transCost.per_tuple * input_tuples;
    total_cost += (cpu_operator_cost * numGroupCols) * input_tuples;
    total_cost += aggcosts->finalCost.startup;
    total_cost += aggcosts->finalCost.per_tuple * numGroups;
    total_cost += cpu_tuple_cost * numGroups;
    output_tuples = numGroups;
  }
  else
  {
                            
    startup_cost = input_total_cost;
    if (!enable_hashagg)
    {
      startup_cost += disable_cost;
    }
    startup_cost += aggcosts->transCost.startup;
    startup_cost += aggcosts->transCost.per_tuple * input_tuples;
    startup_cost += (cpu_operator_cost * numGroupCols) * input_tuples;
    startup_cost += aggcosts->finalCost.startup;
    total_cost = startup_cost;
    total_cost += aggcosts->finalCost.per_tuple * numGroups;
    total_cost += cpu_tuple_cost * numGroups;
    output_tuples = numGroups;
  }

     
                                                                   
                  
     
  if (quals)
  {
    QualCost qual_cost;

    cost_qual_eval(&qual_cost, quals, root);
    startup_cost += qual_cost.startup;
    total_cost += qual_cost.startup + output_tuples * qual_cost.per_tuple;

    output_tuples = clamp_row_est(output_tuples * clauselist_selectivity(root, quals, 0, JOIN_INNER, NULL));
  }

  path->rows = output_tuples;
  path->startup_cost = startup_cost;
  path->total_cost = total_cost;
}

   
                  
                                                                         
                                     
   
                                             
   
void
cost_windowagg(Path *path, PlannerInfo *root, List *windowFuncs, int numPartCols, int numOrderCols, Cost input_startup_cost, Cost input_total_cost, double input_tuples)
{
  Cost startup_cost;
  Cost total_cost;
  ListCell *lc;

  startup_cost = input_startup_cost;
  total_cost = input_total_cost;

     
                                                                            
                                                                            
                                                                           
                                                                          
                                                                           
                                                                           
                                    
     
  foreach (lc, windowFuncs)
  {
    WindowFunc *wfunc = lfirst_node(WindowFunc, lc);
    Cost wfunccost;
    QualCost argcosts;

    argcosts.startup = argcosts.per_tuple = 0;
    add_function_cost(root, wfunc->winfnoid, (Node *)wfunc, &argcosts);
    startup_cost += argcosts.startup;
    wfunccost = argcosts.per_tuple;

                                                                     
    cost_qual_eval_node(&argcosts, (Node *)wfunc->args, root);
    startup_cost += argcosts.startup;
    wfunccost += argcosts.per_tuple;

       
                                                                           
                                                               
       
    cost_qual_eval_node(&argcosts, (Node *)wfunc->aggfilter, root);
    startup_cost += argcosts.startup;
    wfunccost += argcosts.per_tuple;

    total_cost += wfunccost * input_tuples;
  }

     
                                                                        
                                                                     
               
     
                                                                            
                                                               
     
  total_cost += cpu_operator_cost * (numPartCols + numOrderCols) * input_tuples;
  total_cost += cpu_tuple_cost * input_tuples;

  path->rows = input_tuples;
  path->startup_cost = startup_cost;
  path->total_cost = total_cost;
}

   
              
                                                                     
                                     
   
                                                                          
          
   
void
cost_group(Path *path, PlannerInfo *root, int numGroupCols, double numGroups, List *quals, Cost input_startup_cost, Cost input_total_cost, double input_tuples)
{
  double output_tuples;
  Cost startup_cost;
  Cost total_cost;

  output_tuples = numGroups;
  startup_cost = input_startup_cost;
  total_cost = input_total_cost;

     
                                                                            
                                                     
     
  total_cost += cpu_operator_cost * input_tuples * numGroupCols;

     
                                                                   
                  
     
  if (quals)
  {
    QualCost qual_cost;

    cost_qual_eval(&qual_cost, quals, root);
    startup_cost += qual_cost.startup;
    total_cost += qual_cost.startup + output_tuples * qual_cost.per_tuple;

    output_tuples = clamp_row_est(output_tuples * clauselist_selectivity(root, quals, 0, JOIN_INNER, NULL));
  }

  path->rows = output_tuples;
  path->startup_cost = startup_cost;
  path->total_cost = total_cost;
}

   
                         
                                                               
   
                                                                             
                                                                      
                                                                            
                                  
   
                                                                             
                                                                              
                                                                           
                                                                       
                                                                         
                                                                             
                                               
   
                                                                          
                                                 
                                                  
                                               
                                               
                                                             
   
void
initial_cost_nestloop(PlannerInfo *root, JoinCostWorkspace *workspace, JoinType jointype, Path *outer_path, Path *inner_path, JoinPathExtraData *extra)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;
  double outer_path_rows = outer_path->rows;
  Cost inner_rescan_start_cost;
  Cost inner_rescan_total_cost;
  Cost inner_run_cost;
  Cost inner_rescan_run_cost;

                                                   
  cost_rescan(root, inner_path, &inner_rescan_start_cost, &inner_rescan_total_cost);

                           

     
                                                                         
                                                                         
                                                                     
                     
     
  startup_cost += outer_path->startup_cost + inner_path->startup_cost;
  run_cost += outer_path->total_cost - outer_path->startup_cost;
  if (outer_path_rows > 1)
  {
    run_cost += (outer_path_rows - 1) * inner_rescan_start_cost;
  }

  inner_run_cost = inner_path->total_cost - inner_path->startup_cost;
  inner_rescan_run_cost = inner_rescan_total_cost - inner_rescan_start_cost;

  if (jointype == JOIN_SEMI || jointype == JOIN_ANTI || extra->inner_unique)
  {
       
                                                                         
                                                 
       
                                                                       
                                                           
       

                                                   
    workspace->inner_run_cost = inner_run_cost;
    workspace->inner_rescan_run_cost = inner_rescan_run_cost;
  }
  else
  {
                                                                    
    run_cost += inner_run_cost;
    if (outer_path_rows > 1)
    {
      run_cost += (outer_path_rows - 1) * inner_rescan_run_cost;
    }
  }

                                

                            
  workspace->startup_cost = startup_cost;
  workspace->total_cost = startup_cost + run_cost;
                                                 
  workspace->run_cost = run_cost;
}

   
                       
                                                                         
   
                                                                   
                                                        
                                                             
   
void
final_cost_nestloop(PlannerInfo *root, NestPath *path, JoinCostWorkspace *workspace, JoinPathExtraData *extra)
{
  Path *outer_path = path->outerjoinpath;
  Path *inner_path = path->innerjoinpath;
  double outer_path_rows = outer_path->rows;
  double inner_path_rows = inner_path->rows;
  Cost startup_cost = workspace->startup_cost;
  Cost run_cost = workspace->run_cost;
  Cost cpu_per_tuple;
  QualCost restrict_qual_cost;
  double ntuples;

                                                                        
  if (outer_path_rows <= 0 || isnan(outer_path_rows))
  {
    outer_path_rows = 1;
  }
  if (inner_path_rows <= 0 || isnan(inner_path_rows))
  {
    inner_path_rows = 1;
  }

                                                   
  if (path->path.param_info)
  {
    path->path.rows = path->path.param_info->ppi_rows;
  }
  else
  {
    path->path.rows = path->path.parent->rows;
  }

                                              
  if (path->path.parallel_workers > 0)
  {
    double parallel_divisor = get_parallel_divisor(&path->path);

    path->path.rows = clamp_row_est(path->path.rows / parallel_divisor);
  }

     
                                                                         
                                                                      
                                                       
     
  if (!enable_nestloop)
  {
    startup_cost += disable_cost;
  }

                                                                            

  if (path->jointype == JOIN_SEMI || path->jointype == JOIN_ANTI || extra->inner_unique)
  {
       
                                                                         
                                                 
       
    Cost inner_run_cost = workspace->inner_run_cost;
    Cost inner_rescan_run_cost = workspace->inner_rescan_run_cost;
    double outer_matched_rows;
    double outer_unmatched_rows;
    Selectivity inner_scan_frac;

       
                                                                           
                                                                          
                                                                         
                                                                         
                                                                      
                                                                         
                                              
       
    outer_matched_rows = rint(outer_path_rows * extra->semifactors.outer_match_frac);
    outer_unmatched_rows = outer_path_rows - outer_matched_rows;
    inner_scan_frac = 2.0 / (extra->semifactors.match_count + 1.0);

       
                                                                         
                                                    
       
    ntuples = outer_matched_rows * inner_path_rows * inner_scan_frac;

       
                                                                      
                                                                           
                                                                           
                                                                      
                                                                      
                                                                         
                                                                       
       
    if (has_indexed_join_quals(path))
    {
         
                                                                    
                                                                        
                                                                      
                                                                        
                                                                      
                                                                  
                                                                         
                                                                        
                                                              
                                                                    
                                                                  
                                                    
         
      run_cost += inner_run_cost * inner_scan_frac;
      if (outer_matched_rows > 1)
      {
        run_cost += (outer_matched_rows - 1) * inner_rescan_run_cost * inner_scan_frac;
      }

         
                                                                         
                                                                        
                                                                      
                                                    
         
      run_cost += outer_unmatched_rows * inner_rescan_run_cost / inner_path_rows;

         
                                                                        
                                    
         
    }
    else
    {
         
                                                                         
                                                                      
                                                                       
                                                                       
                                                                      
                                                                   
                                                                        
                                                                      
                                                                     
                                                 
         

                                                                     
      ntuples += outer_unmatched_rows * inner_path_rows;

                                                                         
      run_cost += inner_run_cost;
      if (outer_unmatched_rows >= 1)
      {
        outer_unmatched_rows -= 1;
      }
      else
      {
        outer_matched_rows -= 1;
      }

                                                                         
      if (outer_matched_rows > 0)
      {
        run_cost += outer_matched_rows * inner_rescan_run_cost * inner_scan_frac;
      }

                                                                    
      if (outer_unmatched_rows > 0)
      {
        run_cost += outer_unmatched_rows * inner_rescan_run_cost;
      }
    }
  }
  else
  {
                                                                        

                                                                  
    ntuples = outer_path_rows * inner_path_rows;
  }

                 
  cost_qual_eval(&restrict_qual_cost, path->joinrestrictinfo, root);
  startup_cost += restrict_qual_cost.startup;
  cpu_per_tuple = cpu_tuple_cost + restrict_qual_cost.per_tuple;
  run_cost += cpu_per_tuple * ntuples;

                                                                       
  startup_cost += path->path.pathtarget->cost.startup;
  run_cost += path->path.pathtarget->cost.per_tuple * path->path.rows;

  path->path.startup_cost = startup_cost;
  path->path.total_cost = startup_cost + run_cost;
}

   
                          
                                                           
   
                                                                             
                                                                      
                                                                             
                                  
   
                                                                              
                                                                              
                                                                           
                                                                             
                                                                          
                                               
                                                              
   
                                                                          
                                                  
                                                  
                                                                         
                                               
                                               
                                                               
                                                               
                                                             
   
                                                                      
                                                                         
   
void
initial_cost_mergejoin(PlannerInfo *root, JoinCostWorkspace *workspace, JoinType jointype, List *mergeclauses, Path *outer_path, Path *inner_path, List *outersortkeys, List *innersortkeys, JoinPathExtraData *extra)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;
  double outer_path_rows = outer_path->rows;
  double inner_path_rows = inner_path->rows;
  Cost inner_run_cost;
  double outer_rows, inner_rows, outer_skip_rows, inner_skip_rows;
  Selectivity outerstartsel, outerendsel, innerstartsel, innerendsel;
  Path sort_path;                                    

                                                                        
  if (outer_path_rows <= 0 || isnan(outer_path_rows))
  {
    outer_path_rows = 1;
  }
  if (inner_path_rows <= 0 || isnan(inner_path_rows))
  {
    inner_path_rows = 1;
  }

     
                                                                       
                                                                        
                                                                           
                                                                     
                                                                            
                                                                            
                                                                       
                                                                        
                                               
     
  if (mergeclauses && jointype != JOIN_FULL)
  {
    RestrictInfo *firstclause = (RestrictInfo *)linitial(mergeclauses);
    List *opathkeys;
    List *ipathkeys;
    PathKey *opathkey;
    PathKey *ipathkey;
    MergeScanSelCache *cache;

                                                                    
    opathkeys = outersortkeys ? outersortkeys : outer_path->pathkeys;
    ipathkeys = innersortkeys ? innersortkeys : inner_path->pathkeys;
    Assert(opathkeys);
    Assert(ipathkeys);
    opathkey = (PathKey *)linitial(opathkeys);
    ipathkey = (PathKey *)linitial(ipathkeys);
                         
    if (opathkey->pk_opfamily != ipathkey->pk_opfamily || opathkey->pk_eclass->ec_collation != ipathkey->pk_eclass->ec_collation || opathkey->pk_strategy != ipathkey->pk_strategy || opathkey->pk_nulls_first != ipathkey->pk_nulls_first)
    {
      elog(ERROR, "left and right pathkeys do not match in mergejoin");
    }

                                          
    cache = cached_scansel(root, firstclause, opathkey);

    if (bms_is_subset(firstclause->left_relids, outer_path->parent->relids))
    {
                                        
      outerstartsel = cache->leftstartsel;
      outerendsel = cache->leftendsel;
      innerstartsel = cache->rightstartsel;
      innerendsel = cache->rightendsel;
    }
    else
    {
                                        
      outerstartsel = cache->rightstartsel;
      outerendsel = cache->rightendsel;
      innerstartsel = cache->leftstartsel;
      innerendsel = cache->leftendsel;
    }
    if (jointype == JOIN_LEFT || jointype == JOIN_ANTI)
    {
      outerstartsel = 0.0;
      outerendsel = 1.0;
    }
    else if (jointype == JOIN_RIGHT)
    {
      innerstartsel = 0.0;
      innerendsel = 1.0;
    }
  }
  else
  {
                                                
    outerstartsel = innerstartsel = 0.0;
    outerendsel = innerendsel = 1.0;
  }

     
                                                                   
                                                                           
     
  outer_skip_rows = rint(outer_path_rows * outerstartsel);
  inner_skip_rows = rint(inner_path_rows * innerstartsel);
  outer_rows = clamp_row_est(outer_path_rows * outerendsel);
  inner_rows = clamp_row_est(inner_path_rows * innerendsel);

                                                                   
  Assert(outer_skip_rows <= outer_rows || isnan(outer_skip_rows));
  Assert(inner_skip_rows <= inner_rows || isnan(inner_skip_rows));

     
                                                                         
                                                                             
                                                                        
     
  outerstartsel = outer_skip_rows / outer_path_rows;
  innerstartsel = inner_skip_rows / inner_path_rows;
  outerendsel = outer_rows / outer_path_rows;
  innerendsel = inner_rows / inner_path_rows;

                                                                   
  Assert(outerstartsel <= outerendsel || isnan(outerstartsel));
  Assert(innerstartsel <= innerendsel || isnan(innerstartsel));

                           

  if (outersortkeys)                                
  {
    cost_sort(&sort_path, root, outersortkeys, outer_path->total_cost, outer_path_rows, outer_path->pathtarget->width, 0.0, work_mem, -1.0);
    startup_cost += sort_path.startup_cost;
    startup_cost += (sort_path.total_cost - sort_path.startup_cost) * outerstartsel;
    run_cost += (sort_path.total_cost - sort_path.startup_cost) * (outerendsel - outerstartsel);
  }
  else
  {
    startup_cost += outer_path->startup_cost;
    startup_cost += (outer_path->total_cost - outer_path->startup_cost) * outerstartsel;
    run_cost += (outer_path->total_cost - outer_path->startup_cost) * (outerendsel - outerstartsel);
  }

  if (innersortkeys)                                
  {
    cost_sort(&sort_path, root, innersortkeys, inner_path->total_cost, inner_path_rows, inner_path->pathtarget->width, 0.0, work_mem, -1.0);
    startup_cost += sort_path.startup_cost;
    startup_cost += (sort_path.total_cost - sort_path.startup_cost) * innerstartsel;
    inner_run_cost = (sort_path.total_cost - sort_path.startup_cost) * (innerendsel - innerstartsel);
  }
  else
  {
    startup_cost += inner_path->startup_cost;
    startup_cost += (inner_path->total_cost - inner_path->startup_cost) * innerstartsel;
    inner_run_cost = (inner_path->total_cost - inner_path->startup_cost) * (innerendsel - innerstartsel);
  }

     
                                                                  
                                                                     
                                                                         
                                                            
                                                     
     

                                

                            
  workspace->startup_cost = startup_cost;
  workspace->total_cost = startup_cost + run_cost + inner_run_cost;
                                                  
  workspace->run_cost = run_cost;
  workspace->inner_run_cost = inner_run_cost;
  workspace->outer_rows = outer_rows;
  workspace->inner_rows = inner_rows;
  workspace->outer_skip_rows = outer_skip_rows;
  workspace->inner_skip_rows = inner_skip_rows;
}

   
                        
                                                                     
   
                                                                             
                                                                            
                                                                       
                                                                               
                                                                           
                                                                            
                                                                               
                                                                 
                                                   
   
                                                                            
                                                                            
                                                    
   
                                                                              
                                                                         
                                         
   
                                                                       
                                            
                                                         
                                                             
   
void
final_cost_mergejoin(PlannerInfo *root, MergePath *path, JoinCostWorkspace *workspace, JoinPathExtraData *extra)
{
  Path *outer_path = path->jpath.outerjoinpath;
  Path *inner_path = path->jpath.innerjoinpath;
  double inner_path_rows = inner_path->rows;
  List *mergeclauses = path->path_mergeclauses;
  List *innersortkeys = path->innersortkeys;
  Cost startup_cost = workspace->startup_cost;
  Cost run_cost = workspace->run_cost;
  Cost inner_run_cost = workspace->inner_run_cost;
  double outer_rows = workspace->outer_rows;
  double inner_rows = workspace->inner_rows;
  double outer_skip_rows = workspace->outer_skip_rows;
  double inner_skip_rows = workspace->inner_skip_rows;
  Cost cpu_per_tuple, bare_inner_cost, mat_inner_cost;
  QualCost merge_qual_cost;
  QualCost qp_qual_cost;
  double mergejointuples, rescannedtuples;
  double rescanratio;

                                                                        
  if (inner_path_rows <= 0 || isnan(inner_path_rows))
  {
    inner_path_rows = 1;
  }

                                                   
  if (path->jpath.path.param_info)
  {
    path->jpath.path.rows = path->jpath.path.param_info->ppi_rows;
  }
  else
  {
    path->jpath.path.rows = path->jpath.path.parent->rows;
  }

                                              
  if (path->jpath.path.parallel_workers > 0)
  {
    double parallel_divisor = get_parallel_divisor(&path->jpath.path);

    path->jpath.path.rows = clamp_row_est(path->jpath.path.rows / parallel_divisor);
  }

     
                                                                         
                                                                      
                                                       
     
  if (!enable_mergejoin)
  {
    startup_cost += disable_cost;
  }

     
                                                                            
                 
     
  cost_qual_eval(&merge_qual_cost, mergeclauses, root);
  cost_qual_eval(&qp_qual_cost, path->jpath.joinrestrictinfo, root);
  qp_qual_cost.startup -= merge_qual_cost.startup;
  qp_qual_cost.per_tuple -= merge_qual_cost.per_tuple;

     
                                                                       
                                                                          
                                                                             
                                                                  
     
  if ((path->jpath.jointype == JOIN_SEMI || path->jpath.jointype == JOIN_ANTI || extra->inner_unique) && (list_length(path->jpath.joinrestrictinfo) == list_length(path->path_mergeclauses)))
  {
    path->skip_mark_restore = true;
  }
  else
  {
    path->skip_mark_restore = false;
  }

     
                                                                            
                                                                      
     
  mergejointuples = approx_tuple_count(root, &path->jpath, mergeclauses);

     
                                                                          
                                                                       
                                                                           
     
                                                                        
                                                                        
                                                                            
                                                                          
                                                               
     
                                            
     
                                                                             
                                                                         
              
     
                                                                          
                                                                           
                                                                            
                                                                         
                   
     
                                                                         
                                                                
     
  if (IsA(outer_path, UniquePath) || path->skip_mark_restore)
  {
    rescannedtuples = 0;
  }
  else
  {
    rescannedtuples = mergejointuples - inner_path_rows;
                                                      
    if (rescannedtuples < 0)
    {
      rescannedtuples = 0;
    }
  }

     
                                                                            
                                                                         
                                                                        
     
  rescanratio = 1.0 + (rescannedtuples / inner_rows);

     
                                                                             
                                                                         
                                                                        
                                                                            
                                                                            
                                                                             
                  
     
  bare_inner_cost = inner_run_cost * rescanratio;

     
                                                                          
                                                                       
                                                                        
                                                                        
                                                                           
                                                                           
                                                                        
                                                       
     
                                                                            
                                     
     
  mat_inner_cost = inner_run_cost + cpu_operator_cost * inner_rows * rescanratio;

     
                                                                          
     
  if (path->skip_mark_restore)
  {
    path->materialize_inner = false;
  }

     
                                                                            
                               
     
  else if (enable_material && mat_inner_cost < bare_inner_cost)
  {
    path->materialize_inner = true;
  }

     
                                                                        
                                                                        
                           
     
                                                                             
                                                                         
                                                                          
                                                                          
                                                                  
                              
     
                                                              
                                                                           
                                                            
     
  else if (innersortkeys == NIL && !ExecSupportsMarkRestore(inner_path))
  {
    path->materialize_inner = true;
  }

     
                                                                         
                                                                         
                                                                             
                                                                       
             
     
                                                                       
                                                                             
          
     
  else if (enable_material && innersortkeys != NIL && relation_byte_size(inner_path_rows, inner_path->pathtarget->width) > (work_mem * 1024L))
  {
    path->materialize_inner = true;
  }
  else
  {
    path->materialize_inner = false;
  }

                                                             
  if (path->materialize_inner)
  {
    run_cost += mat_inner_cost;
  }
  else
  {
    run_cost += bare_inner_cost;
  }

                 

     
                                                                             
                                                                            
                                                                           
     
  startup_cost += merge_qual_cost.startup;
  startup_cost += merge_qual_cost.per_tuple * (outer_skip_rows + inner_skip_rows * rescanratio);
  run_cost += merge_qual_cost.per_tuple * ((outer_rows - outer_skip_rows) + (inner_rows - inner_skip_rows) * rescanratio);

     
                                                                      
                                                                       
                                                                             
                                                            
     
                                                                  
                                                                
     
  startup_cost += qp_qual_cost.startup;
  cpu_per_tuple = cpu_tuple_cost + qp_qual_cost.per_tuple;
  run_cost += cpu_per_tuple * mergejointuples;

                                                                       
  startup_cost += path->jpath.path.pathtarget->cost.startup;
  run_cost += path->jpath.path.pathtarget->cost.per_tuple * path->jpath.path.rows;

  path->jpath.path.startup_cost = startup_cost;
  path->jpath.path.total_cost = startup_cost + run_cost;
}

   
                                       
   
static MergeScanSelCache *
cached_scansel(PlannerInfo *root, RestrictInfo *rinfo, PathKey *pathkey)
{
  MergeScanSelCache *cache;
  ListCell *lc;
  Selectivity leftstartsel, leftendsel, rightstartsel, rightendsel;
  MemoryContext oldcontext;

                                       
  foreach (lc, rinfo->scansel_cache)
  {
    cache = (MergeScanSelCache *)lfirst(lc);
    if (cache->opfamily == pathkey->pk_opfamily && cache->collation == pathkey->pk_eclass->ec_collation && cache->strategy == pathkey->pk_strategy && cache->nulls_first == pathkey->pk_nulls_first)
    {
      return cache;
    }
  }

                                
  mergejoinscansel(root, (Node *)rinfo->clause, pathkey->pk_opfamily, pathkey->pk_strategy, pathkey->pk_nulls_first, &leftstartsel, &leftendsel, &rightstartsel, &rightendsel);

                                                         
  oldcontext = MemoryContextSwitchTo(root->planner_cxt);

  cache = (MergeScanSelCache *)palloc(sizeof(MergeScanSelCache));
  cache->opfamily = pathkey->pk_opfamily;
  cache->collation = pathkey->pk_eclass->ec_collation;
  cache->strategy = pathkey->pk_strategy;
  cache->nulls_first = pathkey->pk_nulls_first;
  cache->leftstartsel = leftstartsel;
  cache->leftendsel = leftendsel;
  cache->rightstartsel = rightstartsel;
  cache->rightendsel = rightendsel;

  rinfo->scansel_cache = lappend(rinfo->scansel_cache, cache);

  MemoryContextSwitchTo(oldcontext);

  return cache;
}

   
                         
                                                          
   
                                                                             
                                                                      
                                                                            
                                  
   
                                                                             
                                                                              
                                                                           
                                                                        
                                                          
                                                        
   
                                                                          
                                                 
                                                  
                                                                       
                                               
                                               
                                                             
                                                                          
                                         
   
void
initial_cost_hashjoin(PlannerInfo *root, JoinCostWorkspace *workspace, JoinType jointype, List *hashclauses, Path *outer_path, Path *inner_path, JoinPathExtraData *extra, bool parallel_hash)
{
  Cost startup_cost = 0;
  Cost run_cost = 0;
  double outer_path_rows = outer_path->rows;
  double inner_path_rows = inner_path->rows;
  double inner_path_rows_total = inner_path_rows;
  int num_hashclauses = list_length(hashclauses);
  int numbuckets;
  int numbatches;
  int num_skew_mcvs;
  size_t space_allowed;             

                           
  startup_cost += outer_path->startup_cost;
  run_cost += outer_path->total_cost - outer_path->startup_cost;
  startup_cost += inner_path->total_cost;

     
                                                                          
                                                                          
                                                                     
                                           
     
                                                                             
                                                                      
                                                                             
     
  startup_cost += (cpu_operator_cost * num_hashclauses + cpu_tuple_cost) * inner_path_rows;
  run_cost += cpu_operator_cost * num_hashclauses * outer_path_rows;

     
                                                                  
                                                                         
                                                                            
                                              
     
  if (parallel_hash)
  {
    inner_path_rows_total *= get_parallel_divisor(inner_path);
  }

     
                                                                     
     
                                                                      
                                                                           
                                        
     
                                                                          
                                                               
     
  ExecChooseHashTableSize(inner_path_rows_total, inner_path->pathtarget->width, true,              
      parallel_hash,                                                                                             
      outer_path->parallel_workers, &space_allowed, &numbuckets, &numbatches, &num_skew_mcvs);

     
                                                                         
                                                                           
                                                                            
                                                                             
                  
     
  if (numbatches > 1)
  {
    double outerpages = page_size(outer_path_rows, outer_path->pathtarget->width);
    double innerpages = page_size(inner_path_rows, inner_path->pathtarget->width);

    startup_cost += seq_page_cost * innerpages;
    run_cost += seq_page_cost * (innerpages + 2 * outerpages);
  }

                                

                            
  workspace->startup_cost = startup_cost;
  workspace->total_cost = startup_cost + run_cost;
                                                 
  workspace->run_cost = run_cost;
  workspace->numbuckets = numbuckets;
  workspace->numbatches = numbatches;
  workspace->inner_rows_total = inner_path_rows_total;
}

   
                       
                                                                    
   
                                                                         
   
                                                                       
                
                                                        
                                                             
   
void
final_cost_hashjoin(PlannerInfo *root, HashPath *path, JoinCostWorkspace *workspace, JoinPathExtraData *extra)
{
  Path *outer_path = path->jpath.outerjoinpath;
  Path *inner_path = path->jpath.innerjoinpath;
  double outer_path_rows = outer_path->rows;
  double inner_path_rows = inner_path->rows;
  double inner_path_rows_total = workspace->inner_rows_total;
  List *hashclauses = path->path_hashclauses;
  Cost startup_cost = workspace->startup_cost;
  Cost run_cost = workspace->run_cost;
  int numbuckets = workspace->numbuckets;
  int numbatches = workspace->numbatches;
  Cost cpu_per_tuple;
  QualCost hash_qual_cost;
  QualCost qp_qual_cost;
  double hashjointuples;
  double virtualbuckets;
  Selectivity innerbucketsize;
  Selectivity innermcvfreq;
  ListCell *hcl;

                                                   
  if (path->jpath.path.param_info)
  {
    path->jpath.path.rows = path->jpath.path.param_info->ppi_rows;
  }
  else
  {
    path->jpath.path.rows = path->jpath.path.parent->rows;
  }

                                              
  if (path->jpath.path.parallel_workers > 0)
  {
    double parallel_divisor = get_parallel_divisor(&path->jpath.path);

    path->jpath.path.rows = clamp_row_est(path->jpath.path.rows / parallel_divisor);
  }

     
                                                                         
                                                                      
                                                       
     
  if (!enable_hashjoin)
  {
    startup_cost += disable_cost;
  }

                                                 
  path->num_batches = numbatches;

                                                                       
  path->inner_rows_total = inner_path_rows_total;

                                                                     
  virtualbuckets = (double)numbuckets * (double)numbatches;

     
                                                                             
                                                                       
                                                              
     
                                                                           
                                                                             
                                                                            
                             
     
  if (IsA(inner_path, UniquePath))
  {
    innerbucketsize = 1.0 / virtualbuckets;
    innermcvfreq = 0.0;
  }
  else
  {
    innerbucketsize = 1.0;
    innermcvfreq = 1.0;
    foreach (hcl, hashclauses)
    {
      RestrictInfo *restrictinfo = lfirst_node(RestrictInfo, hcl);
      Selectivity thisbucketsize;
      Selectivity thismcvfreq;

         
                                                                       
                            
         
                                                                    
                                                                        
                                                                        
         
      if (bms_is_subset(restrictinfo->right_relids, inner_path->parent->relids))
      {
                                     
        thisbucketsize = restrictinfo->right_bucketsize;
        if (thisbucketsize < 0)
        {
                              
          estimate_hash_bucket_stats(root, get_rightop(restrictinfo->clause), virtualbuckets, &restrictinfo->right_mcvfreq, &restrictinfo->right_bucketsize);
          thisbucketsize = restrictinfo->right_bucketsize;
        }
        thismcvfreq = restrictinfo->right_mcvfreq;
      }
      else
      {
        Assert(bms_is_subset(restrictinfo->left_relids, inner_path->parent->relids));
                                    
        thisbucketsize = restrictinfo->left_bucketsize;
        if (thisbucketsize < 0)
        {
                              
          estimate_hash_bucket_stats(root, get_leftop(restrictinfo->clause), virtualbuckets, &restrictinfo->left_mcvfreq, &restrictinfo->left_bucketsize);
          thisbucketsize = restrictinfo->left_bucketsize;
        }
        thismcvfreq = restrictinfo->left_mcvfreq;
      }

      if (innerbucketsize > thisbucketsize)
      {
        innerbucketsize = thisbucketsize;
      }
      if (innermcvfreq > thismcvfreq)
      {
        innermcvfreq = thismcvfreq;
      }
    }
  }

     
                                                                         
                                                                        
                                                                             
                                                                         
                                                                           
                         
     
  if (relation_byte_size(clamp_row_est(inner_path_rows * innermcvfreq), inner_path->pathtarget->width) > (work_mem * 1024L))
  {
    startup_cost += disable_cost;
  }

     
                                                                           
                 
     
  cost_qual_eval(&hash_qual_cost, hashclauses, root);
  cost_qual_eval(&qp_qual_cost, path->jpath.joinrestrictinfo, root);
  qp_qual_cost.startup -= hash_qual_cost.startup;
  qp_qual_cost.per_tuple -= hash_qual_cost.per_tuple;

                 

  if (path->jpath.jointype == JOIN_SEMI || path->jpath.jointype == JOIN_ANTI || extra->inner_unique)
  {
    double outer_matched_rows;
    Selectivity inner_scan_frac;

       
                                                                         
                                                 
       
                                                                           
                                                                     
                                                                         
                                                                           
                                                                          
                                                                         
                                                 
       
    outer_matched_rows = rint(outer_path_rows * extra->semifactors.outer_match_frac);
    inner_scan_frac = 2.0 / (extra->semifactors.match_count + 1.0);

    startup_cost += hash_qual_cost.startup;
    run_cost += hash_qual_cost.per_tuple * outer_matched_rows * clamp_row_est(inner_path_rows * innerbucketsize * inner_scan_frac) * 0.5;

       
                                                                           
                                                                        
                                                                         
                                                                       
                                                                        
                                                                          
                                                                          
                                                                        
                                                                         
                                                                   
                         
       
    run_cost += hash_qual_cost.per_tuple * (outer_path_rows - outer_matched_rows) * clamp_row_est(inner_path_rows / virtualbuckets) * 0.05;

                                                       
    if (path->jpath.jointype == JOIN_ANTI)
    {
      hashjointuples = outer_path_rows - outer_matched_rows;
    }
    else
    {
      hashjointuples = outer_matched_rows;
    }
  }
  else
  {
       
                                                                     
                                                                         
                                                                          
                                                                   
                                                                      
                                                                      
                                                                       
                       
       
    startup_cost += hash_qual_cost.startup;
    run_cost += hash_qual_cost.per_tuple * outer_path_rows * clamp_row_est(inner_path_rows * innerbucketsize) * 0.5;

       
                                                          
                                                                     
                             
       
    hashjointuples = approx_tuple_count(root, &path->jpath, hashclauses);
  }

     
                                                                     
                                                                       
                                                                             
                                                            
     
  startup_cost += qp_qual_cost.startup;
  cpu_per_tuple = cpu_tuple_cost + qp_qual_cost.per_tuple;
  run_cost += cpu_per_tuple * hashjointuples;

                                                                       
  startup_cost += path->jpath.path.pathtarget->cost.startup;
  run_cost += path->jpath.path.pathtarget->cost.per_tuple * path->jpath.path.rows;

  path->jpath.path.startup_cost = startup_cost;
  path->jpath.path.total_cost = startup_cost + run_cost;
}

   
                
                                                  
   
                                                                               
                                                               
   
void
cost_subplan(PlannerInfo *root, SubPlan *subplan, Plan *plan)
{
  QualCost sp_cost;

                                                   
  cost_qual_eval(&sp_cost, make_ands_implicit((Expr *)subplan->testexpr), root);

  if (subplan->useHashTable)
  {
       
                                                                       
                                                                       
                                                                          
            
       
    sp_cost.startup += plan->total_cost + cpu_operator_cost * plan->plan_rows;

       
                                                                       
                                                                        
                                                                           
                                                                        
                                                                          
                                                                
       
  }
  else
  {
       
                                                                  
                                                                       
                                                                      
                                                          
                         
       
    Cost plan_run_cost = plan->total_cost - plan->startup_cost;

    if (subplan->subLinkType == EXISTS_SUBLINK)
    {
                                                                     
      sp_cost.per_tuple += plan_run_cost / clamp_row_est(plan->plan_rows);
    }
    else if (subplan->subLinkType == ALL_SUBLINK || subplan->subLinkType == ANY_SUBLINK)
    {
                                            
      sp_cost.per_tuple += 0.50 * plan_run_cost;
                                                            
      sp_cost.per_tuple += 0.50 * plan->plan_rows * cpu_operator_cost;
    }
    else
    {
                                     
      sp_cost.per_tuple += plan_run_cost;
    }

       
                                                                  
                                                                        
                                                                        
                                                                       
                   
       
    if (subplan->parParam == NIL && ExecMaterializesOutput(nodeTag(plan)))
    {
      sp_cost.startup += plan->startup_cost;
    }
    else
    {
      sp_cost.per_tuple += plan->startup_cost;
    }
  }

  subplan->startup_cost = sp_cost.startup;
  subplan->per_call_cost = sp_cost.per_tuple;
}

   
               
                                                                     
                                                                    
                                                                      
                                                                      
                                                                      
                                                          
   
                                                                            
                                                                          
                                                                         
                                   
   
static void
cost_rescan(PlannerInfo *root, Path *path, Cost *rescan_startup_cost,                        
    Cost *rescan_total_cost)
{
  switch (path->pathtype)
  {
  case T_FunctionScan:

       
                                                                     
                                                                       
                                                                    
                                                                    
                                
       
    *rescan_startup_cost = 0;
    *rescan_total_cost = path->total_cost - path->startup_cost;
    break;
  case T_HashJoin:

       
                                                                      
                              
       
    if (((HashPath *)path)->num_batches == 1)
    {
                                                                   
      *rescan_startup_cost = 0;
      *rescan_total_cost = path->total_cost - path->startup_cost;
    }
    else
    {
                                           
      *rescan_startup_cost = path->startup_cost;
      *rescan_total_cost = path->total_cost;
    }
    break;
  case T_CteScan:
  case T_WorkTableScan:
  {
       
                                                            
                                                                   
                                                                   
                         
       
    Cost run_cost = cpu_tuple_cost * path->rows;
    double nbytes = relation_byte_size(path->rows, path->pathtarget->width);
    long work_mem_bytes = work_mem * 1024L;

    if (nbytes > work_mem_bytes)
    {
                                                      
      double npages = ceil(nbytes / BLCKSZ);

      run_cost += seq_page_cost * npages;
    }
    *rescan_startup_cost = 0;
    *rescan_total_cost = run_cost;
  }
  break;
  case T_Material:
  case T_Sort:
  {
       
                                                                   
                                                                
                                                                   
                                                                   
                                                                  
                                            
       
    Cost run_cost = cpu_operator_cost * path->rows;
    double nbytes = relation_byte_size(path->rows, path->pathtarget->width);
    long work_mem_bytes = work_mem * 1024L;

    if (nbytes > work_mem_bytes)
    {
                                                      
      double npages = ceil(nbytes / BLCKSZ);

      run_cost += seq_page_cost * npages;
    }
    *rescan_startup_cost = 0;
    *rescan_total_cost = run_cost;
  }
  break;
  default:
    *rescan_startup_cost = path->startup_cost;
    *rescan_total_cost = path->total_cost;
    break;
  }
}

   
                  
                                                         
                                                                
                                                                  
                                                       
                                                             
                                    
   
void
cost_qual_eval(QualCost *cost, List *quals, PlannerInfo *root)
{
  cost_qual_eval_context context;
  ListCell *l;

  context.root = root;
  context.total.startup = 0;
  context.total.per_tuple = 0;

                                                                         

  foreach (l, quals)
  {
    Node *qual = (Node *)lfirst(l);

    cost_qual_eval_walker(qual, &context);
  }

  *cost = context.total;
}

   
                       
                                                       
   
void
cost_qual_eval_node(QualCost *cost, Node *qual, PlannerInfo *root)
{
  cost_qual_eval_context context;

  context.root = root;
  context.total.startup = 0;
  context.total.per_tuple = 0;

  cost_qual_eval_walker(qual, &context);

  *cost = context.total;
}

static bool
cost_qual_eval_walker(Node *node, cost_qual_eval_context *context)
{
  if (node == NULL)
  {
    return false;
  }

     
                                                                     
                                                                             
                                                                          
                                                
     
  if (IsA(node, RestrictInfo))
  {
    RestrictInfo *rinfo = (RestrictInfo *)node;

    if (rinfo->eval_cost.startup < 0)
    {
      cost_qual_eval_context locContext;

      locContext.root = context->root;
      locContext.total.startup = 0;
      locContext.total.per_tuple = 0;

         
                                                                      
                                                            
         
      if (rinfo->orclause)
      {
        cost_qual_eval_walker((Node *)rinfo->orclause, &locContext);
      }
      else
      {
        cost_qual_eval_walker((Node *)rinfo->clause, &locContext);
      }

         
                                                                         
                                                           
         
      if (rinfo->pseudoconstant)
      {
                                                
        locContext.total.startup += locContext.total.per_tuple;
        locContext.total.per_tuple = 0;
      }
      rinfo->eval_cost = locContext.total;
    }
    context->total.startup += rinfo->eval_cost.startup;
    context->total.per_tuple += rinfo->eval_cost.per_tuple;
                                      
    return false;
  }

     
                                                                         
                                                                             
                                 
     
                                                                          
                                                                  
     
                                                                   
                                                                        
                                                                           
                                                                          
                                                                           
                                                          
     
                                                                            
                                                                            
                                                                          
                                                                           
                                                                            
                                                    
     
  if (IsA(node, FuncExpr))
  {
    add_function_cost(context->root, ((FuncExpr *)node)->funcid, node, &context->total);
  }
  else if (IsA(node, OpExpr) || IsA(node, DistinctExpr) || IsA(node, NullIfExpr))
  {
                                                             
    set_opfuncid((OpExpr *)node);
    add_function_cost(context->root, ((OpExpr *)node)->opfuncid, node, &context->total);
  }
  else if (IsA(node, ScalarArrayOpExpr))
  {
       
                                                                       
                                                       
       
    ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *)node;
    Node *arraynode = (Node *)lsecond(saop->args);
    QualCost sacosts;

    set_sa_opfuncid(saop);
    sacosts.startup = sacosts.per_tuple = 0;
    add_function_cost(context->root, saop->opfuncid, NULL, &sacosts);
    context->total.startup += sacosts.startup;
    context->total.per_tuple += sacosts.per_tuple * estimate_array_length(arraynode) * 0.5;
  }
  else if (IsA(node, Aggref) || IsA(node, WindowFunc))
  {
       
                                                                          
                                                                         
                                                                      
                                                                          
                                                                           
                                                                           
                  
       
    return false;                                  
  }
  else if (IsA(node, GroupingFunc))
  {
                                     
    context->total.per_tuple += cpu_operator_cost;
    return false;                                  
  }
  else if (IsA(node, CoerceViaIO))
  {
    CoerceViaIO *iocoerce = (CoerceViaIO *)node;
    Oid iofunc;
    Oid typioparam;
    bool typisvarlena;

                                                
    getTypeInputInfo(iocoerce->resulttype, &iofunc, &typioparam);
    add_function_cost(context->root, iofunc, NULL, &context->total);
                                                
    getTypeOutputInfo(exprType((Node *)iocoerce->arg), &iofunc, &typisvarlena);
    add_function_cost(context->root, iofunc, NULL, &context->total);
  }
  else if (IsA(node, ArrayCoerceExpr))
  {
    ArrayCoerceExpr *acoerce = (ArrayCoerceExpr *)node;
    QualCost perelemcost;

    cost_qual_eval_node(&perelemcost, (Node *)acoerce->elemexpr, context->root);
    context->total.startup += perelemcost.startup;
    if (perelemcost.per_tuple > 0)
    {
      context->total.per_tuple += perelemcost.per_tuple * estimate_array_length((Node *)acoerce->arg);
    }
  }
  else if (IsA(node, RowCompareExpr))
  {
                                                             
    RowCompareExpr *rcexpr = (RowCompareExpr *)node;
    ListCell *lc;

    foreach (lc, rcexpr->opnos)
    {
      Oid opid = lfirst_oid(lc);

      add_function_cost(context->root, get_opcode(opid), NULL, &context->total);
    }
  }
  else if (IsA(node, MinMaxExpr) || IsA(node, SQLValueFunction) || IsA(node, XmlExpr) || IsA(node, CoerceToDomain) || IsA(node, NextValueExpr))
  {
                                          
    context->total.per_tuple += cpu_operator_cost;
  }
  else if (IsA(node, CurrentOfExpr))
  {
                                                                        
    context->total.startup += disable_cost;
  }
  else if (IsA(node, SubLink))
  {
                                                                      
    elog(ERROR, "cannot handle unplanned sub-select");
  }
  else if (IsA(node, SubPlan))
  {
       
                                                                    
                                                                           
                                                                        
                                     
       
    SubPlan *subplan = (SubPlan *)node;

    context->total.startup += subplan->startup_cost;
    context->total.per_tuple += subplan->per_call_cost;

       
                                                                          
                                                            
       
    return false;
  }
  else if (IsA(node, AlternativeSubPlan))
  {
       
                                                                           
                                                                     
                                                                           
             
       
    AlternativeSubPlan *asplan = (AlternativeSubPlan *)node;

    return cost_qual_eval_walker((Node *)linitial(asplan->subplans), context);
  }
  else if (IsA(node, PlaceHolderVar))
  {
       
                                                                           
                                                                        
                                                                         
                                                                      
                                                                           
                                                                    
                                                                     
               
       
    return false;
  }

                             
  return expression_tree_walker(node, cost_qual_eval_walker, (void *)context);
}

   
                             
                                                                         
                                                                
                                             
   
                                                                            
                                                                              
                                                                          
                                                                        
                                 
   
static void
get_restriction_qual_cost(PlannerInfo *root, RelOptInfo *baserel, ParamPathInfo *param_info, QualCost *qpqual_cost)
{
  if (param_info)
  {
                                              
    cost_qual_eval(qpqual_cost, param_info->ppi_clauses, root);

    qpqual_cost->startup += baserel->baserestrictcost.startup;
    qpqual_cost->per_tuple += baserel->baserestrictcost.per_tuple;
  }
  else
  {
    *qpqual_cost = baserel->baserestrictcost;
  }
}

   
                                  
                                                                             
                              
   
                                                                         
                                                                    
                                                                 
                                                                           
                                                                       
                                                                            
                                                                            
                                                   
   
                     
                                              
                                                
                                                
                                                                        
                                                 
                            
                      
                                                                     
   
void
compute_semi_anti_join_factors(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outerrel, RelOptInfo *innerrel, JoinType jointype, SpecialJoinInfo *sjinfo, List *restrictlist, SemiAntiJoinFactors *semifactors)
{
  Selectivity jselec;
  Selectivity nselec;
  Selectivity avgmatch;
  SpecialJoinInfo norm_sjinfo;
  List *joinquals;
  ListCell *l;

     
                                                                           
                                                                    
                                                                           
                                                                             
                                                                             
     
  if (IS_OUTER_JOIN(jointype))
  {
    joinquals = NIL;
    foreach (l, restrictlist)
    {
      RestrictInfo *rinfo = lfirst_node(RestrictInfo, l);

      if (!RINFO_IS_PUSHED_DOWN(rinfo, joinrel->relids))
      {
        joinquals = lappend(joinquals, rinfo);
      }
    }
  }
  else
  {
    joinquals = restrictlist;
  }

     
                                                                     
     
  jselec = clauselist_selectivity(root, joinquals, 0, (jointype == JOIN_ANTI) ? JOIN_ANTI : JOIN_SEMI, sjinfo);

     
                                                                     
     
  norm_sjinfo.type = T_SpecialJoinInfo;
  norm_sjinfo.min_lefthand = outerrel->relids;
  norm_sjinfo.min_righthand = innerrel->relids;
  norm_sjinfo.syn_lefthand = outerrel->relids;
  norm_sjinfo.syn_righthand = innerrel->relids;
  norm_sjinfo.jointype = JOIN_INNER;
                                                                 
  norm_sjinfo.lhs_strict = false;
  norm_sjinfo.delay_upper_joins = false;
  norm_sjinfo.semi_can_btree = false;
  norm_sjinfo.semi_can_hash = false;
  norm_sjinfo.semi_operators = NIL;
  norm_sjinfo.semi_rhs_exprs = NIL;

  nselec = clauselist_selectivity(root, joinquals, 0, JOIN_INNER, &norm_sjinfo);

                                        
  if (IS_OUTER_JOIN(jointype))
  {
    list_free(joinquals);
  }

     
                                                                           
                                                                             
                                                                          
                                                                             
                                   
     
                                                                        
                                                                          
                                                                           
                               
     
  if (jselec > 0)                                  
  {
    avgmatch = nselec * innerrel->rows / jselec;
                             
    avgmatch = Max(1.0, avgmatch);
  }
  else
  {
    avgmatch = 1.0;
  }

  semifactors->outer_match_frac = jselec;
  semifactors->match_count = avgmatch;
}

   
                          
                                                                    
                        
   
                                                                           
                                                                               
                                                                              
              
   
static bool
has_indexed_join_quals(NestPath *joinpath)
{
  Relids joinrelids = joinpath->path.parent->relids;
  Path *innerpath = joinpath->innerjoinpath;
  List *indexclauses;
  bool found_one;
  ListCell *lc;

                                                          
  if (joinpath->joinrestrictinfo != NIL)
  {
    return false;
  }
                                                        
  if (innerpath->param_info == NULL)
  {
    return false;
  }

                                                     
  switch (innerpath->pathtype)
  {
  case T_IndexScan:
  case T_IndexOnlyScan:
    indexclauses = ((IndexPath *)innerpath)->indexclauses;
    break;
  case T_BitmapHeapScan:
  {
                                                            
    Path *bmqual = ((BitmapHeapPath *)innerpath)->bitmapqual;

    if (IsA(bmqual, IndexPath))
    {
      indexclauses = ((IndexPath *)bmqual)->indexclauses;
    }
    else
    {
      return false;
    }
    break;
  }
  default:

       
                                                                       
                                                                      
                      
       
    return false;
  }

     
                                                                          
                                                                          
                                                                             
                                                                    
     
  found_one = false;
  foreach (lc, innerpath->param_info->ppi_clauses)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

    if (join_clause_is_movable_into(rinfo, innerpath->parent->relids, joinrelids))
    {
      if (!is_redundant_with_indexclauses(rinfo, indexclauses))
      {
        return false;
      }
      found_one = true;
    }
  }
  return found_one;
}

   
                      
                                                                  
                              
   
                                                                            
                                                           
   
                                                                         
                                                                        
                                                                       
              
   
                                                                         
                                                                       
                                                                          
                                                                            
                                                                             
                                                              
   
                                                                      
                                                                      
                                            
   
static double
approx_tuple_count(PlannerInfo *root, JoinPath *path, List *quals)
{
  double tuples;
  double outer_tuples = path->outerjoinpath->rows;
  double inner_tuples = path->innerjoinpath->rows;
  SpecialJoinInfo sjinfo;
  Selectivity selec = 1.0;
  ListCell *l;

     
                                                         
     
  sjinfo.type = T_SpecialJoinInfo;
  sjinfo.min_lefthand = path->outerjoinpath->parent->relids;
  sjinfo.min_righthand = path->innerjoinpath->parent->relids;
  sjinfo.syn_lefthand = path->outerjoinpath->parent->relids;
  sjinfo.syn_righthand = path->innerjoinpath->parent->relids;
  sjinfo.jointype = JOIN_INNER;
                                                                 
  sjinfo.lhs_strict = false;
  sjinfo.delay_upper_joins = false;
  sjinfo.semi_can_btree = false;
  sjinfo.semi_can_hash = false;
  sjinfo.semi_operators = NIL;
  sjinfo.semi_rhs_exprs = NIL;

                                       
  foreach (l, quals)
  {
    Node *qual = (Node *)lfirst(l);

                                                                       
    selec *= clause_selectivity(root, qual, 0, JOIN_INNER, &sjinfo);
  }

                                            
  tuples = selec * outer_tuples * inner_tuples;

  return clamp_row_est(tuples);
}

   
                              
                                                        
   
                                                                         
                                         
   
                                                
                                                               
                            
                                                             
                                                                            
   
void
set_baserel_size_estimates(PlannerInfo *root, RelOptInfo *rel)
{
  double nrows;

                                                
  Assert(rel->relid > 0);

  nrows = rel->tuples * clauselist_selectivity(root, rel->baserestrictinfo, 0, JOIN_INNER, NULL);

  rel->rows = clamp_row_est(nrows);

  cost_qual_eval(&rel->baserestrictcost, rel->baserestrictinfo, root);

  set_rel_width(root, rel);
}

   
                                  
                                                                      
   
                                                                 
   
                                                              
   
double
get_parameterized_baserel_size(PlannerInfo *root, RelOptInfo *rel, List *param_clauses)
{
  List *allclauses;
  double nrows;

     
                                                                             
                                                                            
                                                                           
                                                     
     
  allclauses = list_concat(list_copy(param_clauses), rel->baserestrictinfo);
  nrows = rel->tuples * clauselist_selectivity(root, allclauses, rel->relid,                    
                            JOIN_INNER, NULL);
  nrows = clamp_row_est(nrows);
                                                                       
  if (nrows > rel->rows)
  {
    nrows = rel->rows;
  }
  return nrows;
}

   
                              
                                                        
   
                                                                  
                                                                      
                
   
                                                                        
                                                                           
                                                                             
                                                                         
                                                                            
                                                                     
                                                                            
                                                                        
                                                                            
             
   
                                                                            
                                                                        
   
void
set_joinrel_size_estimates(PlannerInfo *root, RelOptInfo *rel, RelOptInfo *outer_rel, RelOptInfo *inner_rel, SpecialJoinInfo *sjinfo, List *restrictlist)
{
  rel->rows = calc_joinrel_size_estimate(root, rel, outer_rel, inner_rel, outer_rel->rows, inner_rel->rows, sjinfo, restrictlist);
}

   
                                  
                                                                      
   
                                             
                                                                           
                                        
                                                          
                                                                            
                                                                               
                                                                        
                 
   
                                                              
   
double
get_parameterized_joinrel_size(PlannerInfo *root, RelOptInfo *rel, Path *outer_path, Path *inner_path, SpecialJoinInfo *sjinfo, List *restrict_clauses)
{
  double nrows;

     
                                                                           
                                                                             
                                 
     
                                                                            
                                                                           
                                                           
     
  nrows = calc_joinrel_size_estimate(root, rel, outer_path->parent, inner_path->parent, outer_path->rows, inner_path->rows, sjinfo, restrict_clauses);
                                                                       
  if (nrows > rel->rows)
  {
    nrows = rel->rows;
  }
  return nrows;
}

   
                              
                                                 
                                    
   
                                                                          
                                                                            
                                                                          
   
static double
calc_joinrel_size_estimate(PlannerInfo *root, RelOptInfo *joinrel, RelOptInfo *outer_rel, RelOptInfo *inner_rel, double outer_rows, double inner_rows, SpecialJoinInfo *sjinfo, List *restrictlist_in)
{
                                                                         
  List *restrictlist = restrictlist_in;
  JoinType jointype = sjinfo->jointype;
  Selectivity fkselec;
  Selectivity jselec;
  Selectivity pselec;
  double nrows;

     
                                                                        
                                                                            
                                                                             
                                  
     
                                                                          
                                                                        
                                                                         
                                                                        
                                                                           
                                                                            
                                                                     
                                                    
     
  fkselec = get_foreign_key_join_selectivity(root, outer_rel->relids, inner_rel->relids, sjinfo, &restrictlist);

     
                                                                             
                                                                         
                                                                    
     
  if (IS_OUTER_JOIN(jointype))
  {
    List *joinquals = NIL;
    List *pushedquals = NIL;
    ListCell *l;

                                                               
    foreach (l, restrictlist)
    {
      RestrictInfo *rinfo = lfirst_node(RestrictInfo, l);

      if (RINFO_IS_PUSHED_DOWN(rinfo, joinrel->relids))
      {
        pushedquals = lappend(pushedquals, rinfo);
      }
      else
      {
        joinquals = lappend(joinquals, rinfo);
      }
    }

                                        
    jselec = clauselist_selectivity(root, joinquals, 0, jointype, sjinfo);
    pselec = clauselist_selectivity(root, pushedquals, 0, jointype, sjinfo);

                                          
    list_free(joinquals);
    list_free(pushedquals);
  }
  else
  {
    jselec = clauselist_selectivity(root, restrictlist, 0, jointype, sjinfo);
    pselec = 0.0;                                    
  }

     
                                                                      
     
                                                                         
                                                                            
                                                                   
                                                                  
                                
     
                                                                             
                                                                         
     
  switch (jointype)
  {
  case JOIN_INNER:
    nrows = outer_rows * inner_rows * fkselec * jselec;
                         
    break;
  case JOIN_LEFT:
    nrows = outer_rows * inner_rows * fkselec * jselec;
    if (nrows < outer_rows)
    {
      nrows = outer_rows;
    }
    nrows *= pselec;
    break;
  case JOIN_FULL:
    nrows = outer_rows * inner_rows * fkselec * jselec;
    if (nrows < outer_rows)
    {
      nrows = outer_rows;
    }
    if (nrows < inner_rows)
    {
      nrows = inner_rows;
    }
    nrows *= pselec;
    break;
  case JOIN_SEMI:
    nrows = outer_rows * fkselec * jselec;
                         
    break;
  case JOIN_ANTI:
    nrows = outer_rows * (1.0 - fkselec * jselec);
    nrows *= pselec;
    break;
  default:
                                        
    elog(ERROR, "unrecognized join type: %d", (int)jointype);
    nrows = 0;                          
    break;
  }

  return clamp_row_est(nrows);
}

   
                                    
                                                               
   
                                                                                
                                                                           
                                   
   
                                                                            
                                                                              
                                                                              
                                                                             
                                                                              
            
   
static Selectivity
get_foreign_key_join_selectivity(PlannerInfo *root, Relids outer_relids, Relids inner_relids, SpecialJoinInfo *sjinfo, List **restrictlist)
{
  Selectivity fkselec = 1.0;
  JoinType jointype = sjinfo->jointype;
  List *worklist = *restrictlist;
  ListCell *lc;

                                                                    
  foreach (lc, root->fkey_list)
  {
    ForeignKeyOptInfo *fkinfo = (ForeignKeyOptInfo *)lfirst(lc);
    bool ref_is_outer;
    List *removedlist;
    ListCell *cell;
    ListCell *prev;
    ListCell *next;

       
                                                                           
                                                 
       
    if (bms_is_member(fkinfo->con_relid, outer_relids) && bms_is_member(fkinfo->ref_relid, inner_relids))
    {
      ref_is_outer = false;
    }
    else if (bms_is_member(fkinfo->ref_relid, outer_relids) && bms_is_member(fkinfo->con_relid, inner_relids))
    {
      ref_is_outer = true;
    }
    else
    {
      continue;
    }

       
                                                                       
                                                                         
                                                                          
                                                                          
                                                                      
                                                                       
                                                                           
                                                                          
                                                                           
                                                              
       
    if ((jointype == JOIN_SEMI || jointype == JOIN_ANTI) && (ref_is_outer || bms_membership(inner_relids) != BMS_SINGLETON))
    {
      continue;
    }

       
                                                                          
                                                                          
                                                                           
                           
       
    if (worklist == *restrictlist)
    {
      worklist = list_copy(worklist);
    }

    removedlist = NIL;
    prev = NULL;
    for (cell = list_head(worklist); cell; cell = next)
    {
      RestrictInfo *rinfo = (RestrictInfo *)lfirst(cell);
      bool remove_it = false;
      int i;

      next = lnext(cell);
                                                               
      for (i = 0; i < fkinfo->nkeys; i++)
      {
        if (rinfo->parent_ec)
        {
             
                                                                     
                                                             
                                                                     
                                                                 
                                                                
                                                                     
             
                                                                  
                                                                    
                                                                 
                                                                   
             
          if (fkinfo->eclass[i] == rinfo->parent_ec)
          {
            remove_it = true;
            break;
          }
        }
        else
        {
             
                                                                     
                               
             
          if (list_member_ptr(fkinfo->rinfos[i], rinfo))
          {
            remove_it = true;
            break;
          }
        }
      }
      if (remove_it)
      {
        worklist = list_delete_cell(worklist, cell, prev);
        removedlist = lappend(removedlist, rinfo);
      }
      else
      {
        prev = cell;
      }
    }

       
                                                                      
                                                                      
                                                                          
                                      
       
                                                                        
                                                                          
                                                                        
                                                                          
                                                                           
                                                                       
                                                                        
                                                                         
                                                                      
                                                                         
                                                                          
                                                                          
       
    if (list_length(removedlist) != (fkinfo->nmatched_ec + fkinfo->nmatched_ri))
    {
      worklist = list_concat(worklist, removedlist);
      continue;
    }

       
                                                                    
                                                                         
                             
       
                                                                       
                                                                           
                                                                       
                                                                         
                                                                         
                                                                          
                                                         
       
                                                                         
                                                                      
                                                                     
                                                                        
                                                                          
                                                                      
                                                                      
                                                                           
                                                                 
       
    if (jointype == JOIN_SEMI || jointype == JOIN_ANTI)
    {
         
                                                                     
                                                                       
                                                                      
                                                                         
                                                                   
                                                               
                                                                
                                                                        
                              
         
      RelOptInfo *ref_rel = find_base_rel(root, fkinfo->ref_relid);
      double ref_tuples = Max(ref_rel->tuples, 1.0);

      fkselec *= ref_rel->rows / ref_tuples;
    }
    else
    {
         
                                                                        
                                                                      
                                                                       
         
      RelOptInfo *ref_rel = find_base_rel(root, fkinfo->ref_relid);
      double ref_tuples = Max(ref_rel->tuples, 1.0);

      fkselec *= 1.0 / ref_tuples;
    }
  }

  *restrictlist = worklist;
  return fkselec;
}

   
                               
                                                                   
   
                                                                         
                                                                     
                                                          
   
                                                         
   
void
set_subquery_size_estimates(PlannerInfo *root, RelOptInfo *rel)
{
  PlannerInfo *subroot = rel->subroot;
  RelOptInfo *sub_final_rel;
  ListCell *lc;

                                                                    
  Assert(rel->relid > 0);
  Assert(planner_rt_fetch(rel->relid, root)->rtekind == RTE_SUBQUERY);

     
                                                                            
                                                                    
     
  sub_final_rel = fetch_upper_rel(subroot, UPPERREL_FINAL, NULL);
  rel->tuples = sub_final_rel->cheapest_total_path->rows;

     
                                                                           
                                                                             
                                                                           
                                                                 
     
  foreach (lc, subroot->parse->targetList)
  {
    TargetEntry *te = lfirst_node(TargetEntry, lc);
    Node *texpr = (Node *)te->expr;
    int32 item_width = 0;

                                                    
    if (te->resjunk)
    {
      continue;
    }

       
                                                                       
                                                                         
                                                                        
                                                         
       
    if (te->resno < rel->min_attr || te->resno > rel->max_attr)
    {
      continue;
    }

       
                                                                     
                                                                         
                                                                        
                                                      
       
                                                                           
                                                              
                                                                           
       
                                                                   
                               
       
    if (IsA(texpr, Var) && subroot->parse->setOperations == NULL)
    {
      Var *var = (Var *)texpr;
      RelOptInfo *subrel = find_base_rel(subroot, var->varno);

      item_width = subrel->attr_widths[var->varattno - subrel->min_attr];
    }
    rel->attr_widths[te->resno - rel->min_attr] = item_width;
  }

                                               
  set_baserel_size_estimates(root, rel);
}

   
                               
                                                                        
   
                                                                         
            
   
                                                         
   
void
set_function_size_estimates(PlannerInfo *root, RelOptInfo *rel)
{
  RangeTblEntry *rte;
  ListCell *lc;

                                                                   
  Assert(rel->relid > 0);
  rte = planner_rt_fetch(rel->relid, root);
  Assert(rte->rtekind == RTE_FUNCTION);

     
                                                                            
                                                  
     
  rel->tuples = 0;
  foreach (lc, rte->functions)
  {
    RangeTblFunction *rtfunc = (RangeTblFunction *)lfirst(lc);
    double ntup = expression_returns_set_rows(root, rtfunc->funcexpr);

    if (ntup > rel->tuples)
    {
      rel->tuples = ntup;
    }
  }

                                               
  set_baserel_size_estimates(root, rel);
}

   
                               
                                                                        
   
                                                                         
            
   
                                                           
   
void
set_tablefunc_size_estimates(PlannerInfo *root, RelOptInfo *rel)
{
                                                                   
  Assert(rel->relid > 0);
  Assert(planner_rt_fetch(rel->relid, root)->rtekind == RTE_TABLEFUNC);

  rel->tuples = 100;

                                               
  set_baserel_size_estimates(root, rel);
}

   
                             
                                                                      
   
                                                                         
            
   
                                                         
   
void
set_values_size_estimates(PlannerInfo *root, RelOptInfo *rel)
{
  RangeTblEntry *rte;

                                                                      
  Assert(rel->relid > 0);
  rte = planner_rt_fetch(rel->relid, root);
  Assert(rte->rtekind == RTE_VALUES);

     
                                                                       
                                                                     
                                                                      
                            
     
  rel->tuples = list_length(rte->values_lists);

                                               
  set_baserel_size_estimates(root, rel);
}

   
                          
                                                                        
   
                                                                         
                                                                              
                                                                       
   
                                                         
   
void
set_cte_size_estimates(PlannerInfo *root, RelOptInfo *rel, double cte_rows)
{
  RangeTblEntry *rte;

                                                                        
  Assert(rel->relid > 0);
  rte = planner_rt_fetch(rel->relid, root);
  Assert(rte->rtekind == RTE_CTE);

  if (rte->self_reference)
  {
       
                                                                          
                                                       
       
    rel->tuples = 10 * cte_rows;
  }
  else
  {
                                                            
    rel->tuples = cte_rows;
  }

                                               
  set_baserel_size_estimates(root, rel);
}

   
                                      
                                                                               
   
                                                                         
            
   
                                                         
   
void
set_namedtuplestore_size_estimates(PlannerInfo *root, RelOptInfo *rel)
{
  RangeTblEntry *rte;

                                                                               
  Assert(rel->relid > 0);
  rte = planner_rt_fetch(rel->relid, root);
  Assert(rte->rtekind == RTE_NAMEDTUPLESTORE);

     
                                                                         
                                                                          
                                                                         
                         
     
  rel->tuples = rte->enrtuples;
  if (rel->tuples < 0)
  {
    rel->tuples = 1000;
  }

                                               
  set_baserel_size_estimates(root, rel);
}

   
                             
                                                           
   
                                                                         
            
   
                                                         
   
void
set_result_size_estimates(PlannerInfo *root, RelOptInfo *rel)
{
                                                           
  Assert(rel->relid > 0);
  Assert(planner_rt_fetch(rel->relid, root)->rtekind == RTE_RESULT);

                                                          
  rel->tuples = 1;

                                               
  set_baserel_size_estimates(root, rel);
}

   
                              
                                                                        
   
                                                                          
                                                                          
                                                                            
                                                                            
                                                                           
                                                                        
                                                                    
   
                                                                         
            
   
void
set_foreign_size_estimates(PlannerInfo *root, RelOptInfo *rel)
{
                                                
  Assert(rel->relid > 0);

  rel->rows = 1000;                                      

  cost_qual_eval(&rel->baserestrictcost, rel->baserestrictinfo, root);

  set_rel_width(root, rel);
}

   
                 
                                                       
   
                                                                              
                                                                           
                                                                            
                                                           
   
                                                                        
   
                                                                        
                                                                             
                                                                          
                                                                           
                                                                          
                      
   
                                                                          
                                                          
   
static void
set_rel_width(PlannerInfo *root, RelOptInfo *rel)
{
  Oid reloid = planner_rt_fetch(rel->relid, root)->relid;
  int32 tuple_width = 0;
  bool have_wholerow_var = false;
  ListCell *lc;

                                                                  
  rel->reltarget->cost.startup = 0;
  rel->reltarget->cost.per_tuple = 0;

  foreach (lc, rel->reltarget->exprs)
  {
    Node *node = (Node *)lfirst(lc);

       
                                                                        
                                                                          
                                                                      
                                                                           
       
    if (IsA(node, Var) && ((Var *)node)->varno == rel->relid)
    {
      Var *var = (Var *)node;
      int ndx;
      int32 item_width;

      Assert(var->varattno >= rel->min_attr);
      Assert(var->varattno <= rel->max_attr);

      ndx = var->varattno - rel->min_attr;

         
                                                                         
                                                         
         
      if (var->varattno == 0)
      {
        have_wholerow_var = true;
        continue;
      }

         
                                                                      
                                               
         
      if (rel->attr_widths[ndx] > 0)
      {
        tuple_width += rel->attr_widths[ndx];
        continue;
      }

                                                   
      if (reloid != InvalidOid && var->varattno > 0)
      {
        item_width = get_attavgwidth(reloid, var->varattno);
        if (item_width > 0)
        {
          rel->attr_widths[ndx] = item_width;
          tuple_width += item_width;
          continue;
        }
      }

         
                                                                         
                                   
         
      item_width = get_typavgwidth(var->vartype, var->vartypmod);
      Assert(item_width > 0);
      rel->attr_widths[ndx] = item_width;
      tuple_width += item_width;
    }
    else if (IsA(node, PlaceHolderVar))
    {
         
                                                                       
                                                                         
         
      PlaceHolderVar *phv = (PlaceHolderVar *)node;
      PlaceHolderInfo *phinfo = find_placeholder_info(root, phv, false);
      QualCost cost;

      tuple_width += phinfo->ph_width;
      cost_qual_eval_node(&cost, (Node *)phv->phexpr, root);
      rel->reltarget->cost.startup += cost.startup;
      rel->reltarget->cost.per_tuple += cost.per_tuple;
    }
    else
    {
         
                                                                         
                                                                         
                                                    
         
      int32 item_width;
      QualCost cost;

      item_width = get_typavgwidth(exprType(node), exprTypmod(node));
      Assert(item_width > 0);
      tuple_width += item_width;
                                                                        
      cost_qual_eval_node(&cost, node, root);
      rel->reltarget->cost.startup += cost.startup;
      rel->reltarget->cost.per_tuple += cost.per_tuple;
    }
  }

     
                                                                        
                                                        
     
  if (have_wholerow_var)
  {
    int32 wholerow_width = MAXALIGN(SizeofHeapTupleHeader);

    if (reloid != InvalidOid)
    {
                                                       
      wholerow_width += get_relation_data_width(reloid, rel->attr_widths - rel->min_attr);
    }
    else
    {
                                                    
      AttrNumber i;

      for (i = 1; i <= rel->max_attr; i++)
      {
        wholerow_width += rel->attr_widths[i - rel->min_attr];
      }
    }

    rel->attr_widths[0 - rel->min_attr] = wholerow_width;

       
                                                                         
                                          
       
    tuple_width += wholerow_width;
  }

  Assert(tuple_width >= 0);
  rel->reltarget->width = tuple_width;
}

   
                             
                                                                        
   
                                                                               
   
                                                                           
                                                                             
                                                                       
                                                                              
                                                                         
   
PathTarget *
set_pathtarget_cost_width(PlannerInfo *root, PathTarget *target)
{
  int32 tuple_width = 0;
  ListCell *lc;

                                                                  
  target->cost.startup = 0;
  target->cost.per_tuple = 0;

  foreach (lc, target->exprs)
  {
    Node *node = (Node *)lfirst(lc);

    if (IsA(node, Var))
    {
      Var *var = (Var *)node;
      int32 item_width;

                                                       
      Assert(var->varlevelsup == 0);

                                                 
      if (var->varno < root->simple_rel_array_size)
      {
        RelOptInfo *rel = root->simple_rel_array[var->varno];

        if (rel != NULL && var->varattno >= rel->min_attr && var->varattno <= rel->max_attr)
        {
          int ndx = var->varattno - rel->min_attr;

          if (rel->attr_widths[ndx] > 0)
          {
            tuple_width += rel->attr_widths[ndx];
            continue;
          }
        }
      }

         
                                                                         
         
      item_width = get_typavgwidth(var->vartype, var->vartypmod);
      Assert(item_width > 0);
      tuple_width += item_width;
    }
    else
    {
         
                                                     
         
      int32 item_width;
      QualCost cost;

      item_width = get_typavgwidth(exprType(node), exprTypmod(node));
      Assert(item_width > 0);
      tuple_width += item_width;

                                 
      cost_qual_eval_node(&cost, node, root);
      target->cost.startup += cost.startup;
      target->cost.per_tuple += cost.per_tuple;
    }
  }

  Assert(tuple_width >= 0);
  target->width = tuple_width;

  return target;
}

   
                      
                                                                      
                                       
   
static double
relation_byte_size(double tuples, int width)
{
  return tuples * (MAXALIGN(width) + MAXALIGN(SizeofHeapTupleHeader));
}

   
             
                                                                   
                                                        
   
static double
page_size(double tuples, int width)
{
  return ceil(relation_byte_size(tuples, width) / BLCKSZ);
}

   
                                                                        
                                            
   
static double
get_parallel_divisor(Path *path)
{
  double parallel_divisor = path->parallel_workers;

     
                                                                           
                                                                           
                                                                         
                                                                             
                                                                            
                                                                          
                                                                          
                                                                     
                    
     
  if (parallel_leader_participation)
  {
    double leader_contribution;

    leader_contribution = 1.0 - (0.3 * path->parallel_workers);
    if (leader_contribution > 0)
    {
      parallel_divisor += leader_contribution;
    }
  }

  return parallel_divisor;
}

   
                        
   
                                                                  
   
double
compute_bitmap_pages(PlannerInfo *root, RelOptInfo *baserel, Path *bitmapqual, int loop_count, Cost *cost, double *tuple)
{
  Cost indexTotalCost;
  Selectivity indexSelectivity;
  double T;
  double pages_fetched;
  double tuples_fetched;
  double heap_pages;
  long maxentries;

     
                                                                    
                  
     
  cost_bitmap_tree_node(bitmapqual, &indexTotalCost, &indexSelectivity);

     
                                                  
     
  tuples_fetched = clamp_row_est(indexSelectivity * baserel->tuples);

  T = (baserel->pages > 1) ? (double)baserel->pages : 1.0;

     
                                                                            
                                                                            
                       
     
  pages_fetched = (2.0 * T * tuples_fetched) / (2.0 * T + tuples_fetched);

     
                                                                         
                                                                           
                                                                           
                                                                          
                                                                           
                              
     
  heap_pages = Min(pages_fetched, baserel->pages);
  maxentries = tbm_calculate_entries(work_mem * 1024L);

  if (loop_count > 1)
  {
       
                                                                           
                                                                         
                                                                   
                              
       
    pages_fetched = index_pages_fetched(tuples_fetched * loop_count, baserel->pages, get_indexpath_pages(bitmapqual), root);
    pages_fetched /= loop_count;
  }

  if (pages_fetched >= T)
  {
    pages_fetched = T;
  }
  else
  {
    pages_fetched = ceil(pages_fetched);
  }

  if (maxentries < heap_pages)
  {
    double exact_pages;
    double lossy_pages;

       
                                                                         
                                                                       
                                                                        
                                                                          
                                            
       
    lossy_pages = Max(0, heap_pages - maxentries / 2);
    exact_pages = heap_pages - lossy_pages;

       
                                                                     
                                                                          
                                                                     
                                                                       
                                                
       
    if (lossy_pages > 0)
    {
      tuples_fetched = clamp_row_est(indexSelectivity * (exact_pages / heap_pages) * baserel->tuples + (lossy_pages / heap_pages) * baserel->tuples);
    }
  }

  if (cost)
  {
    *cost = indexTotalCost;
  }
  if (tuple)
  {
    *tuple = tuples_fetched;
  }

  return pages_fetched;
}
