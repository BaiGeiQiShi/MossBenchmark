                                                                            
   
               
                                                                      
   
                                                                               
                                                                           
   
                                                                             
                                                                        
                                                                         
                                                                               
                                 
   
                                                                           
                                                                        
                                                                              
                                                                           
   
                                                                           
                                                                           
                                                                         
                                                                     
                       
   
                                                                           
   
                                                                         
                                                                        
   
                  
                                           
   
                                                                            
   
#include "postgres.h"

#include "access/hash.h"
#include "access/nbtree.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/appendinfo.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/pathnode.h"
#include "parser/parsetree.h"
#include "partitioning/partbounds.h"
#include "partitioning/partprune.h"
#include "rewrite/rewriteManip.h"
#include "utils/lsyscache.h"

   
                                                            
   
typedef struct PartClauseInfo
{
  int keyno;                                                      
  Oid opno;                                                      
  bool op_is_ne;                                           
  Expr *expr;                                                 
  Oid cmpfn;                                                   
                                      
  int op_strategy;                                              
} PartClauseInfo;

   
                         
                                                            
   
typedef enum PartClauseMatchStatus
{
  PARTCLAUSE_NOMATCH,
  PARTCLAUSE_MATCH_CLAUSE,
  PARTCLAUSE_MATCH_NULLNESS,
  PARTCLAUSE_MATCH_STEPS,
  PARTCLAUSE_MATCH_CONTRADICT,
  PARTCLAUSE_UNSUPPORTED
} PartClauseMatchStatus;

   
                    
                                                                          
   
typedef enum PartClauseTarget
{
  PARTTARGET_PLANNER,                                    
  PARTTARGET_INITIAL,                                            
  PARTTARGET_EXEC                                                   
} PartClauseTarget;

   
                               
                                                                         
                               
   
                                                                             
   
                                                                            
                                                                               
                                                                       
                                                                        
   
typedef struct GeneratePruningStepsContext
{
                                                          
  RelOptInfo *rel;                                       
  PartClauseTarget target;                                          
                    
  List *steps;                                           
  bool has_mutable_op;                                            
  bool has_mutable_arg;                                           
                                                              
  bool has_exec_param;                                             
  bool contradictory;                                               
                      
  int next_step_id;
} GeneratePruningStepsContext;

                                                     
typedef struct PruneStepResult
{
     
                                                                       
                                   
     
  Bitmapset *bound_offsets;

  bool scan_default;                                  
  bool scan_null;                                             
} PruneStepResult;

static List *
make_partitionedrel_pruneinfo(PlannerInfo *root, RelOptInfo *parentrel, int *relid_subplan_map, List *partitioned_rels, List *prunequal, Bitmapset **matchedsubplans);
static void
gen_partprune_steps(RelOptInfo *rel, List *clauses, PartClauseTarget target, GeneratePruningStepsContext *context);
static List *
gen_partprune_steps_internal(GeneratePruningStepsContext *context, List *clauses);
static PartitionPruneStep *
gen_prune_step_op(GeneratePruningStepsContext *context, StrategyNumber opstrategy, bool op_is_ne, List *exprs, List *cmpfns, Bitmapset *nullkeys);
static PartitionPruneStep *
gen_prune_step_combine(GeneratePruningStepsContext *context, List *source_stepids, PartitionPruneCombineOp combineOp);
static PartitionPruneStep *
gen_prune_steps_from_opexps(GeneratePruningStepsContext *context, List **keyclauses, Bitmapset *nullkeys);
static PartClauseMatchStatus
match_clause_to_partition_key(GeneratePruningStepsContext *context, Expr *clause, Expr *partkey, int partkeyidx, bool *clause_is_not_null, PartClauseInfo **pc, List **clause_steps);
static List *
get_steps_using_prefix(GeneratePruningStepsContext *context, StrategyNumber step_opstrategy, bool step_op_is_ne, Expr *step_lastexpr, Oid step_lastcmpfn, int step_lastkeyno, Bitmapset *step_nullkeys, List *prefix);
static List *
get_steps_using_prefix_recurse(GeneratePruningStepsContext *context, StrategyNumber step_opstrategy, bool step_op_is_ne, Expr *step_lastexpr, Oid step_lastcmpfn, int step_lastkeyno, Bitmapset *step_nullkeys, ListCell *start, List *step_exprs, List *step_cmpfns);
static PruneStepResult *
get_matching_hash_bounds(PartitionPruneContext *context, StrategyNumber opstrategy, Datum *values, int nvalues, FmgrInfo *partsupfunc, Bitmapset *nullkeys);
static PruneStepResult *
get_matching_list_bounds(PartitionPruneContext *context, StrategyNumber opstrategy, Datum value, int nvalues, FmgrInfo *partsupfunc, Bitmapset *nullkeys);
static PruneStepResult *
get_matching_range_bounds(PartitionPruneContext *context, StrategyNumber opstrategy, Datum *values, int nvalues, FmgrInfo *partsupfunc, Bitmapset *nullkeys);
static Bitmapset *
pull_exec_paramids(Expr *expr);
static bool
pull_exec_paramids_walker(Node *node, Bitmapset **context);
static Bitmapset *
get_partkey_exec_paramids(List *steps);
static PruneStepResult *
perform_pruning_base_step(PartitionPruneContext *context, PartitionPruneStepOp *opstep);
static PruneStepResult *
perform_pruning_combine_step(PartitionPruneContext *context, PartitionPruneStepCombine *cstep, PruneStepResult **step_results);
static PartClauseMatchStatus
match_boolean_partition_clause(Oid partopfamily, Expr *clause, Expr *partkey, Expr **outconst);
static void
partkey_datum_from_expr(PartitionPruneContext *context, Expr *expr, int stateidx, Datum *value, bool *isnull);

   
                            
                                                                           
                                                                   
                                        
   
                                                                              
                                     
   
                                                                          
                                                                            
                                                                         
                                                                       
                                                                          
                                                                              
                                                                            
                                                                           
                                                                               
   
                                                     
   
PartitionPruneInfo *
make_partition_pruneinfo(PlannerInfo *root, RelOptInfo *parentrel, List *subpaths, List *partitioned_rels, List *prunequal)
{
  PartitionPruneInfo *pruneinfo;
  Bitmapset *allmatchedsubplans = NULL;
  int *relid_subplan_map;
  ListCell *lc;
  List *prunerelinfos;
  int i;

     
                                                                       
                                                                          
                                             
     
  relid_subplan_map = palloc0(sizeof(int) * root->simple_rel_array_size);

     
                                                                      
                                                     
     
  i = 1;
  foreach (lc, subpaths)
  {
    Path *path = (Path *)lfirst(lc);
    RelOptInfo *pathrel = path->parent;

    Assert(IS_SIMPLE_REL(pathrel));
    Assert(pathrel->relid < root->simple_rel_array_size);
                              
    Assert(relid_subplan_map[pathrel->relid] == 0);

    relid_subplan_map[pathrel->relid] = i++;
  }

                                                                        
  prunerelinfos = NIL;
  foreach (lc, partitioned_rels)
  {
    List *rels = (List *)lfirst(lc);
    List *pinfolist;
    Bitmapset *matchedsubplans = NULL;

    pinfolist = make_partitionedrel_pruneinfo(root, parentrel, relid_subplan_map, rels, prunequal, &matchedsubplans);

                                                               
    if (pinfolist != NIL)
    {
      prunerelinfos = lappend(prunerelinfos, pinfolist);
      allmatchedsubplans = bms_join(matchedsubplans, allmatchedsubplans);
    }
  }

  pfree(relid_subplan_map);

     
                                                                          
                                                               
     
  if (prunerelinfos == NIL)
  {
    return NULL;
  }

                                            
  pruneinfo = makeNode(PartitionPruneInfo);
  pruneinfo->prune_infos = prunerelinfos;

     
                                                                         
                                                                           
                                                                             
                                                                           
                                                         
     
  if (bms_num_members(allmatchedsubplans) < list_length(subpaths))
  {
    Bitmapset *other_subplans;

                                                     
    other_subplans = bms_add_range(NULL, 0, list_length(subpaths) - 1);
    other_subplans = bms_del_members(other_subplans, allmatchedsubplans);

    pruneinfo->other_subplans = other_subplans;
  }
  else
  {
    pruneinfo->other_subplans = NULL;
  }

  return pruneinfo;
}

   
                                 
                                                                       
                                                                          
                           
   
                                                                             
                                                                            
            
   
                                                                              
                                                                         
                                                                             
                                                                         
   
                                                                            
                                             
   
static List *
make_partitionedrel_pruneinfo(PlannerInfo *root, RelOptInfo *parentrel, int *relid_subplan_map, List *partitioned_rels, List *prunequal, Bitmapset **matchedsubplans)
{
  RelOptInfo *targetpart = NULL;
  List *pinfolist = NIL;
  bool doruntimeprune = false;
  int *relid_subpart_map;
  Bitmapset *subplansfound = NULL;
  ListCell *lc;
  int i;

     
                                                                         
                                                                         
                                                       
     
                                                                            
                                           
     
  relid_subpart_map = palloc0(sizeof(int) * root->simple_rel_array_size);

  i = 1;
  foreach (lc, partitioned_rels)
  {
    Index rti = lfirst_int(lc);
    RelOptInfo *subpart = find_base_rel(root, rti);
    PartitionedRelPruneInfo *pinfo;
    List *partprunequal;
    List *initial_pruning_steps;
    List *exec_pruning_steps;
    Bitmapset *execparamids;
    GeneratePruningStepsContext context;

       
                               
       
                                                                         
                                                                          
                                                                      
                                                                  
                                           
       
    Assert(rti < root->simple_rel_array_size);
                              
    Assert(relid_subpart_map[rti] == 0);
    relid_subpart_map[rti] = i++;

       
                                                                 
       
                                                                      
       
    if (!targetpart)
    {
      targetpart = subpart;

         
                                                                     
                                                                       
                                                                        
                                                                        
                                                                       
                                                              
         
      if (!bms_equal(parentrel->relids, subpart->relids))
      {
        int nappinfos;
        AppendRelInfo **appinfos = find_appinfos_by_relids(root, subpart->relids, &nappinfos);

        prunequal = (List *)adjust_appendrel_attrs(root, (Node *)prunequal, nappinfos, appinfos);

        pfree(appinfos);
      }

      partprunequal = prunequal;
    }
    else
    {
         
                                                                       
                                                                         
                                           
         
      partprunequal = (List *)adjust_appendrel_attrs_multilevel(root, (Node *)prunequal, subpart->relids, targetpart->relids);
    }

       
                                                                      
                                                                          
                                                                         
                                                                           
                                            
       
    gen_partprune_steps(subpart, partprunequal, PARTTARGET_INITIAL, &context);

    if (context.contradictory)
    {
         
                                                                        
                                                                         
                                                                        
                                                               
                                                                         
                                                                     
                                                                      
         
      return NIL;
    }

       
                                                                       
                                                                          
                                                                 
       
    if (context.has_mutable_op || context.has_mutable_arg)
    {
      initial_pruning_steps = context.steps;
    }
    else
    {
      initial_pruning_steps = NIL;
    }

       
                                                                       
                                                                      
       
    if (context.has_exec_param)
    {
                                              
      gen_partprune_steps(subpart, partprunequal, PARTTARGET_EXEC, &context);

      if (context.contradictory)
      {
                                                                       
        return NIL;
      }

      exec_pruning_steps = context.steps;

         
                                                                        
                                                                       
                                                  
         
      execparamids = get_partkey_exec_paramids(exec_pruning_steps);

      if (bms_is_empty(execparamids))
      {
        exec_pruning_steps = NIL;
      }
    }
    else
    {
                                                                      
      exec_pruning_steps = NIL;
      execparamids = NULL;
    }

    if (initial_pruning_steps || exec_pruning_steps)
    {
      doruntimeprune = true;
    }

                                                                     
    pinfo = makeNode(PartitionedRelPruneInfo);
    pinfo->rtindex = rti;
    pinfo->initial_pruning_steps = initial_pruning_steps;
    pinfo->exec_pruning_steps = exec_pruning_steps;
    pinfo->execparamids = execparamids;
                                                          

    pinfolist = lappend(pinfolist, pinfo);
  }

  if (!doruntimeprune)
  {
                                       
    pfree(relid_subpart_map);
    return NIL;
  }

     
                                                                         
                                                                          
                                                                           
                                                                    
                                                                       
                                   
     
  foreach (lc, pinfolist)
  {
    PartitionedRelPruneInfo *pinfo = lfirst(lc);
    RelOptInfo *subpart = find_base_rel(root, pinfo->rtindex);
    Bitmapset *present_parts;
    int nparts = subpart->nparts;
    int *subplan_map;
    int *subpart_map;
    Oid *relid_map;

       
                                                                           
                                                                         
                                                                           
                                
       
    subplan_map = (int *)palloc(nparts * sizeof(int));
    memset(subplan_map, -1, nparts * sizeof(int));
    subpart_map = (int *)palloc(nparts * sizeof(int));
    memset(subpart_map, -1, nparts * sizeof(int));
    relid_map = (Oid *)palloc0(nparts * sizeof(Oid));
    present_parts = NULL;

    for (i = 0; i < nparts; i++)
    {
      RelOptInfo *partrel = subpart->part_rels[i];
      int subplanidx;
      int subpartidx;

                                              
      if (partrel == NULL)
      {
        continue;
      }

      subplan_map[i] = subplanidx = relid_subplan_map[partrel->relid] - 1;
      subpart_map[i] = subpartidx = relid_subpart_map[partrel->relid] - 1;
      relid_map[i] = planner_rt_fetch(partrel->relid, root)->relid;
      if (subplanidx >= 0)
      {
        present_parts = bms_add_member(present_parts, i);

                                          
        subplansfound = bms_add_member(subplansfound, subplanidx);
      }
      else if (subpartidx >= 0)
      {
        present_parts = bms_add_member(present_parts, i);
      }
    }

                                                
    pinfo->present_parts = present_parts;
    pinfo->nparts = nparts;
    pinfo->subplan_map = subplan_map;
    pinfo->subpart_map = subpart_map;
    pinfo->relid_map = relid_map;
  }

  pfree(relid_subpart_map);

  *matchedsubplans = subplansfound;

  return pinfolist;
}

   
                       
                                                                           
                                                    
   
                                                                      
                                                                       
                                                                     
                                                
   
                                                                           
                                                                       
   
static void
gen_partprune_steps(RelOptInfo *rel, List *clauses, PartClauseTarget target, GeneratePruningStepsContext *context)
{
                                                       
  memset(context, 0, sizeof(GeneratePruningStepsContext));
  context->rel = rel;
  context->target = target;

     
                                                                   
                                                                           
                                                                     
                                                                   
                                                                        
                                                                         
                                                                            
                                                                           
                                                                           
                                                                           
                                                
     
  if (partition_bound_has_default(rel->boundinfo) && rel->partition_qual != NIL)
  {
    List *partqual = rel->partition_qual;

    partqual = (List *)expression_planner((Expr *)partqual);

                                            
    if (rel->relid != 1)
    {
      ChangeVarNodes((Node *)partqual, 1, rel->relid, 0);
    }

                                                             
    clauses = list_concat(list_copy(clauses), partqual);
  }

                                  
  (void)gen_partprune_steps_internal(context, clauses);
}

   
                               
                                                                      
                                                                          
                                                                         
                                                                      
                                                      
   
                                                          
   
Bitmapset *
prune_append_rel_partitions(RelOptInfo *rel)
{
  List *clauses = rel->baserestrictinfo;
  List *pruning_steps;
  GeneratePruningStepsContext gcontext;
  PartitionPruneContext context;

  Assert(rel->part_scheme != NULL);

                                                        
  if (rel->nparts == 0)
  {
    return NULL;
  }

     
                                                                             
                     
     
  if (!enable_partition_pruning || clauses == NIL)
  {
    return bms_add_range(NULL, 0, rel->nparts - 1);
  }

     
                                                                            
                                                                           
          
     
  gen_partprune_steps(rel, clauses, PARTTARGET_PLANNER, &gcontext);
  if (gcontext.contradictory)
  {
    return NULL;
  }
  pruning_steps = gcontext.steps;

                                                        
  if (pruning_steps == NIL)
  {
    return bms_add_range(NULL, 0, rel->nparts - 1);
  }

                                    
  context.strategy = rel->part_scheme->strategy;
  context.partnatts = rel->part_scheme->partnatts;
  context.nparts = rel->nparts;
  context.boundinfo = rel->boundinfo;
  context.partcollation = rel->part_scheme->partcollation;
  context.partsupfunc = rel->part_scheme->partsupfunc;
  context.stepcmpfuncs = (FmgrInfo *)palloc0(sizeof(FmgrInfo) * context.partnatts * list_length(pruning_steps));
  context.ppccontext = CurrentMemoryContext;

                                                              
  context.planstate = NULL;
  context.exprstates = NULL;

                                    
  return get_matching_partitions(&context, pruning_steps);
}

   
                           
                                                        
   
                                                                      
                                                                             
   
                                                                             
               
   
Bitmapset *
get_matching_partitions(PartitionPruneContext *context, List *pruning_steps)
{
  Bitmapset *result;
  int num_steps = list_length(pruning_steps), i;
  PruneStepResult **results, *final_result;
  ListCell *lc;
  bool scan_default;

                                                                
  if (num_steps == 0)
  {
    Assert(context->nparts > 0);
    return bms_add_range(NULL, 0, context->nparts - 1);
  }

     
                                                                            
                                                                             
                                                                       
                                                                             
                               
     
  results = (PruneStepResult **)palloc0(num_steps * sizeof(PruneStepResult *));
  foreach (lc, pruning_steps)
  {
    PartitionPruneStep *step = lfirst(lc);

    switch (nodeTag(step))
    {
    case T_PartitionPruneStepOp:
      results[step->step_id] = perform_pruning_base_step(context, (PartitionPruneStepOp *)step);
      break;

    case T_PartitionPruneStepCombine:
      results[step->step_id] = perform_pruning_combine_step(context, (PartitionPruneStepCombine *)step, results);
      break;

    default:
      elog(ERROR, "invalid pruning step type: %d", (int)nodeTag(step));
    }
  }

     
                                                                             
                                                                           
                                                                        
     
  final_result = results[num_steps - 1];
  Assert(final_result != NULL);
  i = -1;
  result = NULL;
  scan_default = final_result->scan_default;
  while ((i = bms_next_member(final_result->bound_offsets, i)) >= 0)
  {
    int partindex;

    Assert(i < context->boundinfo->nindexes);
    partindex = context->boundinfo->indexes[i];

    if (partindex < 0)
    {
         
                                                                    
                                                                     
                                                                   
                                                                      
                                                                    
                
         
                                                                         
                                                                      
         
      scan_default |= partition_bound_has_default(context->boundinfo);
      continue;
    }

    result = bms_add_member(result, partindex);
  }

                                                                    
  if (final_result->scan_null)
  {
    Assert(context->strategy == PARTITION_STRATEGY_LIST);
    Assert(partition_bound_accepts_nulls(context->boundinfo));
    result = bms_add_member(result, context->boundinfo->null_index);
  }
  if (scan_default)
  {
    Assert(context->strategy == PARTITION_STRATEGY_LIST || context->strategy == PARTITION_STRATEGY_RANGE);
    Assert(partition_bound_has_default(context->boundinfo));
    result = bms_add_member(result, context->boundinfo->default_index);
  }

  return result;
}

   
                                
                                                             
   
                                                                              
                                                                           
                                                                           
                                                                          
                                                                         
                                                                              
              
   
                                                                              
                                                        
   
                                                                              
                                                                              
                                
   
                                                                           
                                                                         
                                                                       
                                      
   
static List *
gen_partprune_steps_internal(GeneratePruningStepsContext *context, List *clauses)
{
  PartitionScheme part_scheme = context->rel->part_scheme;
  List *keyclauses[PARTITION_MAX_KEYS];
  Bitmapset *nullkeys = NULL, *notnullkeys = NULL;
  bool generate_opsteps = false;
  List *result = NIL;
  ListCell *lc;

  memset(keyclauses, 0, sizeof(keyclauses));
  foreach (lc, clauses)
  {
    Expr *clause = (Expr *)lfirst(lc);
    int i;

                                           
    if (IsA(clause, RestrictInfo))
    {
      clause = ((RestrictInfo *)clause)->clause;
    }

                                                 
    if (IsA(clause, Const) && (((Const *)clause)->constisnull || !DatumGetBool(((Const *)clause)->constvalue)))
    {
      context->contradictory = true;
      return NIL;
    }

                                            
    if (IsA(clause, BoolExpr))
    {
         
                                       
         
                                                                    
                                                                        
                                                                   
                                         
         
      if (is_orclause(clause))
      {
        List *arg_stepids = NIL;
        bool all_args_contradictory = true;
        ListCell *lc1;

           
                                                                  
                                                               
           
        Assert(!context->contradictory);

           
                                                                       
                                                                     
           
        foreach (lc1, ((BoolExpr *)clause)->args)
        {
          Expr *arg = lfirst(lc1);
          bool arg_contradictory;
          List *argsteps;

          argsteps = gen_partprune_steps_internal(context, list_make1(arg));
          arg_contradictory = context->contradictory;
                                                                 
          context->contradictory = false;

          if (arg_contradictory)
          {
                                                           
            continue;
          }
          else
          {
            all_args_contradictory = false;
          }

          if (argsteps != NIL)
          {
            PartitionPruneStep *step;

            Assert(list_length(argsteps) == 1);
            step = (PartitionPruneStep *)linitial(argsteps);
            arg_stepids = lappend_int(arg_stepids, step->step_id);
          }
          else
          {
               
                                                             
                                                                  
                                                             
                                                                 
                                                       
               
                                                                   
                                                             
                                                                 
                                                                 
                            
               
            List *partconstr = context->rel->partition_qual;
            PartitionPruneStep *orstep;

            if (partconstr)
            {
              partconstr = (List *)expression_planner((Expr *)partconstr);
              if (context->rel->relid != 1)
              {
                ChangeVarNodes((Node *)partconstr, 1, context->rel->relid, 0);
              }
              if (predicate_refuted_by(partconstr, list_make1(arg), false))
              {
                continue;
              }
            }

            orstep = gen_prune_step_combine(context, NIL, PARTPRUNE_COMBINE_UNION);
            arg_stepids = lappend_int(arg_stepids, orstep->step_id);
          }
        }

                                                               
        if (all_args_contradictory)
        {
          context->contradictory = true;
          return NIL;
        }

        if (arg_stepids != NIL)
        {
          PartitionPruneStep *step;

          step = gen_prune_step_combine(context, arg_stepids, PARTPRUNE_COMBINE_UNION);
          result = lappend(result, step);
        }
        continue;
      }
      else if (is_andclause(clause))
      {
        List *args = ((BoolExpr *)clause)->args;
        List *argsteps, *arg_stepids = NIL;
        ListCell *lc1;

           
                                                                      
                                                                   
                                 
           
        argsteps = gen_partprune_steps_internal(context, args);

                                                                      
        if (context->contradictory)
        {
          return NIL;
        }

        foreach (lc1, argsteps)
        {
          PartitionPruneStep *step = lfirst(lc1);

          arg_stepids = lappend_int(arg_stepids, step->step_id);
        }

        if (arg_stepids != NIL)
        {
          PartitionPruneStep *step;

          step = gen_prune_step_combine(context, arg_stepids, PARTPRUNE_COMBINE_INTERSECT);
          result = lappend(result, step);
        }
        continue;
      }

         
                                                                        
                                                                
                                                                  
                  
         
    }

       
                                                                     
       
    for (i = 0; i < part_scheme->partnatts; i++)
    {
      Expr *partkey = linitial(context->rel->partexprs[i]);
      bool clause_is_not_null = false;
      PartClauseInfo *pc = NULL;
      List *clause_steps = NIL;

      switch (match_clause_to_partition_key(context, clause, partkey, i, &clause_is_not_null, &pc, &clause_steps))
      {
      case PARTCLAUSE_MATCH_CLAUSE:
        Assert(pc != NULL);

           
                                                               
                                  
           
        if (bms_is_member(i, nullkeys))
        {
          context->contradictory = true;
          return NIL;
        }
        generate_opsteps = true;
        keyclauses[i] = lappend(keyclauses[i], pc);
        break;

      case PARTCLAUSE_MATCH_NULLNESS:
        if (!clause_is_not_null)
        {
             
                                                          
                                          
             
          if (bms_is_member(i, notnullkeys) || keyclauses[i] != NIL)
          {
            context->contradictory = true;
            return NIL;
          }
          nullkeys = bms_add_member(nullkeys, i);
        }
        else
        {
                                             
          if (bms_is_member(i, nullkeys))
          {
            context->contradictory = true;
            return NIL;
          }
          notnullkeys = bms_add_member(notnullkeys, i);
        }
        break;

      case PARTCLAUSE_MATCH_STEPS:
        Assert(clause_steps != NIL);
        result = list_concat(result, clause_steps);
        break;

      case PARTCLAUSE_MATCH_CONTRADICT:
                                                                    
        context->contradictory = true;
        return NIL;

      case PARTCLAUSE_NOMATCH:

           
                                                                
                     
           
        continue;

      case PARTCLAUSE_UNSUPPORTED:
                                                     
        break;
      }

                                           
      break;
    }
  }

                
                                                                        
     
                                                         
                                                                          
                                                                           
                                                                         
                                                                     
                                                  
                                                                         
                                                      
                                                                        
                                                                        
                                                                        
                           
     
                                                                  
     
                                                                         
                                                                            
                                                    
     
  if (!bms_is_empty(nullkeys) && (part_scheme->strategy == PARTITION_STRATEGY_LIST || part_scheme->strategy == PARTITION_STRATEGY_RANGE || (part_scheme->strategy == PARTITION_STRATEGY_HASH && bms_num_members(nullkeys) == part_scheme->partnatts)))
  {
    PartitionPruneStep *step;

                    
    step = gen_prune_step_op(context, InvalidStrategy, false, NIL, NIL, nullkeys);
    result = lappend(result, step);
  }
  else if (generate_opsteps)
  {
    PartitionPruneStep *step;

                    
    step = gen_prune_steps_from_opexps(context, keyclauses, nullkeys);
    if (step != NULL)
    {
      result = lappend(result, step);
    }
  }
  else if (bms_num_members(notnullkeys) == part_scheme->partnatts)
  {
    PartitionPruneStep *step;

                    
    step = gen_prune_step_op(context, InvalidStrategy, false, NIL, NIL, NULL);
    result = lappend(result, step);
  }

     
                                                                     
                                                                 
     
  if (list_length(result) > 1)
  {
    List *step_ids = NIL;

    foreach (lc, result)
    {
      PartitionPruneStep *step = lfirst(lc);

      step_ids = lappend_int(step_ids, step->step_id);
    }

    if (step_ids != NIL)
    {
      PartitionPruneStep *step;

      step = gen_prune_step_combine(context, step_ids, PARTPRUNE_COMBINE_INTERSECT);
      result = lappend(result, step);
    }
  }

  return result;
}

   
                     
                                                    
   
                                                                                
         
   
static PartitionPruneStep *
gen_prune_step_op(GeneratePruningStepsContext *context, StrategyNumber opstrategy, bool op_is_ne, List *exprs, List *cmpfns, Bitmapset *nullkeys)
{
  PartitionPruneStepOp *opstep = makeNode(PartitionPruneStepOp);

  opstep->step.step_id = context->next_step_id++;

     
                                                                
                                                                        
            
     
  opstep->opstrategy = op_is_ne ? InvalidStrategy : opstrategy;
  Assert(list_length(exprs) == list_length(cmpfns));
  opstep->exprs = exprs;
  opstep->cmpfns = cmpfns;
  opstep->nullkeys = nullkeys;

  context->steps = lappend(context->steps, opstep);

  return (PartitionPruneStep *)opstep;
}

   
                          
                                                                     
   
                                                                        
                 
   
static PartitionPruneStep *
gen_prune_step_combine(GeneratePruningStepsContext *context, List *source_stepids, PartitionPruneCombineOp combineOp)
{
  PartitionPruneStepCombine *cstep = makeNode(PartitionPruneStepCombine);

  cstep->step.step_id = context->next_step_id++;
  cstep->combineOp = combineOp;
  cstep->source_stepids = source_stepids;

  context->steps = lappend(context->steps, cstep);

  return (PartitionPruneStep *)cstep;
}

   
                               
                                                               
   
                                                                               
                                                                             
                                                                          
                                                                       
                                                            
   
static PartitionPruneStep *
gen_prune_steps_from_opexps(GeneratePruningStepsContext *context, List **keyclauses, Bitmapset *nullkeys)
{
  PartitionScheme part_scheme = context->rel->part_scheme;
  List *opsteps = NIL;
  List *btree_clauses[BTMaxStrategyNumber + 1], *hash_clauses[HTMaxStrategyNumber + 1];
  int i;
  ListCell *lc;

  memset(btree_clauses, 0, sizeof(btree_clauses));
  memset(hash_clauses, 0, sizeof(hash_clauses));
  for (i = 0; i < part_scheme->partnatts; i++)
  {
    List *clauselist = keyclauses[i];
    bool consider_next_key = true;

       
                                                                          
                                                                     
       
    if (part_scheme->strategy == PARTITION_STRATEGY_RANGE && clauselist == NIL)
    {
      break;
    }

       
                                                                     
                                                                     
                                
       
    if (part_scheme->strategy == PARTITION_STRATEGY_HASH && clauselist == NIL && !bms_is_member(i, nullkeys))
    {
      return NULL;
    }

    foreach (lc, clauselist)
    {
      PartClauseInfo *pc = (PartClauseInfo *)lfirst(lc);
      Oid lefttype, righttype;

                                                              
      if (pc->op_strategy == InvalidStrategy)
      {
        get_op_opfamily_properties(pc->opno, part_scheme->partopfamily[i], false, &pc->op_strategy, &lefttype, &righttype);
      }

      switch (part_scheme->strategy)
      {
      case PARTITION_STRATEGY_LIST:
      case PARTITION_STRATEGY_RANGE:
        btree_clauses[pc->op_strategy] = lappend(btree_clauses[pc->op_strategy], pc);

           
                                                              
                                                               
                     
           
        if (pc->op_strategy == BTLessStrategyNumber || pc->op_strategy == BTGreaterStrategyNumber)
        {
          consider_next_key = false;
        }
        break;

      case PARTITION_STRATEGY_HASH:
        if (pc->op_strategy != HTEqualStrategyNumber)
        {
          elog(ERROR, "invalid clause for hash partitioning");
        }
        hash_clauses[pc->op_strategy] = lappend(hash_clauses[pc->op_strategy], pc);
        break;

      default:
        elog(ERROR, "invalid partition strategy: %c", part_scheme->strategy);
        break;
      }
    }

       
                                                                   
                                                                 
       
    if (!consider_next_key)
    {
      break;
    }
  }

     
                                                                          
                                                                   
                                                                            
                                                                     
               
     
  switch (part_scheme->strategy)
  {
  case PARTITION_STRATEGY_LIST:
  case PARTITION_STRATEGY_RANGE:
  {
    List *eq_clauses = btree_clauses[BTEqualStrategyNumber];
    List *le_clauses = btree_clauses[BTLessEqualStrategyNumber];
    List *ge_clauses = btree_clauses[BTGreaterEqualStrategyNumber];
    int strat;

       
                                                                 
                                                                   
                                                          
                                                                 
                                                                
                                                                
                                                                 
                                                         
                                                            
                                                    
       
    for (strat = 1; strat <= BTMaxStrategyNumber; strat++)
    {
      foreach (lc, btree_clauses[strat])
      {
        PartClauseInfo *pc = lfirst(lc);
        ListCell *eq_start;
        ListCell *le_start;
        ListCell *ge_start;
        ListCell *lc1;
        List *prefix = NIL;
        List *pc_steps;
        bool prefix_valid = true;
        bool pk_has_clauses;
        int keyno;

           
                                                            
                                                          
                                          
           
                                                             
                                                             
                               
           
        if (pc->keyno == 0)
        {
          Assert(pc->op_strategy == strat);
          pc_steps = get_steps_using_prefix(context, strat, pc->op_is_ne, pc->expr, pc->cmpfn, 0, NULL, NIL);
          opsteps = list_concat(opsteps, pc_steps);
          continue;
        }

        eq_start = list_head(eq_clauses);
        le_start = list_head(le_clauses);
        ge_start = list_head(ge_clauses);

           
                                                             
                                           
           
        for (keyno = 0; keyno < pc->keyno; keyno++)
        {
          pk_has_clauses = false;

             
                                                             
                                                           
             
          for_each_cell(lc1, eq_start)
          {
            PartClauseInfo *eqpc = lfirst(lc1);

            if (eqpc->keyno == keyno)
            {
              prefix = lappend(prefix, eqpc);
              pk_has_clauses = true;
            }
            else
            {
              Assert(eqpc->keyno > keyno);
              break;
            }
          }
          eq_start = lc1;

             
                                                             
                                                     
                                                   
             
          if (strat == BTLessStrategyNumber || strat == BTLessEqualStrategyNumber)
          {
            for_each_cell(lc1, le_start)
            {
              PartClauseInfo *lepc = lfirst(lc1);

              if (lepc->keyno == keyno)
              {
                prefix = lappend(prefix, lepc);
                pk_has_clauses = true;
              }
              else
              {
                Assert(lepc->keyno > keyno);
                break;
              }
            }
            le_start = lc1;
          }

             
                                                             
                                                     
                                                   
             
          if (strat == BTGreaterStrategyNumber || strat == BTGreaterEqualStrategyNumber)
          {
            for_each_cell(lc1, ge_start)
            {
              PartClauseInfo *gepc = lfirst(lc1);

              if (gepc->keyno == keyno)
              {
                prefix = lappend(prefix, gepc);
                pk_has_clauses = true;
              }
              else
              {
                Assert(gepc->keyno > keyno);
                break;
              }
            }
            ge_start = lc1;
          }

             
                                                             
                      
             
          if (!pk_has_clauses)
          {
            prefix_valid = false;
            break;
          }
        }

           
                                                            
                                                            
                                                        
                                                             
                                           
           
                                                             
                                                            
                                                             
                                                  
           
                                                             
                                                             
                               
           
        if (prefix_valid)
        {
          Assert(pc->op_strategy == strat);
          pc_steps = get_steps_using_prefix(context, strat, pc->op_is_ne, pc->expr, pc->cmpfn, pc->keyno, NULL, prefix);
          opsteps = list_concat(opsteps, pc_steps);
        }
        else
        {
          break;
        }
      }
    }
    break;
  }

  case PARTITION_STRATEGY_HASH:
  {
    List *eq_clauses = hash_clauses[HTEqualStrategyNumber];

                                                             
    if (eq_clauses != NIL)
    {
      PartClauseInfo *pc;
      List *pc_steps;
      List *prefix = NIL;
      int last_keyno;
      ListCell *lc1;

         
                                                              
                                                             
                                                               
                           
         
      pc = llast(eq_clauses);

         
                                                               
                                                              
                                                              
         
      last_keyno = pc->keyno;
      foreach (lc, eq_clauses)
      {
        pc = lfirst(lc);
        if (pc->keyno == last_keyno)
        {
          break;
        }
        prefix = lappend(prefix, pc);
      }

         
                                                                
                                                            
                                                             
                                                             
                                                             
                                                               
                                                 
                                                          
         
      for_each_cell(lc1, lc)
      {
        pc = lfirst(lc1);

           
                                                         
                                                               
                                                               
                
           
        Assert(pc->op_strategy == HTEqualStrategyNumber);
        pc_steps = get_steps_using_prefix(context, HTEqualStrategyNumber, false, pc->expr, pc->cmpfn, pc->keyno, nullkeys, prefix);
        opsteps = list_concat(opsteps, list_copy(pc_steps));
      }
    }
    break;
  }

  default:
    elog(ERROR, "invalid partition strategy: %c", part_scheme->strategy);
    break;
  }

                                                                            
  if (list_length(opsteps) > 1)
  {
    List *opstep_ids = NIL;

    foreach (lc, opsteps)
    {
      PartitionPruneStep *step = lfirst(lc);

      opstep_ids = lappend_int(opstep_ids, step->step_id);
    }

    if (opstep_ids != NIL)
    {
      return gen_prune_step_combine(context, opstep_ids, PARTPRUNE_COMBINE_INTERSECT);
    }
    return NULL;
  }
  else if (opsteps != NIL)
  {
    return linitial(opsteps);
  }

  return NULL;
}

   
                                                                            
                                                                           
                                                                            
                                                                           
                                                      
   
                                      
   
#define PartCollMatchesExprColl(partcoll, exprcoll) ((partcoll) == InvalidOid || (partcoll) == (exprcoll))

   
                                 
                                                                          
   
                    
                                                                            
                                                                          
                                 
   
                                                  
                                                                          
                     
   
                                                                               
                                                     
                                                                              
                     
   
                                                 
                                                                            
                               
   
                                                                         
                                        
                                 
   
                                                                           
                                                                          
                                                          
                                 
   
static PartClauseMatchStatus
match_clause_to_partition_key(GeneratePruningStepsContext *context, Expr *clause, Expr *partkey, int partkeyidx, bool *clause_is_not_null, PartClauseInfo **pc, List **clause_steps)
{
  PartClauseMatchStatus boolmatchstatus;
  PartitionScheme part_scheme = context->rel->part_scheme;
  Oid partopfamily = part_scheme->partopfamily[partkeyidx], partcoll = part_scheme->partcollation[partkeyidx];
  Expr *expr;

     
                                                                            
     
  boolmatchstatus = match_boolean_partition_clause(partopfamily, clause, partkey, &expr);

  if (boolmatchstatus == PARTCLAUSE_MATCH_CLAUSE)
  {
    PartClauseInfo *partclause;

    partclause = (PartClauseInfo *)palloc(sizeof(PartClauseInfo));
    partclause->keyno = partkeyidx;
                                                        
    partclause->opno = BooleanEqualOperator;
    partclause->op_is_ne = false;
    partclause->expr = expr;
                                               
    partclause->cmpfn = part_scheme->partsupfunc[partkeyidx].fn_oid;
    partclause->op_strategy = InvalidStrategy;

    *pc = partclause;

    return PARTCLAUSE_MATCH_CLAUSE;
  }
  else if (IsA(clause, OpExpr) && list_length(((OpExpr *)clause)->args) == 2)
  {
    OpExpr *opclause = (OpExpr *)clause;
    Expr *leftop, *rightop;
    Oid opno, op_lefttype, op_righttype, negator = InvalidOid;
    Oid cmpfn;
    int op_strategy;
    bool is_opne_listp = false;
    PartClauseInfo *partclause;

    leftop = (Expr *)get_leftop(clause);
    if (IsA(leftop, RelabelType))
    {
      leftop = ((RelabelType *)leftop)->arg;
    }
    rightop = (Expr *)get_rightop(clause);
    if (IsA(rightop, RelabelType))
    {
      rightop = ((RelabelType *)rightop)->arg;
    }
    opno = opclause->opno;

                                                        
    if (equal(leftop, partkey))
    {
      expr = rightop;
    }
    else if (equal(rightop, partkey))
    {
         
                                                                    
                                                                     
                                                                         
                                                            
         
      opno = get_commutator(opno);
      if (!OidIsValid(opno))
      {
        return PARTCLAUSE_UNSUPPORTED;
      }
      expr = leftop;
    }
    else
    {
                                                                       
      return PARTCLAUSE_NOMATCH;
    }

       
                                                                        
                                                                
                                          
       
    if (!PartCollMatchesExprColl(partcoll, opclause->inputcollid))
    {
      return PARTCLAUSE_NOMATCH;
    }

       
                                                                     
       
                                                                           
                                                                         
                                                                      
                                                                         
                                                            
       
                                                                           
                                                                         
                                                                          
       
    if (op_in_opfamily(opno, partopfamily))
    {
      get_op_opfamily_properties(opno, partopfamily, false, &op_strategy, &op_lefttype, &op_righttype);
    }
    else
    {
      if (part_scheme->strategy != PARTITION_STRATEGY_LIST)
      {
        return PARTCLAUSE_NOMATCH;
      }

                                          
      negator = get_negator(opno);
      if (OidIsValid(negator) && op_in_opfamily(negator, partopfamily))
      {
        get_op_opfamily_properties(negator, partopfamily, false, &op_strategy, &op_lefttype, &op_righttype);
        if (op_strategy == BTEqualStrategyNumber)
        {
          is_opne_listp = true;            
        }
      }

                                     
      if (!is_opne_listp)
      {
        return PARTCLAUSE_NOMATCH;
      }
    }

       
                                                                   
                                                                     
                                                   
       
    if (!op_strict(opno))
    {
      return PARTCLAUSE_UNSUPPORTED;
    }

       
                                                                         
                                                                     
       
                                                                          
                                                                         
                                                                       
                                                                         
                                                                          
                                           
       
                                                                         
                                                                         
                                                                         
                                                                         
                                                 
       
                                                                           
                                                                
       
    if (!IsA(expr, Const))
    {
      Bitmapset *paramids;

         
                                                                    
                                                                    
                                                                        
                                                               
         
      if (context->target == PARTTARGET_PLANNER)
      {
        return PARTCLAUSE_UNSUPPORTED;
      }

         
                                                                    
         
      if (contain_var_clause((Node *)expr))
      {
        return PARTCLAUSE_UNSUPPORTED;
      }

         
                                                                     
                                         
         
      if (contain_volatile_functions((Node *)expr))
      {
        return PARTCLAUSE_UNSUPPORTED;
      }

         
                                                                        
                                             
         
      paramids = pull_exec_paramids(expr);
      if (!bms_is_empty(paramids))
      {
        context->has_exec_param = true;
        if (context->target != PARTTARGET_EXEC)
        {
          return PARTCLAUSE_UNSUPPORTED;
        }
      }
      else
      {
                                                  
        context->has_mutable_arg = true;
      }
    }

       
                                                                       
                                                                     
                                                       
       
    if (op_volatile(opno) != PROVOLATILE_IMMUTABLE)
    {
      context->has_mutable_op = true;

         
                                                                   
                    
         
      if (context->target == PARTTARGET_PLANNER)
      {
        return PARTCLAUSE_UNSUPPORTED;
      }
    }

       
                                                                           
                                                                        
                                                               
                                                                      
                                                        
       
    if (op_righttype == part_scheme->partopcintype[partkeyidx])
    {
      cmpfn = part_scheme->partsupfunc[partkeyidx].fn_oid;
    }
    else
    {
      switch (part_scheme->strategy)
      {
           
                                                                 
                                                                   
                                                             
           
      case PARTITION_STRATEGY_LIST:
      case PARTITION_STRATEGY_RANGE:
        cmpfn = get_opfamily_proc(part_scheme->partopfamily[partkeyidx], part_scheme->partopcintype[partkeyidx], op_righttype, BTORDER_PROC);
        break;

           
                                                                
                                  
           
      case PARTITION_STRATEGY_HASH:
        cmpfn = get_opfamily_proc(part_scheme->partopfamily[partkeyidx], op_righttype, op_righttype, HASHEXTENDED_PROC);
        break;

      default:
        elog(ERROR, "invalid partition strategy: %c", part_scheme->strategy);
        cmpfn = InvalidOid;                          
        break;
      }

      if (!OidIsValid(cmpfn))
      {
        return PARTCLAUSE_NOMATCH;
      }
    }

       
                                                            
       
    partclause = (PartClauseInfo *)palloc(sizeof(PartClauseInfo));
    partclause->keyno = partkeyidx;
    if (is_opne_listp)
    {
      Assert(OidIsValid(negator));
      partclause->opno = negator;
      partclause->op_is_ne = true;
      partclause->op_strategy = InvalidStrategy;
    }
    else
    {
      partclause->opno = opno;
      partclause->op_is_ne = false;
      partclause->op_strategy = op_strategy;
    }
    partclause->expr = expr;
    partclause->cmpfn = cmpfn;

    *pc = partclause;

    return PARTCLAUSE_MATCH_CLAUSE;
  }
  else if (IsA(clause, ScalarArrayOpExpr))
  {
    ScalarArrayOpExpr *saop = (ScalarArrayOpExpr *)clause;
    Oid saop_op = saop->opno;
    Oid saop_coll = saop->inputcollid;
    Expr *leftop = (Expr *)linitial(saop->args), *rightop = (Expr *)lsecond(saop->args);
    List *elem_exprs, *elem_clauses;
    ListCell *lc1;

    if (IsA(leftop, RelabelType))
    {
      leftop = ((RelabelType *)leftop)->arg;
    }

                                                     
    if (!equal(leftop, partkey) || !PartCollMatchesExprColl(partcoll, saop->inputcollid))
    {
      return PARTCLAUSE_NOMATCH;
    }

       
                                                                     
       
                                                                      
                                                                          
                                                                           
                                                                    
       
    if (!op_in_opfamily(saop_op, partopfamily))
    {
      Oid negator;

      if (part_scheme->strategy != PARTITION_STRATEGY_LIST)
      {
        return PARTCLAUSE_NOMATCH;
      }

      negator = get_negator(saop_op);
      if (OidIsValid(negator) && op_in_opfamily(negator, partopfamily))
      {
        int strategy;
        Oid lefttype, righttype;

        get_op_opfamily_properties(negator, partopfamily, false, &strategy, &lefttype, &righttype);
        if (strategy != BTEqualStrategyNumber)
        {
          return PARTCLAUSE_NOMATCH;
        }
      }
      else
      {
        return PARTCLAUSE_NOMATCH;                        
      }
    }

       
                                                                   
                                                                     
                                                   
       
    if (!op_strict(saop_op))
    {
      return PARTCLAUSE_UNSUPPORTED;
    }

       
                                                                         
                                                                           
                                                     
       
    if (!IsA(rightop, Const))
    {
      Bitmapset *paramids;

         
                                                                    
                                                                    
                                                                        
                                                               
         
      if (context->target == PARTTARGET_PLANNER)
      {
        return PARTCLAUSE_UNSUPPORTED;
      }

         
                                                                    
         
      if (contain_var_clause((Node *)rightop))
      {
        return PARTCLAUSE_UNSUPPORTED;
      }

         
                                                                     
                                         
         
      if (contain_volatile_functions((Node *)rightop))
      {
        return PARTCLAUSE_UNSUPPORTED;
      }

         
                                                                        
                                             
         
      paramids = pull_exec_paramids(rightop);
      if (!bms_is_empty(paramids))
      {
        context->has_exec_param = true;
        if (context->target != PARTTARGET_EXEC)
        {
          return PARTCLAUSE_UNSUPPORTED;
        }
      }
      else
      {
                                                  
        context->has_mutable_arg = true;
      }
    }

       
                                                                       
                                                                     
                                                       
       
    if (op_volatile(saop_op) != PROVOLATILE_IMMUTABLE)
    {
      context->has_mutable_op = true;

         
                                                                   
                    
         
      if (context->target == PARTTARGET_PLANNER)
      {
        return PARTCLAUSE_UNSUPPORTED;
      }
    }

       
                                                   
       
    elem_exprs = NIL;
    if (IsA(rightop, Const))
    {
         
                                                                       
                                                              
         
      Const *arr = (Const *)rightop;
      ArrayType *arrval;
      int16 elemlen;
      bool elembyval;
      char elemalign;
      Datum *elem_values;
      bool *elem_nulls;
      int num_elems, i;

                                                              
      if (arr->constisnull)
      {
        return PARTCLAUSE_MATCH_CONTRADICT;
      }

      arrval = DatumGetArrayTypeP(arr->constvalue);
      get_typlenbyvalalign(ARR_ELEMTYPE(arrval), &elemlen, &elembyval, &elemalign);
      deconstruct_array(arrval, ARR_ELEMTYPE(arrval), elemlen, elembyval, elemalign, &elem_values, &elem_nulls, &num_elems);
      for (i = 0; i < num_elems; i++)
      {
        Const *elem_expr;

           
                                                                       
                                                                   
                                                                    
           
        if (elem_nulls[i])
        {
          if (saop->useOr)
          {
            continue;
          }
          return PARTCLAUSE_MATCH_CONTRADICT;
        }

        elem_expr = makeConst(ARR_ELEMTYPE(arrval), -1, arr->constcollid, elemlen, elem_values[i], false, elembyval);
        elem_exprs = lappend(elem_exprs, elem_expr);
      }
    }
    else if (IsA(rightop, ArrayExpr))
    {
      ArrayExpr *arrexpr = castNode(ArrayExpr, rightop);

         
                                                                     
                                                                 
                                               
         
      if (arrexpr->multidims)
      {
        return PARTCLAUSE_UNSUPPORTED;
      }

         
                                                                
         
      elem_exprs = arrexpr->elements;
    }
    else
    {
                                              
      return PARTCLAUSE_UNSUPPORTED;
    }

       
                                                                          
                                          
       
    elem_clauses = NIL;
    foreach (lc1, elem_exprs)
    {
      Expr *rightop = (Expr *)lfirst(lc1), *elem_clause;

      elem_clause = make_opclause(saop_op, BOOLOID, false, leftop, rightop, InvalidOid, saop_coll);
      elem_clauses = lappend(elem_clauses, elem_clause);
    }

       
                                                                         
                                         
       
    if (saop->useOr && list_length(elem_clauses) > 1)
    {
      elem_clauses = list_make1(makeBoolExpr(OR_EXPR, elem_clauses, -1));
    }

                                 
    *clause_steps = gen_partprune_steps_internal(context, elem_clauses);
    if (context->contradictory)
    {
      return PARTCLAUSE_MATCH_CONTRADICT;
    }
    else if (*clause_steps == NIL)
    {
      return PARTCLAUSE_UNSUPPORTED;                             
    }
    return PARTCLAUSE_MATCH_STEPS;
  }
  else if (IsA(clause, NullTest))
  {
    NullTest *nulltest = (NullTest *)clause;
    Expr *arg = nulltest->arg;

    if (IsA(arg, RelabelType))
    {
      arg = ((RelabelType *)arg)->arg;
    }

                                                        
    if (!equal(arg, partkey))
    {
      return PARTCLAUSE_NOMATCH;
    }

    *clause_is_not_null = (nulltest->nulltesttype == IS_NOT_NULL);

    return PARTCLAUSE_MATCH_NULLNESS;
  }

     
                                                                       
                                                                      
                                                                           
                                                                          
                                                                            
                                                                          
                                                                             
                                                              
                                                    
                                                                           
                                                                         
                             
     
  return boolmatchstatus;
}

   
                          
                                                                         
               
   
                                                                       
                                                                       
                                                                            
                                                                              
                                     
   
                                                                          
                                                                      
                                                                       
                                                                             
                                                                           
                                                                              
                       
   
                                                                              
                                                      
   
static List *
get_steps_using_prefix(GeneratePruningStepsContext *context, StrategyNumber step_opstrategy, bool step_op_is_ne, Expr *step_lastexpr, Oid step_lastcmpfn, int step_lastkeyno, Bitmapset *step_nullkeys, List *prefix)
{
  Assert(step_nullkeys == NULL || context->rel->part_scheme->strategy == PARTITION_STRATEGY_HASH);

                                                         
  if (list_length(prefix) == 0)
  {
    PartitionPruneStep *step;

    step = gen_prune_step_op(context, step_opstrategy, step_op_is_ne, list_make1(step_lastexpr), list_make1_oid(step_lastcmpfn), step_nullkeys);
    return list_make1(step);
  }

                                                           
  return get_steps_using_prefix_recurse(context, step_opstrategy, step_op_is_ne, step_lastexpr, step_lastcmpfn, step_lastkeyno, step_nullkeys, list_head(prefix), NIL, NIL);
}

   
                                  
                                                                         
                                                                           
                                                                          
                                    
   
                                                                          
                                                                           
                                                                       
   
static List *
get_steps_using_prefix_recurse(GeneratePruningStepsContext *context, StrategyNumber step_opstrategy, bool step_op_is_ne, Expr *step_lastexpr, Oid step_lastcmpfn, int step_lastkeyno, Bitmapset *step_nullkeys, ListCell *start, List *step_exprs, List *step_cmpfns)
{
  List *result = NIL;
  ListCell *lc;
  int cur_keyno;

                                                                   
  check_stack_depth();

                                    
  Assert(start != NULL);
  cur_keyno = ((PartClauseInfo *)lfirst(start))->keyno;
  if (cur_keyno < step_lastkeyno - 1)
  {
    PartClauseInfo *pc;
    ListCell *next_start;

       
                                                                 
                                                                           
                                                                   
                      
       
    for_each_cell(lc, start)
    {
      pc = lfirst(lc);

      if (pc->keyno > cur_keyno)
      {
        break;
      }
    }
    next_start = lc;

    for_each_cell(lc, start)
    {
      List *moresteps;
      List *step_exprs1, *step_cmpfns1;

      pc = lfirst(lc);
      if (pc->keyno == cur_keyno)
      {
                                                       
        step_exprs1 = list_copy(step_exprs);
        step_exprs1 = lappend(step_exprs1, pc->expr);

                                                        
        step_cmpfns1 = list_copy(step_cmpfns);
        step_cmpfns1 = lappend_oid(step_cmpfns1, pc->cmpfn);
      }
      else
      {
        Assert(pc->keyno > cur_keyno);
        break;
      }

      moresteps = get_steps_using_prefix_recurse(context, step_opstrategy, step_op_is_ne, step_lastexpr, step_lastcmpfn, step_lastkeyno, step_nullkeys, next_start, step_exprs1, step_cmpfns1);
      result = list_concat(result, moresteps);

      list_free(step_exprs1);
      list_free(step_cmpfns1);
    }
  }
  else
  {
       
                                                                           
                                                                         
                                                                   
                                                                          
                                                                          
                                           
       
    Assert(list_length(step_exprs) == cur_keyno || !bms_is_empty(step_nullkeys));
       
                                                                       
                                                                  
                                                                       
                         
       
    Assert(context->rel->part_scheme->strategy != PARTITION_STRATEGY_HASH || list_length(step_exprs) + 2 + bms_num_members(step_nullkeys) == context->rel->part_scheme->partnatts);
    for_each_cell(lc, start)
    {
      PartClauseInfo *pc = lfirst(lc);
      PartitionPruneStep *step;
      List *step_exprs1, *step_cmpfns1;

      Assert(pc->keyno == cur_keyno);

                                                     
      step_exprs1 = list_copy(step_exprs);
      step_exprs1 = lappend(step_exprs1, pc->expr);
      step_exprs1 = lappend(step_exprs1, step_lastexpr);

                                                      
      step_cmpfns1 = list_copy(step_cmpfns);
      step_cmpfns1 = lappend_oid(step_cmpfns1, pc->cmpfn);
      step_cmpfns1 = lappend_oid(step_cmpfns1, step_lastcmpfn);

      step = gen_prune_step_op(context, step_opstrategy, step_op_is_ne, step_exprs1, step_cmpfns1, step_nullkeys);
      result = lappend(result, step);
    }
  }

  return result;
}

   
                            
                                                                      
                                                                          
                                                                        
                            
   
                                                                       
                                                                              
                                                                             
                                                         
   
                                                           
   
                                                                             
   
                                                          
   
                                                                               
                                                          
   
                                                          
   
static PruneStepResult *
get_matching_hash_bounds(PartitionPruneContext *context, StrategyNumber opstrategy, Datum *values, int nvalues, FmgrInfo *partsupfunc, Bitmapset *nullkeys)
{
  PruneStepResult *result = (PruneStepResult *)palloc0(sizeof(PruneStepResult));
  PartitionBoundInfo boundinfo = context->boundinfo;
  int *partindices = boundinfo->indexes;
  int partnatts = context->partnatts;
  bool isnull[PARTITION_MAX_KEYS];
  int i;
  uint64 rowHash;
  int greatest_modulus;
  Oid *partcollation = context->partcollation;

  Assert(context->strategy == PARTITION_STRATEGY_HASH);

     
                                                                         
                                                                        
                                          
     
  if (nvalues + bms_num_members(nullkeys) == partnatts)
  {
       
                                                                 
                                                                          
       
    Assert(opstrategy == HTEqualStrategyNumber || nvalues == 0);

    for (i = 0; i < partnatts; i++)
    {
      isnull[i] = bms_is_member(i, nullkeys);
    }

    rowHash = compute_partition_hash_value(partnatts, partsupfunc, partcollation, values, isnull);

    greatest_modulus = boundinfo->nindexes;
    if (partindices[rowHash % greatest_modulus] >= 0)
    {
      result->bound_offsets = bms_make_singleton(rowHash % greatest_modulus);
    }
  }
  else
  {
                                                                     
    result->bound_offsets = bms_add_range(NULL, 0, boundinfo->nindexes - 1);
  }

     
                                                                        
                
     
  result->scan_null = result->scan_default = false;

  return result;
}

   
                            
                                                                       
                                                              
   
                                                                             
                                                                              
                                                             
   
                                                             
   
                                                  
   
                                                                              
   
                                                                               
                                     
   
                                                          
   
static PruneStepResult *
get_matching_list_bounds(PartitionPruneContext *context, StrategyNumber opstrategy, Datum value, int nvalues, FmgrInfo *partsupfunc, Bitmapset *nullkeys)
{
  PruneStepResult *result = (PruneStepResult *)palloc0(sizeof(PruneStepResult));
  PartitionBoundInfo boundinfo = context->boundinfo;
  int off, minoff, maxoff;
  bool is_equal;
  bool inclusive = false;
  Oid *partcollation = context->partcollation;

  Assert(context->strategy == PARTITION_STRATEGY_LIST);
  Assert(context->partnatts == 1);

  result->scan_null = result->scan_default = false;

  if (!bms_is_empty(nullkeys))
  {
       
                                                                   
                                                                        
                                 
       
    if (partition_bound_accepts_nulls(boundinfo))
    {
      result->scan_null = true;
    }
    else
    {
      result->scan_default = partition_bound_has_default(boundinfo);
    }
    return result;
  }

     
                                                                            
                                                      
     
  if (boundinfo->ndatums == 0)
  {
    result->scan_default = partition_bound_has_default(boundinfo);
    return result;
  }

  minoff = 0;
  maxoff = boundinfo->ndatums - 1;

     
                                                                        
                                                                         
                                                                
     
  if (nvalues == 0)
  {
    Assert(boundinfo->ndatums > 0);
    result->bound_offsets = bms_add_range(NULL, 0, boundinfo->ndatums - 1);
    result->scan_default = partition_bound_has_default(boundinfo);
    return result;
  }

                                                                         
  if (opstrategy == InvalidStrategy)
  {
       
                                                                           
       
    Assert(boundinfo->ndatums > 0);
    result->bound_offsets = bms_add_range(NULL, 0, boundinfo->ndatums - 1);

    off = partition_list_bsearch(partsupfunc, partcollation, boundinfo, value, &is_equal);
    if (off >= 0 && is_equal)
    {

                                                    
      Assert(boundinfo->indexes[off] >= 0);
      result->bound_offsets = bms_del_member(result->bound_offsets, off);
    }

                                                      
    result->scan_default = partition_bound_has_default(boundinfo);

    return result;
  }

     
                                                                            
                                                                             
                                                                             
                                                                            
                                                                            
                                                      
     
  if (opstrategy != BTEqualStrategyNumber)
  {
    result->scan_default = partition_bound_has_default(boundinfo);
  }

  switch (opstrategy)
  {
  case BTEqualStrategyNumber:
    off = partition_list_bsearch(partsupfunc, partcollation, boundinfo, value, &is_equal);
    if (off >= 0 && is_equal)
    {
      Assert(boundinfo->indexes[off] >= 0);
      result->bound_offsets = bms_make_singleton(off);
    }
    else
    {
      result->scan_default = partition_bound_has_default(boundinfo);
    }
    return result;

  case BTGreaterEqualStrategyNumber:
    inclusive = true;
                      
  case BTGreaterStrategyNumber:
    off = partition_list_bsearch(partsupfunc, partcollation, boundinfo, value, &is_equal);
    if (off >= 0)
    {
                                                                
      if (!is_equal || !inclusive)
      {
        off++;
      }
    }
    else
    {
         
                                                                    
                                                          
         
      off = 0;
    }

       
                                                                    
                                                                       
                                                                       
                                   
       
    if (off > boundinfo->ndatums - 1)
    {
      return result;
    }

    minoff = off;
    break;

  case BTLessEqualStrategyNumber:
    inclusive = true;
                      
  case BTLessStrategyNumber:
    off = partition_list_bsearch(partsupfunc, partcollation, boundinfo, value, &is_equal);
    if (off >= 0 && is_equal && !inclusive)
    {
      off--;
    }

       
                                                                     
                                                                     
                                                                   
                                   
       
    if (off < 0)
    {
      return result;
    }

    maxoff = off;
    break;

  default:
    elog(ERROR, "invalid strategy number %d", opstrategy);
    break;
  }

  Assert(minoff >= 0 && maxoff >= 0);
  result->bound_offsets = bms_add_range(NULL, minoff, maxoff);
  return result;
}

   
                             
                                                                         
                                                              
   
                                                                               
                                                       
   
                                                                            
                                                                              
                                                                       
                                                                              
                                                                            
                                                                      
              
   
                                                             
   
                                                                             
   
                                                                                 
   
                                                                            
                                                                               
          
   
                                                          
   
static PruneStepResult *
get_matching_range_bounds(PartitionPruneContext *context, StrategyNumber opstrategy, Datum *values, int nvalues, FmgrInfo *partsupfunc, Bitmapset *nullkeys)
{
  PruneStepResult *result = (PruneStepResult *)palloc0(sizeof(PruneStepResult));
  PartitionBoundInfo boundinfo = context->boundinfo;
  Oid *partcollation = context->partcollation;
  int partnatts = context->partnatts;
  int *partindices = boundinfo->indexes;
  int off, minoff, maxoff;
  bool is_equal;
  bool inclusive = false;

  Assert(context->strategy == PARTITION_STRATEGY_RANGE);
  Assert(nvalues <= partnatts);

  result->scan_null = result->scan_default = false;

     
                                                                          
                                                             
     
  if (boundinfo->ndatums == 0 || !bms_is_empty(nullkeys))
  {
    result->scan_default = partition_bound_has_default(boundinfo);
    return result;
  }

  minoff = 0;
  maxoff = boundinfo->ndatums;

     
                                                                        
                                                                         
                                                                         
             
     
  if (nvalues == 0)
  {
                                                        
    if (partindices[minoff] < 0)
    {
      minoff++;
    }
    if (partindices[maxoff] < 0)
    {
      maxoff--;
    }

    result->scan_default = partition_bound_has_default(boundinfo);
    Assert(partindices[minoff] >= 0 && partindices[maxoff] >= 0);
    result->bound_offsets = bms_add_range(NULL, minoff, maxoff);

    return result;
  }

     
                                                                             
                                
     
  if (nvalues < partnatts)
  {
    result->scan_default = partition_bound_has_default(boundinfo);
  }

  switch (opstrategy)
  {
  case BTEqualStrategyNumber:
                                                             
    off = partition_range_datum_bsearch(partsupfunc, partcollation, boundinfo, nvalues, values, &is_equal);

    if (off >= 0 && is_equal)
    {
      if (nvalues == partnatts)
      {
                                                               
        result->bound_offsets = bms_make_singleton(off + 1);
        return result;
      }
      else
      {
        int saved_off = off;

           
                                                                  
                                                             
                                                                
                                                                   
                   
           

           
                                                             
                         
           
        while (off >= 1)
        {
          int32 cmpval;

          cmpval = partition_rbound_datum_cmp(partsupfunc, partcollation, boundinfo->datums[off - 1], boundinfo->kind[off - 1], values, nvalues);
          if (cmpval != 0)
          {
            break;
          }
          off--;
        }

        Assert(0 == partition_rbound_datum_cmp(partsupfunc, partcollation, boundinfo->datums[off], boundinfo->kind[off], values, nvalues));

           
                                                                  
                                                              
                                                                  
                                                                 
                                                                  
                             
           
        if (boundinfo->kind[off][nvalues] == PARTITION_RANGE_DATUM_MINVALUE)
        {
          off++;
        }

        minoff = off;

           
                                                                  
                  
           
        off = saved_off;
        while (off < boundinfo->ndatums - 1)
        {
          int32 cmpval;

          cmpval = partition_rbound_datum_cmp(partsupfunc, partcollation, boundinfo->datums[off + 1], boundinfo->kind[off + 1], values, nvalues);
          if (cmpval != 0)
          {
            break;
          }
          off++;
        }

        Assert(0 == partition_rbound_datum_cmp(partsupfunc, partcollation, boundinfo->datums[off], boundinfo->kind[off], values, nvalues));

           
                                                                   
                                         
           
        maxoff = off + 1;
      }

      Assert(minoff >= 0 && maxoff >= 0);
      result->bound_offsets = bms_add_range(NULL, minoff, maxoff);
    }
    else
    {
         
                                                                    
                                                                     
                                                               
                                                              
                                                                   
                                                                   
                                                             
         
      result->bound_offsets = bms_make_singleton(off + 1);
    }

    return result;

  case BTGreaterEqualStrategyNumber:
    inclusive = true;
                      
  case BTGreaterStrategyNumber:

       
                                                                    
                                 
       
    off = partition_range_datum_bsearch(partsupfunc, partcollation, boundinfo, nvalues, values, &is_equal);
    if (off < 0)
    {
         
                                                                  
                                    
         
      minoff = 0;
    }
    else
    {
      if (is_equal && nvalues < partnatts)
      {
           
                                                                  
                                                             
                                                                
                                                                   
                   
           
                                                               
                                                               
                                                                  
                                                                   
                                                                  
           
        while (off >= 1 && off < boundinfo->ndatums - 1)
        {
          int32 cmpval;
          int nextoff;

          nextoff = inclusive ? off - 1 : off + 1;
          cmpval = partition_rbound_datum_cmp(partsupfunc, partcollation, boundinfo->datums[nextoff], boundinfo->kind[nextoff], values, nvalues);
          if (cmpval != 0)
          {
            break;
          }

          off = nextoff;
        }

        Assert(0 == partition_rbound_datum_cmp(partsupfunc, partcollation, boundinfo->datums[off], boundinfo->kind[off], values, nvalues));

        minoff = inclusive ? off : off + 1;
      }
      else
      {

           
                                                                  
                                                               
                                                                
                                                                  
                                                                 
           
        minoff = off + 1;
      }
    }
    break;

  case BTLessEqualStrategyNumber:
    inclusive = true;
                      
  case BTLessStrategyNumber:

       
                                                                    
                                 
       
    off = partition_range_datum_bsearch(partsupfunc, partcollation, boundinfo, nvalues, values, &is_equal);
    if (off >= 0)
    {
         
                                
         
      if (is_equal && nvalues < partnatts)
      {
        while (off >= 1 && off < boundinfo->ndatums - 1)
        {
          int32 cmpval;
          int nextoff;

          nextoff = inclusive ? off + 1 : off - 1;
          cmpval = partition_rbound_datum_cmp(partsupfunc, partcollation, boundinfo->datums[nextoff], boundinfo->kind[nextoff], values, nvalues);
          if (cmpval != 0)
          {
            break;
          }

          off = nextoff;
        }

        Assert(0 == partition_rbound_datum_cmp(partsupfunc, partcollation, boundinfo->datums[off], boundinfo->kind[off], values, nvalues));

        maxoff = inclusive ? off + 1 : off;
      }

         
                                                                    
                                                                     
                                                               
                                                                  
                                                                 
                                                           
                                                        
         
      else if (!is_equal || inclusive)
      {
        maxoff = off + 1;
      }
      else
      {
        maxoff = off;
      }
    }
    else
    {
         
                                                                     
                                                 
         
      maxoff = off + 1;
    }
    break;

  default:
    elog(ERROR, "invalid strategy number %d", opstrategy);
    break;
  }

  Assert(minoff >= 0 && minoff <= boundinfo->ndatums);
  Assert(maxoff >= 0 && maxoff <= boundinfo->ndatums);

     
                                                                             
                                                                     
                                                                       
                                     
     
  if (minoff < boundinfo->ndatums && partindices[minoff] < 0)
  {
    int lastkey = nvalues - 1;

    if (boundinfo->kind[minoff][lastkey] == PARTITION_RANGE_DATUM_MINVALUE)
    {
      minoff++;
      Assert(boundinfo->indexes[minoff] >= 0);
    }
  }

     
                                                                            
                                                                            
                                                                      
                                                                      
                        
     
  if (maxoff >= 1 && partindices[maxoff] < 0)
  {
    int lastkey = nvalues - 1;

    if (boundinfo->kind[maxoff - 1][lastkey] == PARTITION_RANGE_DATUM_MAXVALUE)
    {
      maxoff--;
      Assert(boundinfo->indexes[maxoff] >= 0);
    }
  }

  Assert(minoff >= 0 && maxoff >= 0);
  if (minoff <= maxoff)
  {
    result->bound_offsets = bms_add_range(NULL, minoff, maxoff);
  }

  return result;
}

   
                      
                                                                   
                                      
   
static Bitmapset *
pull_exec_paramids(Expr *expr)
{
  Bitmapset *result = NULL;

  (void)pull_exec_paramids_walker((Node *)expr, &result);

  return result;
}

static bool
pull_exec_paramids_walker(Node *node, Bitmapset **context)
{
  if (node == NULL)
  {
    return false;
  }
  if (IsA(node, Param))
  {
    Param *param = (Param *)node;

    if (param->paramkind == PARAM_EXEC)
    {
      *context = bms_add_member(*context, param->paramid);
    }
    return false;
  }
  return expression_tree_walker(node, pull_exec_paramids_walker, (void *)context);
}

   
                             
                                                                    
              
   
                                     
   
static Bitmapset *
get_partkey_exec_paramids(List *steps)
{
  Bitmapset *execparamids = NULL;
  ListCell *lc;

  foreach (lc, steps)
  {
    PartitionPruneStepOp *step = (PartitionPruneStepOp *)lfirst(lc);
    ListCell *lc2;

    if (!IsA(step, PartitionPruneStepOp))
    {
      continue;
    }

    foreach (lc2, step->exprs)
    {
      Expr *expr = lfirst(lc2);

                                            
      if (!IsA(expr, Const))
      {
        execparamids = bms_join(execparamids, pull_exec_paramids(expr));
      }
    }
  }

  return execparamids;
}

   
                             
                                                                          
              
   
                                                                      
                                 
   
static PruneStepResult *
perform_pruning_base_step(PartitionPruneContext *context, PartitionPruneStepOp *opstep)
{
  ListCell *lc1, *lc2;
  int keyno, nvalues;
  Datum values[PARTITION_MAX_KEYS];
  FmgrInfo *partsupfunc;
  int stateidx;

     
                                                                           
     
  Assert(list_length(opstep->exprs) == list_length(opstep->cmpfns));

  nvalues = 0;
  lc1 = list_head(opstep->exprs);
  lc2 = list_head(opstep->cmpfns);

     
                                                                       
                                                   
     
  for (keyno = 0; keyno < context->partnatts; keyno++)
  {
       
                                                                          
                                                                       
                                               
       
    if (bms_is_member(keyno, opstep->nullkeys))
    {
      continue;
    }

       
                                                                        
                                                          
       
    if (keyno > nvalues && context->strategy == PARTITION_STRATEGY_RANGE)
    {
      break;
    }

    if (lc1 != NULL)
    {
      Expr *expr;
      Datum datum;
      bool isnull;
      Oid cmpfn;

      expr = lfirst(lc1);
      stateidx = PruneCxtStateIdx(context->partnatts, opstep->step.step_id, keyno);
      partkey_datum_from_expr(context, expr, stateidx, &datum, &isnull);

         
                                                                    
                                                                         
                                            
         
      if (isnull)
      {
        PruneStepResult *result;

        result = (PruneStepResult *)palloc(sizeof(PruneStepResult));
        result->bound_offsets = NULL;
        result->scan_default = false;
        result->scan_null = false;

        return result;
      }

                                                                
      cmpfn = lfirst_oid(lc2);
      Assert(OidIsValid(cmpfn));
      if (cmpfn != context->stepcmpfuncs[stateidx].fn_oid)
      {
           
                                                                    
                                                                   
                                                                      
                                      
           
        if (cmpfn == context->partsupfunc[keyno].fn_oid)
        {
          fmgr_info_copy(&context->stepcmpfuncs[stateidx], &context->partsupfunc[keyno], context->ppccontext);
        }
        else
        {
          fmgr_info_cxt(cmpfn, &context->stepcmpfuncs[stateidx], context->ppccontext);
        }
      }

      values[keyno] = datum;
      nvalues++;

      lc1 = lnext(lc1);
      lc2 = lnext(lc2);
    }
  }

     
                                                                      
                                                                 
     
  stateidx = PruneCxtStateIdx(context->partnatts, opstep->step.step_id, 0);
  partsupfunc = &context->stepcmpfuncs[stateidx];

  switch (context->strategy)
  {
  case PARTITION_STRATEGY_HASH:
    return get_matching_hash_bounds(context, opstep->opstrategy, values, nvalues, partsupfunc, opstep->nullkeys);

  case PARTITION_STRATEGY_LIST:
    return get_matching_list_bounds(context, opstep->opstrategy, values[0], nvalues, &partsupfunc[0], opstep->nullkeys);

  case PARTITION_STRATEGY_RANGE:
    return get_matching_range_bounds(context, opstep->opstrategy, values, nvalues, partsupfunc, opstep->nullkeys);

  default:
    elog(ERROR, "unexpected partition strategy: %d", (int)context->strategy);
    break;
  }

  return NULL;
}

   
                                
                                                                       
                                                                         
                       
   
                                                                         
                      
   
static PruneStepResult *
perform_pruning_combine_step(PartitionPruneContext *context, PartitionPruneStepCombine *cstep, PruneStepResult **step_results)
{
  PruneStepResult *result = (PruneStepResult *)palloc0(sizeof(PruneStepResult));
  bool firststep;
  ListCell *lc1;

     
                                                                             
                                                                    
     
  if (cstep->source_stepids == NIL)
  {
    PartitionBoundInfo boundinfo = context->boundinfo;

    result->bound_offsets = bms_add_range(NULL, 0, boundinfo->nindexes - 1);
    result->scan_default = partition_bound_has_default(boundinfo);
    result->scan_null = partition_bound_accepts_nulls(boundinfo);
    return result;
  }

  switch (cstep->combineOp)
  {
  case PARTPRUNE_COMBINE_UNION:
    foreach (lc1, cstep->source_stepids)
    {
      int step_id = lfirst_int(lc1);
      PruneStepResult *step_result;

         
                                                                     
                                                                    
                                                                   
                                                      
         
      if (step_id >= cstep->step.step_id)
      {
        elog(ERROR, "invalid pruning combine step argument");
      }
      step_result = step_results[step_id];
      Assert(step_result != NULL);

                                                              
      result->bound_offsets = bms_add_members(result->bound_offsets, step_result->bound_offsets);

                                                               
      if (!result->scan_null)
      {
        result->scan_null = step_result->scan_null;
      }
      if (!result->scan_default)
      {
        result->scan_default = step_result->scan_default;
      }
    }
    break;

  case PARTPRUNE_COMBINE_INTERSECT:
    firststep = true;
    foreach (lc1, cstep->source_stepids)
    {
      int step_id = lfirst_int(lc1);
      PruneStepResult *step_result;

      if (step_id >= cstep->step.step_id)
      {
        elog(ERROR, "invalid pruning combine step argument");
      }
      step_result = step_results[step_id];
      Assert(step_result != NULL);

      if (firststep)
      {
                                                
        result->bound_offsets = bms_copy(step_result->bound_offsets);
        result->scan_null = step_result->scan_null;
        result->scan_default = step_result->scan_default;
        firststep = false;
      }
      else
      {
                                                       
        result->bound_offsets = bms_int_members(result->bound_offsets, step_result->bound_offsets);

                                                                 
        if (result->scan_null)
        {
          result->scan_null = step_result->scan_null;
        }
        if (result->scan_default)
        {
          result->scan_default = step_result->scan_default;
        }
      }
    }
    break;
  }

  return result;
}

   
                                  
   
                                                                              
                                                                             
                                                                              
                                                                             
                                                                           
                                                                            
                     
   
static PartClauseMatchStatus
match_boolean_partition_clause(Oid partopfamily, Expr *clause, Expr *partkey, Expr **outconst)
{
  Expr *leftop;

  *outconst = NULL;

  if (!IsBooleanOpfamily(partopfamily))
  {
    return PARTCLAUSE_UNSUPPORTED;
  }

  if (IsA(clause, BooleanTest))
  {
    BooleanTest *btest = (BooleanTest *)clause;

                                                     
    if (btest->booltesttype == IS_UNKNOWN || btest->booltesttype == IS_NOT_UNKNOWN)
    {
      return PARTCLAUSE_UNSUPPORTED;
    }

    leftop = btest->arg;
    if (IsA(leftop, RelabelType))
    {
      leftop = ((RelabelType *)leftop)->arg;
    }

    if (equal(leftop, partkey))
    {
      *outconst = (btest->booltesttype == IS_TRUE || btest->booltesttype == IS_NOT_FALSE) ? (Expr *)makeBoolConst(true, false) : (Expr *)makeBoolConst(false, false);
    }

    if (*outconst)
    {
      return PARTCLAUSE_MATCH_CLAUSE;
    }
  }
  else
  {
    bool is_not_clause = is_notclause(clause);

    leftop = is_not_clause ? get_notclausearg(clause) : clause;

    if (IsA(leftop, RelabelType))
    {
      leftop = ((RelabelType *)leftop)->arg;
    }

                                                                
    if (equal(leftop, partkey))
    {
      *outconst = is_not_clause ? (Expr *)makeBoolConst(false, false) : (Expr *)makeBoolConst(true, false);
    }
    else if (equal(negate_clause((Node *)leftop), partkey))
    {
      *outconst = (Expr *)makeBoolConst(false, false);
    }

    if (*outconst)
    {
      return PARTCLAUSE_MATCH_CLAUSE;
    }
  }

  return PARTCLAUSE_NOMATCH;
}

   
                           
                                                        
   
                                                                                
   
                                                                      
                    
   
                                                                            
                                                                           
                                                                           
                                                                      
   
static void
partkey_datum_from_expr(PartitionPruneContext *context, Expr *expr, int stateidx, Datum *value, bool *isnull)
{
  if (IsA(expr, Const))
  {
                                                         
    Const *con = (Const *)expr;

    *value = con->constvalue;
    *isnull = con->constisnull;
  }
  else
  {
    ExprState *exprstate;
    ExprContext *ectx;

       
                                                                         
                     
       
    Assert(context->planstate != NULL);

    exprstate = context->exprstates[stateidx];
    ectx = context->planstate->ps_ExprContext;
    *value = ExecEvalExprSwitchContext(exprstate, ectx, isnull);
  }
}
