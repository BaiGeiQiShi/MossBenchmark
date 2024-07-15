                                                                            
   
                   
                                        
   
                                                                         
                                                                        
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include "access/table.h"
#include "access/tableam.h"
#include "catalog/partition.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_type.h"
#include "executor/execPartition.h"
#include "executor/executor.h"
#include "foreign/fdwapi.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "partitioning/partbounds.h"
#include "partitioning/partdesc.h"
#include "partitioning/partprune.h"
#include "rewrite/rewriteManip.h"
#include "utils/lsyscache.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/rls.h"
#include "utils/ruleutils.h"

                          
                                                                    
                                                                      
               
   
                  
                                                            
   
                           
                                                               
                                                                          
                                                                     
                                                               
                                                                    
             
   
                      
                                                                 
                                                                      
                              
   
                
                                                                        
                                                                        
                                                          
   
                
                                                                       
   
              
                                                                 
                                                                      
                                                                           
                                                                        
                                                                           
                                          
   
                  
                                                                        
                                                               
                                                  
   
                  
                                                          
   
                          
                                                                        
                                                                      
                                                                        
                                        
   
          
                                                        
                          
   
struct PartitionTupleRouting
{
  Relation partition_root;
  PartitionDispatch *partition_dispatch_info;
  ResultRelInfo **nonleaf_partitions;
  int num_dispatch;
  int max_dispatch;
  ResultRelInfo **partitions;
  int num_partitions;
  int max_partitions;
  HTAB *subplan_resultrel_htab;
  MemoryContext memcxt;
};

                          
                                                                              
                                                                    
                                                                           
                                                                 
   
           
                                     
   
       
                                           
   
            
                                                                  
   
            
                                      
   
           
                                                                    
                                                                     
              
   
          
                                                                            
                                                                       
                                                                       
                            
   
           
                                                                       
                                                                      
                                                                           
                                                                 
                                                                         
                                                         
                          
   
typedef struct PartitionDispatchData
{
  Relation reldesc;
  PartitionKey key;
  List *keystate;                        
  PartitionDesc partdesc;
  TupleTableSlot *tupslot;
  AttrNumber *tupmap;
  int indexes[FLEXIBLE_ARRAY_MEMBER];
} PartitionDispatchData;

                                                                 
typedef struct SubplanResultRelHashElem
{
  Oid relid;                                
  ResultRelInfo *rri;
} SubplanResultRelHashElem;

static void
ExecHashSubPlanResultRelsByOid(ModifyTableState *mtstate, PartitionTupleRouting *proute);
static ResultRelInfo *
ExecInitPartitionInfo(ModifyTableState *mtstate, EState *estate, PartitionTupleRouting *proute, PartitionDispatch dispatch, ResultRelInfo *rootResultRelInfo, int partidx);
static void
ExecInitRoutingInfo(ModifyTableState *mtstate, EState *estate, PartitionTupleRouting *proute, PartitionDispatch dispatch, ResultRelInfo *partRelInfo, int partidx);
static PartitionDispatch
ExecInitPartitionDispatchInfo(EState *estate, PartitionTupleRouting *proute, Oid partoid, PartitionDispatch parent_pd, int partidx, ResultRelInfo *rootResultRelInfo);
static void
FormPartitionKeyDatum(PartitionDispatch pd, TupleTableSlot *slot, EState *estate, Datum *values, bool *isnull);
static int
get_partition_for_tuple(PartitionDispatch pd, Datum *values, bool *isnull);
static char *
ExecBuildSlotPartitionKeyDescription(Relation rel, Datum *values, bool *isnull, int maxfieldlen);
static List *
adjust_partition_tlist(List *tlist, TupleConversionMap *map);
static void
ExecInitPruningContext(PartitionPruneContext *context, List *pruning_steps, PartitionDesc partdesc, PartitionKey partkey, PlanState *planstate);
static void
find_matching_subplans_recurse(PartitionPruningData *prunedata, PartitionedRelPruningData *pprune, bool initial_prune, Bitmapset **validsubplans);

   
                                                                      
                                                            
                                          
   
                                                                       
                                                                          
                                                             
   
                                                                      
                                                                          
                                      
   
PartitionTupleRouting *
ExecSetupPartitionTupleRouting(EState *estate, ModifyTableState *mtstate, Relation rel)
{
  PartitionTupleRouting *proute;
  ModifyTable *node = mtstate ? (ModifyTable *)mtstate->ps.plan : NULL;

     
                                                                          
                                                                            
                                                                            
                                                                         
                                                                  
     
  proute = (PartitionTupleRouting *)palloc0(sizeof(PartitionTupleRouting));
  proute->partition_root = rel;
  proute->memcxt = CurrentMemoryContext;
                                              

     
                                                                            
                                                                            
                        
     
  ExecInitPartitionDispatchInfo(estate, proute, RelationGetRelid(rel), NULL, 0, NULL);

     
                                                                        
                                                                     
                                                                           
                                                                           
                                                                            
                                           
     
  if (node && node->operation == CMD_UPDATE)
  {
    ExecHashSubPlanResultRelsByOid(mtstate, proute);
  }

  return proute;
}

   
                                                                             
                                                  
   
                                                                               
                                                                           
                                                                         
                                                                     
   
                                                         
   
                                                                            
                                                                        
                  
   
                                                                               
                                                                             
                                        
   
ResultRelInfo *
ExecFindPartition(ModifyTableState *mtstate, ResultRelInfo *rootResultRelInfo, PartitionTupleRouting *proute, TupleTableSlot *slot, EState *estate)
{
  PartitionDispatch *pd = proute->partition_dispatch_info;
  Datum values[PARTITION_MAX_KEYS];
  bool isnull[PARTITION_MAX_KEYS];
  Relation rel;
  PartitionDispatch dispatch;
  PartitionDesc partdesc;
  ExprContext *ecxt = GetPerTupleExprContext(estate);
  TupleTableSlot *ecxt_scantuple_saved = ecxt->ecxt_scantuple;
  TupleTableSlot *rootslot = slot;
  TupleTableSlot *myslot = NULL;
  MemoryContext oldcxt;
  ResultRelInfo *rri = NULL;

                                                          
  oldcxt = MemoryContextSwitchTo(GetPerTupleMemoryContext(estate));

     
                                                                             
                                                                      
     
  if (rootResultRelInfo->ri_PartitionCheck)
  {
    ExecPartitionCheck(rootResultRelInfo, slot, estate, true);
  }

                                             
  dispatch = pd[0];
  while (dispatch != NULL)
  {
    int partidx = -1;

    CHECK_FOR_INTERRUPTS();

    rel = dispatch->reldesc;
    partdesc = dispatch->partdesc;

       
                                                                         
                                                                      
                                                                          
                                                                      
                                                                          
                                             
       
    ecxt->ecxt_scantuple = slot;
    FormPartitionKeyDatum(dispatch, slot, estate, values, isnull);

       
                                                                       
                                
       
    if (partdesc->nparts == 0 || (partidx = get_partition_for_tuple(dispatch, values, isnull)) < 0)
    {
      char *val_desc;

      val_desc = ExecBuildSlotPartitionKeyDescription(rel, values, isnull, 64);
      Assert(OidIsValid(RelationGetRelid(rel)));
      ereport(ERROR, (errcode(ERRCODE_CHECK_VIOLATION), errmsg("no partition of relation \"%s\" found for row", RelationGetRelationName(rel)), val_desc ? errdetail("Partition key of the failing row contains %s.", val_desc) : 0));
    }

    if (partdesc->is_leaf[partidx])
    {
         
                                                                       
                                                               
         
      if (likely(dispatch->indexes[partidx] >= 0))
      {
                                         
        Assert(dispatch->indexes[partidx] < proute->num_partitions);
        rri = proute->partitions[dispatch->indexes[partidx]];
      }
      else
      {
        bool found = false;

           
                                                                      
                                                                  
                                                     
           
        if (proute->subplan_resultrel_htab)
        {
          Oid partoid = partdesc->oids[partidx];
          SubplanResultRelHashElem *elem;

          elem = hash_search(proute->subplan_resultrel_htab, &partoid, HASH_FIND, NULL);
          if (elem)
          {
            found = true;
            rri = elem->rri;

                                                          
            CheckValidResultRel(rri, CMD_INSERT);

                                                        
            ExecInitRoutingInfo(mtstate, estate, proute, dispatch, rri, partidx);
          }
        }

                                          
        if (!found)
        {
          rri = ExecInitPartitionInfo(mtstate, estate, proute, dispatch, rootResultRelInfo, partidx);
        }
      }
      Assert(rri != NULL);

                                        
      dispatch = NULL;
    }
    else
    {
         
                                                                         
         
      if (likely(dispatch->indexes[partidx] >= 0))
      {
                            
        Assert(dispatch->indexes[partidx] < proute->num_dispatch);

        rri = proute->nonleaf_partitions[dispatch->indexes[partidx]];

           
                                                                  
                                                                  
           
        dispatch = pd[dispatch->indexes[partidx]];
      }
      else
      {
                                         
        PartitionDispatch subdispatch;

           
                                                                      
                                              
           
        subdispatch = ExecInitPartitionDispatchInfo(estate, proute, partdesc->oids[partidx], dispatch, partidx, mtstate->rootResultRelInfo);
        Assert(dispatch->indexes[partidx] >= 0 && dispatch->indexes[partidx] < proute->num_dispatch);

        rri = proute->nonleaf_partitions[dispatch->indexes[partidx]];
        dispatch = subdispatch;
      }

         
                                                                         
                              
         
      if (dispatch->tupslot)
      {
        AttrNumber *map = dispatch->tupmap;
        TupleTableSlot *tempslot = myslot;

        myslot = dispatch->tupslot;
        slot = execute_attr_map_slot(map, slot, myslot);

        if (tempslot != NULL)
        {
          ExecClearTuple(tempslot);
        }
      }
    }

       
                                                                         
                                                                  
                                             
       
                                                                         
                                                                
       
    if (partidx == partdesc->boundinfo->default_index)
    {
      PartitionRoutingInfo *partrouteinfo = rri->ri_PartitionInfo;

         
                                                                        
                                                                       
                                                                         
                                                                     
                          
         
                                                                 
                                                                        
                                                                         
                              
         
      if (partrouteinfo)
      {
        TupleConversionMap *map = partrouteinfo->pi_RootToPartitionMap;

        if (map)
        {
          slot = execute_attr_map_slot(map->attrMap, rootslot, partrouteinfo->pi_PartitionTupleSlot);
        }
        else
        {
          slot = rootslot;
        }
      }

      ExecPartitionCheck(rri, slot, estate, true);
    }
  }

                                                                
  if (myslot != NULL)
  {
    ExecClearTuple(myslot);
  }
                                    
  ecxt->ecxt_scantuple = ecxt_scantuple_saved;
  MemoryContextSwitchTo(oldcxt);

  return rri;
}

   
                                  
                                                                          
                                                                       
                      
   
static void
ExecHashSubPlanResultRelsByOid(ModifyTableState *mtstate, PartitionTupleRouting *proute)
{
  HASHCTL ctl;
  HTAB *htab;
  int i;

  memset(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(Oid);
  ctl.entrysize = sizeof(SubplanResultRelHashElem);
  ctl.hcxt = CurrentMemoryContext;

  htab = hash_create("PartitionTupleRouting table", mtstate->mt_nplans, &ctl, HASH_ELEM | HASH_BLOBS | HASH_CONTEXT);
  proute->subplan_resultrel_htab = htab;

                                      
  for (i = 0; i < mtstate->mt_nplans; i++)
  {
    ResultRelInfo *rri = &mtstate->resultRelInfo[i];
    bool found;
    Oid partoid = RelationGetRelid(rri->ri_RelationDesc);
    SubplanResultRelHashElem *elem;

    elem = (SubplanResultRelHashElem *)hash_search(htab, &partoid, HASH_ENTER, &found);
    Assert(!found);
    elem->rri = rri;

       
                                                                        
                                                                           
                                                                 
       
    rri->ri_RootResultRelInfo = mtstate->rootResultRelInfo;
  }
}

   
                         
                                                                       
                                                                         
                                  
   
                             
   
static ResultRelInfo *
ExecInitPartitionInfo(ModifyTableState *mtstate, EState *estate, PartitionTupleRouting *proute, PartitionDispatch dispatch, ResultRelInfo *rootResultRelInfo, int partidx)
{
  ModifyTable *node = (ModifyTable *)mtstate->ps.plan;
  Relation partrel;
  int firstVarno = mtstate->resultRelInfo[0].ri_RangeTableIndex;
  Relation firstResultRel = mtstate->resultRelInfo[0].ri_RelationDesc;
  ResultRelInfo *leaf_part_rri;
  MemoryContext oldcxt;
  AttrNumber *part_attnos = NULL;
  bool found_whole_row;

  oldcxt = MemoryContextSwitchTo(proute->memcxt);

  partrel = table_open(dispatch->partdesc->oids[partidx], RowExclusiveLock);

  leaf_part_rri = makeNode(ResultRelInfo);
  InitResultRelInfo(leaf_part_rri, partrel, 0, rootResultRelInfo, estate->es_instrument);

     
                                                                             
                                                                             
                                                
     
  CheckValidResultRel(leaf_part_rri, CMD_INSERT);

     
                                                                             
                                                                        
                                                                            
                                                               
     
  if (partrel->rd_rel->relhasindex && leaf_part_rri->ri_IndexRelationDescs == NULL)
  {
    ExecOpenIndices(leaf_part_rri, (node != NULL && node->onConflictAction != ONCONFLICT_NONE));
  }

     
                                                                          
                                                                             
                                                                             
                                                                            
                                                                     
     
  if (node && node->withCheckOptionLists != NIL)
  {
    List *wcoList;
    List *wcoExprs = NIL;
    ListCell *ll;

       
                                                                       
                                                                           
                                                                   
       
    Assert((node->operation == CMD_INSERT && list_length(node->withCheckOptionLists) == 1 && list_length(node->plans) == 1) || (node->operation == CMD_UPDATE && list_length(node->withCheckOptionLists) == list_length(node->plans)));

       
                                                                      
                                                                        
                                                                        
                                                                     
                                                                          
                                                                         
                                       
       
    wcoList = linitial(node->withCheckOptionLists);

       
                                                                         
       
    part_attnos = convert_tuples_by_name_map(RelationGetDescr(partrel), RelationGetDescr(firstResultRel), gettext_noop("could not convert row type"));
    wcoList = (List *)map_variable_attnos((Node *)wcoList, firstVarno, 0, part_attnos, RelationGetDescr(firstResultRel)->natts, RelationGetForm(partrel)->reltype, &found_whole_row);
                                                 

    foreach (ll, wcoList)
    {
      WithCheckOption *wco = castNode(WithCheckOption, lfirst(ll));
      ExprState *wcoExpr = ExecInitQual(castNode(List, wco->qual), &mtstate->ps);

      wcoExprs = lappend(wcoExprs, wcoExpr);
    }

    leaf_part_rri->ri_WithCheckOptions = wcoList;
    leaf_part_rri->ri_WithCheckOptionExprs = wcoExprs;
  }

     
                                                                            
                                                                           
                                                                             
                                                                        
                                                              
     
  if (node && node->returningLists != NIL)
  {
    TupleTableSlot *slot;
    ExprContext *econtext;
    List *returningList;

                                              
    Assert((node->operation == CMD_INSERT && list_length(node->returningLists) == 1 && list_length(node->plans) == 1) || (node->operation == CMD_UPDATE && list_length(node->returningLists) == list_length(node->plans)));

       
                                                                  
                                                                        
                                                                       
             
       
    returningList = linitial(node->returningLists);

       
                                                                         
       
    if (part_attnos == NULL)
    {
      part_attnos = convert_tuples_by_name_map(RelationGetDescr(partrel), RelationGetDescr(firstResultRel), gettext_noop("could not convert row type"));
    }
    returningList = (List *)map_variable_attnos((Node *)returningList, firstVarno, 0, part_attnos, RelationGetDescr(firstResultRel)->natts, RelationGetForm(partrel)->reltype, &found_whole_row);
                                                 

    leaf_part_rri->ri_returningList = returningList;

       
                                         
       
                                                                           
                                                         
       
    Assert(mtstate->ps.ps_ResultTupleSlot != NULL);
    slot = mtstate->ps.ps_ResultTupleSlot;
    Assert(mtstate->ps.ps_ExprContext != NULL);
    econtext = mtstate->ps.ps_ExprContext;
    leaf_part_rri->ri_projectReturning = ExecBuildProjectionInfo(returningList, econtext, slot, &mtstate->ps, RelationGetDescr(partrel));
  }

                                                                      
  ExecInitRoutingInfo(mtstate, estate, proute, dispatch, leaf_part_rri, partidx);

     
                                                                 
     
  if (node && node->onConflictAction != ONCONFLICT_NONE)
  {
    TupleDesc partrelDesc = RelationGetDescr(partrel);
    ExprContext *econtext = mtstate->ps.ps_ExprContext;
    ListCell *lc;
    List *arbiterIndexes = NIL;

       
                                                                          
                                                                       
                                                                          
                       
       
    if (list_length(rootResultRelInfo->ri_onConflictArbiterIndexes) > 0)
    {
      List *childIdxs;

      childIdxs = RelationGetIndexList(leaf_part_rri->ri_RelationDesc);

      foreach (lc, childIdxs)
      {
        Oid childIdx = lfirst_oid(lc);
        List *ancestors;
        ListCell *lc2;

        ancestors = get_partition_ancestors(childIdx);
        foreach (lc2, rootResultRelInfo->ri_onConflictArbiterIndexes)
        {
          if (list_member_oid(ancestors, lfirst_oid(lc2)))
          {
            arbiterIndexes = lappend_oid(arbiterIndexes, childIdx);
          }
        }
        list_free(ancestors);
      }
    }

       
                                                                         
                                                                        
                                  
       
    if (list_length(rootResultRelInfo->ri_onConflictArbiterIndexes) != list_length(arbiterIndexes))
    {
      elog(ERROR, "invalid arbiter index list");
    }
    leaf_part_rri->ri_onConflictArbiterIndexes = arbiterIndexes;

       
                                                                     
       
    if (node->onConflictAction == ONCONFLICT_UPDATE)
    {
      OnConflictSetState *onconfl = makeNode(OnConflictSetState);
      TupleConversionMap *map;

      map = leaf_part_rri->ri_PartitionInfo->pi_RootToPartitionMap;

      Assert(node->onConflictSet != NIL);
      Assert(rootResultRelInfo->ri_onConflict != NULL);

      leaf_part_rri->ri_onConflict = onconfl;

         
                                                                  
                                                                 
                            
         
      onconfl->oc_Existing = table_slot_create(leaf_part_rri->ri_RelationDesc, &mtstate->ps.state->es_tupleTable);

         
                                                                      
                                                                         
                                                                      
                                                          
         
      if (map == NULL)
      {
           
                                                                   
                                                                
                                                               
                                                                  
                                                                      
                                                        
           
        onconfl->oc_ProjSlot = rootResultRelInfo->ri_onConflict->oc_ProjSlot;
        onconfl->oc_ProjInfo = rootResultRelInfo->ri_onConflict->oc_ProjInfo;
        onconfl->oc_WhereClause = rootResultRelInfo->ri_onConflict->oc_WhereClause;
      }
      else
      {
        List *onconflset;
        bool found_whole_row;

           
                                                                 
                                                                 
                                                        
                                                                      
                                         
           
        onconflset = copyObject(node->onConflictSet);
        if (part_attnos == NULL)
        {
          part_attnos = convert_tuples_by_name_map(RelationGetDescr(partrel), RelationGetDescr(firstResultRel), gettext_noop("could not convert row type"));
        }
        onconflset = (List *)map_variable_attnos((Node *)onconflset, INNER_VAR, 0, part_attnos, RelationGetDescr(firstResultRel)->natts, RelationGetForm(partrel)->reltype, &found_whole_row);
                                                     
        onconflset = (List *)map_variable_attnos((Node *)onconflset, firstVarno, 0, part_attnos, RelationGetDescr(firstResultRel)->natts, RelationGetForm(partrel)->reltype, &found_whole_row);
                                                     

                                                                
        onconflset = adjust_partition_tlist(onconflset, map);

                                                                 
        onconfl->oc_ProjSlot = table_slot_create(partrel, &mtstate->ps.state->es_tupleTable);

                                               
        onconfl->oc_ProjInfo = ExecBuildProjectionInfoExt(onconflset, econtext, onconfl->oc_ProjSlot, false, &mtstate->ps, partrelDesc);

           
                                                                      
                                                                      
                                                                     
                                       
           
        if (node->onConflictWhere)
        {
          List *clause;

          clause = copyObject((List *)node->onConflictWhere);
          clause = (List *)map_variable_attnos((Node *)clause, INNER_VAR, 0, part_attnos, RelationGetDescr(firstResultRel)->natts, RelationGetForm(partrel)->reltype, &found_whole_row);
                                                       
          clause = (List *)map_variable_attnos((Node *)clause, firstVarno, 0, part_attnos, RelationGetDescr(firstResultRel)->natts, RelationGetForm(partrel)->reltype, &found_whole_row);
                                                       
          onconfl->oc_WhereClause = ExecInitQual((List *)clause, &mtstate->ps);
        }
      }
    }
  }

     
                                                                           
                                                                            
     
                                                                          
                                                                       
             
     
  MemoryContextSwitchTo(estate->es_query_cxt);
  estate->es_tuple_routing_result_relations = lappend(estate->es_tuple_routing_result_relations, leaf_part_rri);

  MemoryContextSwitchTo(oldcxt);

  return leaf_part_rri;
}

   
                       
                                                                  
                                                                        
                              
   
static void
ExecInitRoutingInfo(ModifyTableState *mtstate, EState *estate, PartitionTupleRouting *proute, PartitionDispatch dispatch, ResultRelInfo *partRelInfo, int partidx)
{
  ResultRelInfo *rootRelInfo = partRelInfo->ri_RootResultRelInfo;
  MemoryContext oldcxt;
  PartitionRoutingInfo *partrouteinfo;
  int rri_index;

  oldcxt = MemoryContextSwitchTo(proute->memcxt);

  partrouteinfo = palloc(sizeof(PartitionRoutingInfo));

     
                                                                    
                                                          
     
  partrouteinfo->pi_RootToPartitionMap = convert_tuples_by_name(RelationGetDescr(rootRelInfo->ri_RelationDesc), RelationGetDescr(partRelInfo->ri_RelationDesc), gettext_noop("could not convert row type"));

     
                                                                             
                                                                            
                                                                           
                              
     
  if (partrouteinfo->pi_RootToPartitionMap != NULL)
  {
    Relation partrel = partRelInfo->ri_RelationDesc;

       
                                                                 
                                                                          
                           
       
    partrouteinfo->pi_PartitionTupleSlot = table_slot_create(partrel, &estate->es_tupleTable);
  }
  else
  {
    partrouteinfo->pi_PartitionTupleSlot = NULL;
  }

     
                                                                            
                                                             
     
  if (mtstate && (mtstate->mt_transition_capture || mtstate->mt_oc_transition_capture))
  {
    partrouteinfo->pi_PartitionToRootMap = convert_tuples_by_name(RelationGetDescr(partRelInfo->ri_RelationDesc), RelationGetDescr(rootRelInfo->ri_RelationDesc), gettext_noop("could not convert row type"));
  }
  else
  {
    partrouteinfo->pi_PartitionToRootMap = NULL;
  }

     
                                                                      
                                      
     
  if (partRelInfo->ri_FdwRoutine != NULL && partRelInfo->ri_FdwRoutine->BeginForeignInsert != NULL)
  {
    partRelInfo->ri_FdwRoutine->BeginForeignInsert(mtstate, partRelInfo);
  }

  partRelInfo->ri_PartitionInfo = partrouteinfo;
  partRelInfo->ri_CopyMultiInsertBuffer = NULL;

     
                                                                      
     
  Assert(dispatch->indexes[partidx] == -1);

  rri_index = proute->num_partitions++;

                                                
  if (proute->num_partitions >= proute->max_partitions)
  {
    if (proute->max_partitions == 0)
    {
      proute->max_partitions = 8;
      proute->partitions = (ResultRelInfo **)palloc(sizeof(ResultRelInfo *) * proute->max_partitions);
    }
    else
    {
      proute->max_partitions *= 2;
      proute->partitions = (ResultRelInfo **)repalloc(proute->partitions, sizeof(ResultRelInfo *) * proute->max_partitions);
    }
  }

  proute->partitions[rri_index] = partRelInfo;
  dispatch->indexes[partidx] = rri_index;

  MemoryContextSwitchTo(oldcxt);
}

   
                                 
                                                                      
                                                                       
                                                                        
                                                                          
                                                                           
                             
   
static PartitionDispatch
ExecInitPartitionDispatchInfo(EState *estate, PartitionTupleRouting *proute, Oid partoid, PartitionDispatch parent_pd, int partidx, ResultRelInfo *rootResultRelInfo)
{
  Relation rel;
  PartitionDesc partdesc;
  PartitionDispatch pd;
  int dispatchidx;
  MemoryContext oldcxt;

  if (estate->es_partition_directory == NULL)
  {
    estate->es_partition_directory = CreatePartitionDirectory(estate->es_query_cxt);
  }

  oldcxt = MemoryContextSwitchTo(proute->memcxt);

     
                                                                   
                                                                           
                         
     
  if (partoid != RelationGetRelid(proute->partition_root))
  {
    rel = table_open(partoid, RowExclusiveLock);
  }
  else
  {
    rel = proute->partition_root;
  }
  partdesc = PartitionDirectoryLookup(estate->es_partition_directory, rel);

  pd = (PartitionDispatch)palloc(offsetof(PartitionDispatchData, indexes) + partdesc->nparts * sizeof(int));
  pd->reldesc = rel;
  pd->key = RelationGetPartitionKey(rel);
  pd->keystate = NIL;
  pd->partdesc = partdesc;
  if (parent_pd != NULL)
  {
    TupleDesc tupdesc = RelationGetDescr(rel);

       
                                                                          
                                                                         
                                                                           
                                                                         
                                                                          
                                                                   
                
       
    pd->tupmap = convert_tuples_by_name_map_if_req(RelationGetDescr(parent_pd->reldesc), tupdesc, gettext_noop("could not convert row type"));
    pd->tupslot = pd->tupmap ? MakeSingleTupleTableSlot(tupdesc, &TTSOpsVirtual) : NULL;
  }
  else
  {
                                                     
    pd->tupmap = NULL;
    pd->tupslot = NULL;
  }

     
                                                                      
                                                                  
     
  memset(pd->indexes, -1, sizeof(int) * partdesc->nparts);

                                                    
  dispatchidx = proute->num_dispatch++;

                                                
  if (proute->num_dispatch >= proute->max_dispatch)
  {
    if (proute->max_dispatch == 0)
    {
      proute->max_dispatch = 4;
      proute->partition_dispatch_info = (PartitionDispatch *)palloc(sizeof(PartitionDispatch) * proute->max_dispatch);
      proute->nonleaf_partitions = (ResultRelInfo **)palloc(sizeof(ResultRelInfo *) * proute->max_dispatch);
    }
    else
    {
      proute->max_dispatch *= 2;
      proute->partition_dispatch_info = (PartitionDispatch *)repalloc(proute->partition_dispatch_info, sizeof(PartitionDispatch) * proute->max_dispatch);
      proute->nonleaf_partitions = (ResultRelInfo **)repalloc(proute->nonleaf_partitions, sizeof(ResultRelInfo *) * proute->max_dispatch);
    }
  }
  proute->partition_dispatch_info[dispatchidx] = pd;

     
                                                                           
                                                                          
                                        
     
  if (parent_pd)
  {
    ResultRelInfo *rri = makeNode(ResultRelInfo);

    InitResultRelInfo(rri, rel, 0, rootResultRelInfo, 0);
    proute->nonleaf_partitions[dispatchidx] = rri;
  }
  else
  {
    proute->nonleaf_partitions[dispatchidx] = NULL;
  }

     
                                                                             
                                                              
     
  if (parent_pd)
  {
    Assert(parent_pd->indexes[partidx] == -1);
    parent_pd->indexes[partidx] = dispatchidx;
  }

  MemoryContextSwitchTo(oldcxt);

  return pd;
}

   
                                                                             
            
   
                                                                         
   
void
ExecCleanupTupleRouting(ModifyTableState *mtstate, PartitionTupleRouting *proute)
{
  HTAB *htab = proute->subplan_resultrel_htab;
  int i;

     
                                                                          
                                                                          
                                                                           
                                                                   
                        
     
  for (i = 1; i < proute->num_dispatch; i++)
  {
    PartitionDispatch pd = proute->partition_dispatch_info[i];

    table_close(pd->reldesc, NoLock);

    if (pd->tupslot)
    {
      ExecDropSingleTupleTableSlot(pd->tupslot);
    }
  }

  for (i = 0; i < proute->num_partitions; i++)
  {
    ResultRelInfo *resultRelInfo = proute->partitions[i];

                                     
    if (resultRelInfo->ri_FdwRoutine != NULL && resultRelInfo->ri_FdwRoutine->EndForeignInsert != NULL)
    {
      resultRelInfo->ri_FdwRoutine->EndForeignInsert(mtstate->ps.state, resultRelInfo);
    }

       
                                                                         
                                             
       
    if (htab)
    {
      Oid partoid;
      bool found;

      partoid = RelationGetRelid(resultRelInfo->ri_RelationDesc);

      (void)hash_search(htab, &partoid, HASH_FIND, &found);
      if (found)
      {
        continue;
      }
    }

    ExecCloseIndices(resultRelInfo);
    table_close(resultRelInfo->ri_RelationDesc, NoLock);
  }
}

                    
                          
                                                                  
                 
   
                                                            
                                                         
                                                            
                                      
                                                        
                                                      
   
                                                                            
                             
                    
   
static void
FormPartitionKeyDatum(PartitionDispatch pd, TupleTableSlot *slot, EState *estate, Datum *values, bool *isnull)
{
  ListCell *partexpr_item;
  int i;

  if (pd->key->partexprs != NIL && pd->keystate == NIL)
  {
                                                   
    Assert(estate != NULL && GetPerTupleExprContext(estate)->ecxt_scantuple == slot);

                                                                
    pd->keystate = ExecPrepareExprList(pd->key->partexprs, estate);
  }

  partexpr_item = list_head(pd->keystate);
  for (i = 0; i < pd->key->partnatts; i++)
  {
    AttrNumber keycol = pd->key->partattrs[i];
    Datum datum;
    bool isNull;

    if (keycol != 0)
    {
                                                                    
      datum = slot_getattr(slot, keycol, &isNull);
    }
    else
    {
                                           
      if (partexpr_item == NULL)
      {
        elog(ERROR, "wrong number of partition key expressions");
      }
      datum = ExecEvalExprSwitchContext((ExprState *)lfirst(partexpr_item), GetPerTupleExprContext(estate), &isNull);
      partexpr_item = lnext(partexpr_item);
    }
    values[i] = datum;
    isnull[i] = isNull;
  }

  if (partexpr_item != NULL)
  {
    elog(ERROR, "wrong number of partition key expressions");
  }
}

   
                           
                                                                          
                         
   
                                                                               
                              
   
static int
get_partition_for_tuple(PartitionDispatch pd, Datum *values, bool *isnull)
{
  int bound_offset;
  int part_index = -1;
  PartitionKey key = pd->key;
  PartitionDesc partdesc = pd->partdesc;
  PartitionBoundInfo boundinfo = partdesc->boundinfo;

                                                            
  switch (key->strategy)
  {
  case PARTITION_STRATEGY_HASH:
  {
    uint64 rowHash;

    rowHash = compute_partition_hash_value(key->partnatts, key->partsupfunc, key->partcollation, values, isnull);

    part_index = boundinfo->indexes[rowHash % boundinfo->nindexes];
  }
  break;

  case PARTITION_STRATEGY_LIST:
    if (isnull[0])
    {
      if (partition_bound_accepts_nulls(boundinfo))
      {
        part_index = boundinfo->null_index;
      }
    }
    else
    {
      bool equal = false;

      bound_offset = partition_list_bsearch(key->partsupfunc, key->partcollation, boundinfo, values[0], &equal);
      if (bound_offset >= 0 && equal)
      {
        part_index = boundinfo->indexes[bound_offset];
      }
    }
    break;

  case PARTITION_STRATEGY_RANGE:
  {
    bool equal = false, range_partkey_has_null = false;
    int i;

       
                                                               
                                                                  
       
    for (i = 0; i < key->partnatts; i++)
    {
      if (isnull[i])
      {
        range_partkey_has_null = true;
        break;
      }
    }

    if (!range_partkey_has_null)
    {
      bound_offset = partition_range_datum_bsearch(key->partsupfunc, key->partcollation, boundinfo, key->partnatts, values, &equal);

         
                                                                
                                                            
                                                            
                              
         
      part_index = boundinfo->indexes[bound_offset + 1];
    }
  }
  break;

  default:
    elog(ERROR, "unexpected partition strategy: %d", (int)key->strategy);
  }

     
                                                                            
                                             
     
  if (part_index < 0)
  {
    part_index = boundinfo->default_index;
  }

  return part_index;
}

   
                                        
   
                                                                           
                                                                           
                        
   
static char *
ExecBuildSlotPartitionKeyDescription(Relation rel, Datum *values, bool *isnull, int maxfieldlen)
{
  StringInfoData buf;
  PartitionKey key = RelationGetPartitionKey(rel);
  int partnatts = get_partition_natts(key);
  int i;
  Oid relid = RelationGetRelid(rel);
  AclResult aclresult;

  if (check_enable_rls(relid, InvalidOid, true) == RLS_ENABLED)
  {
    return NULL;
  }

                                                                          
  aclresult = pg_class_aclcheck(relid, GetUserId(), ACL_SELECT);
  if (aclresult != ACLCHECK_OK)
  {
       
                                                                       
                                              
       
    for (i = 0; i < partnatts; i++)
    {
      AttrNumber attnum = get_partition_col_attnum(key, i);

         
                                                                     
                                                                 
                                                                        
         
      if (attnum == InvalidAttrNumber || pg_attribute_aclcheck(relid, attnum, GetUserId(), ACL_SELECT) != ACLCHECK_OK)
      {
        return NULL;
      }
    }
  }

  initStringInfo(&buf);
  appendStringInfo(&buf, "(%s) = (", pg_get_partkeydef_columns(relid, true));

  for (i = 0; i < partnatts; i++)
  {
    char *val;
    int vallen;

    if (isnull[i])
    {
      val = "null";
    }
    else
    {
      Oid foutoid;
      bool typisvarlena;

      getTypeOutputInfo(get_partition_col_typid(key, i), &foutoid, &typisvarlena);
      val = OidOutputFunctionCall(foutoid, values[i]);
    }

    if (i > 0)
    {
      appendStringInfoString(&buf, ", ");
    }

                            
    vallen = strlen(val);
    if (vallen <= maxfieldlen)
    {
      appendStringInfoString(&buf, val);
    }
    else
    {
      vallen = pg_mbcliplen(val, vallen, maxfieldlen);
      appendBinaryStringInfo(&buf, val, vallen);
      appendStringInfoString(&buf, "...");
    }
  }

  appendStringInfoChar(&buf, ')');

  return buf.data;
}

   
                          
                                                                         
                                                                      
   
                                                                         
                                                                          
                                                    
   
                                                                         
                                                                
   
static List *
adjust_partition_tlist(List *tlist, TupleConversionMap *map)
{
  List *new_tlist = NIL;
  TupleDesc tupdesc = map->outdesc;
  AttrNumber *attrMap = map->attrMap;
  AttrNumber attrno;
  ListCell *lc;

  for (attrno = 1; attrno <= tupdesc->natts; attrno++)
  {
    Form_pg_attribute att_tup = TupleDescAttr(tupdesc, attrno - 1);
    AttrNumber parentattrno = attrMap[attrno - 1];
    TargetEntry *tle;

    if (parentattrno != InvalidAttrNumber)
    {
         
                                                                        
                                                   
         
      Assert(!att_tup->attisdropped);
      tle = (TargetEntry *)list_nth(tlist, parentattrno - 1);
      Assert(!tle->resjunk);
      Assert(tle->resno == parentattrno);
      tle->resno = attrno;
    }
    else
    {
         
                                                                    
                                                                       
                                              
         
      Const *expr;

      Assert(att_tup->attisdropped);
      expr = makeConst(INT4OID, -1, InvalidOid, sizeof(int32), (Datum)0, true,             
          true            );
      tle = makeTargetEntry((Expr *)expr, attrno, pstrdup(NameStr(att_tup->attname)), false);
    }

    new_tlist = lappend(new_tlist, tle);
  }

                                                                       
  foreach (lc, tlist)
  {
    TargetEntry *tle = (TargetEntry *)lfirst(lc);

    if (tle->resjunk)
    {
      tle->resno = list_length(new_tlist) + 1;
      new_tlist = lappend(new_tlist, tle);
    }
  }

  return new_tlist;
}

                                                                            
                                       
   
                                                                              
                                                                              
                                                                            
                                                 
   
                                                                           
                                                                            
                                                                          
                                                                         
                
   
                                                                     
                                                                            
                                                                           
                                                                    
                                                                            
                                                                               
                                                                         
   
                                                                            
                                                                           
   
   
              
   
                                  
                                                                        
                                                                      
                                                                 
   
                                    
                                                                          
                                                                        
                                                                        
                                                                         
                                                                     
                                                                          
                                                                    
                                                                      
                           
   
                             
                                                                        
                                                                        
                                                                  
                                                  
                                                                            
   

   
                                 
                                                  
                                                                  
   
                                                          
   
                                                                
                                                                               
                                                                               
                                                              
                                                                           
                                                                             
                                                                               
                                                                          
                                                                             
                                             
   
PartitionPruneState *
ExecCreatePartitionPruneState(PlanState *planstate, PartitionPruneInfo *partitionpruneinfo)
{
  EState *estate = planstate->state;
  PartitionPruneState *prunestate;
  int n_part_hierarchies;
  ListCell *lc;
  int i;

  if (estate->es_partition_directory == NULL)
  {
    estate->es_partition_directory = CreatePartitionDirectory(estate->es_query_cxt);
  }

  n_part_hierarchies = list_length(partitionpruneinfo->prune_infos);
  Assert(n_part_hierarchies > 0);

     
                                 
     
  prunestate = (PartitionPruneState *)palloc(offsetof(PartitionPruneState, partprunedata) + sizeof(PartitionPruningData *) * n_part_hierarchies);

  prunestate->execparamids = NULL;
                                                                     
  prunestate->other_subplans = bms_copy(partitionpruneinfo->other_subplans);
  prunestate->do_initial_prune = false;                       
  prunestate->do_exec_prune = false;                          
  prunestate->num_partprunedata = n_part_hierarchies;

     
                                                                             
                                                                          
                                                                             
                  
     
  prunestate->prune_context = AllocSetContextCreate(CurrentMemoryContext, "Partition Prune", ALLOCSET_DEFAULT_SIZES);

  i = 0;
  foreach (lc, partitionpruneinfo->prune_infos)
  {
    List *partrelpruneinfos = lfirst_node(List, lc);
    int npartrelpruneinfos = list_length(partrelpruneinfos);
    PartitionPruningData *prunedata;
    ListCell *lc2;
    int j;

    prunedata = (PartitionPruningData *)palloc(offsetof(PartitionPruningData, partrelprunedata) + npartrelpruneinfos * sizeof(PartitionedRelPruningData));
    prunestate->partprunedata[i] = prunedata;
    prunedata->num_partrelprunedata = npartrelpruneinfos;

    j = 0;
    foreach (lc2, partrelpruneinfos)
    {
      PartitionedRelPruneInfo *pinfo = lfirst_node(PartitionedRelPruneInfo, lc2);
      PartitionedRelPruningData *pprune = &prunedata->partrelprunedata[j];
      Relation partrel;
      PartitionDesc partdesc;
      PartitionKey partkey;

         
                                                                        
                                                                       
                                                                 
                                        
         
      partrel = ExecGetRangeTableRelation(estate, pinfo->rtindex);
      partkey = RelationGetPartitionKey(partrel);
      partdesc = PartitionDirectoryLookup(estate->es_partition_directory, partrel);

         
                                                                        
                                                                        
                                                                         
                                                      
         
      Assert(partdesc->nparts >= pinfo->nparts);
      pprune->nparts = partdesc->nparts;
      pprune->subplan_map = palloc(sizeof(int) * partdesc->nparts);
      if (partdesc->nparts == pinfo->nparts)
      {
           
                                                                   
                                                                      
                                                              
           
        pprune->subpart_map = pinfo->subpart_map;
        memcpy(pprune->subplan_map, pinfo->subplan_map, sizeof(int) * pinfo->nparts);

           
                                                                    
                                                                 
           
#ifdef USE_ASSERT_CHECKING
        for (int k = 0; k < pinfo->nparts; k++)
        {
          Assert(partdesc->oids[k] == pinfo->relid_map[k] || pinfo->subplan_map[k] == -1);
        }
#endif
      }
      else
      {
        int pd_idx = 0;
        int pp_idx;

           
                                                                  
                                                                 
                                                                
                                                                    
                                                                    
                                                                    
                                                         
           
                                                                       
                                                                     
                                                                     
                                                               
                                                                     
                                                   
           
        pprune->subpart_map = palloc(sizeof(int) * partdesc->nparts);
        for (pp_idx = 0; pp_idx < partdesc->nparts; pp_idx++)
        {
                                                     
          while (pd_idx < pinfo->nparts && !OidIsValid(pinfo->relid_map[pd_idx]))
          {
            pd_idx++;
          }

          if (pd_idx < pinfo->nparts && pinfo->relid_map[pd_idx] == partdesc->oids[pp_idx])
          {
                          
            pprune->subplan_map[pp_idx] = pinfo->subplan_map[pd_idx];
            pprune->subpart_map[pp_idx] = pinfo->subpart_map[pd_idx];
            pd_idx++;
          }
          else
          {
                                                        
            pprune->subplan_map[pp_idx] = -1;
            pprune->subpart_map[pp_idx] = -1;
          }
        }

           
                                                                      
                                                                       
                                                                       
                                                               
                                                 
           
        if (pd_idx != pinfo->nparts)
        {
          elog(ERROR, "could not match partition child tables to plan elements");
        }
      }

                                                               
      pprune->present_parts = bms_copy(pinfo->present_parts);

         
                                                
         
      pprune->initial_pruning_steps = pinfo->initial_pruning_steps;
      if (pinfo->initial_pruning_steps)
      {
        ExecInitPruningContext(&pprune->initial_context, pinfo->initial_pruning_steps, partdesc, partkey, planstate);
                                                                   
        prunestate->do_initial_prune = true;
      }
      pprune->exec_pruning_steps = pinfo->exec_pruning_steps;
      if (pinfo->exec_pruning_steps)
      {
        ExecInitPruningContext(&pprune->exec_context, pinfo->exec_pruning_steps, partdesc, partkey, planstate);
                                                                
        prunestate->do_exec_prune = true;
      }

         
                                                                   
                                                   
         
      prunestate->execparamids = bms_add_members(prunestate->execparamids, pinfo->execparamids);

      j++;
    }
    i++;
  }

  return prunestate;
}

   
                                                                           
   
static void
ExecInitPruningContext(PartitionPruneContext *context, List *pruning_steps, PartitionDesc partdesc, PartitionKey partkey, PlanState *planstate)
{
  int n_steps;
  int partnatts;
  ListCell *lc;

  n_steps = list_length(pruning_steps);

  context->strategy = partkey->strategy;
  context->partnatts = partnatts = partkey->partnatts;
  context->nparts = partdesc->nparts;
  context->boundinfo = partdesc->boundinfo;
  context->partcollation = partkey->partcollation;
  context->partsupfunc = partkey->partsupfunc;

                                                               
  context->stepcmpfuncs = (FmgrInfo *)palloc0(sizeof(FmgrInfo) * n_steps * partnatts);

  context->ppccontext = CurrentMemoryContext;
  context->planstate = planstate;

                                                               
  context->exprstates = (ExprState **)palloc0(sizeof(ExprState *) * n_steps * partnatts);
  foreach (lc, pruning_steps)
  {
    PartitionPruneStepOp *step = (PartitionPruneStepOp *)lfirst(lc);
    ListCell *lc2;
    int keyno;

                                         
    if (!IsA(step, PartitionPruneStepOp))
    {
      continue;
    }

    Assert(list_length(step->exprs) <= partnatts);

    keyno = 0;
    foreach (lc2, step->exprs)
    {
      Expr *expr = (Expr *)lfirst(lc2);

                                 
      if (!IsA(expr, Const))
      {
        int stateidx = PruneCxtStateIdx(partnatts, step->step.step_id, keyno);

        context->exprstates[stateidx] = ExecInitExpr(expr, context->planstate);
      }
      keyno++;
    }
  }
}

   
                                   
                                                                      
                                                                       
            
   
                                                                        
                                                                            
                                                                         
                                 
   
                                                                          
                
   
                                                                        
   
Bitmapset *
ExecFindInitialMatchingSubPlans(PartitionPruneState *prunestate, int nsubplans)
{
  Bitmapset *result = NULL;
  MemoryContext oldcontext;
  int i;

                                                            
  Assert(prunestate->do_initial_prune);

     
                                                                        
                                    
     
  oldcontext = MemoryContextSwitchTo(prunestate->prune_context);

     
                                                                    
                                    
     
  for (i = 0; i < prunestate->num_partprunedata; i++)
  {
    PartitionPruningData *prunedata;
    PartitionedRelPruningData *pprune;

    prunedata = prunestate->partprunedata[i];
    pprune = &prunedata->partrelprunedata[0];

                                                         
    find_matching_subplans_recurse(prunedata, pprune, true, &result);

                                                                          
    if (pprune->initial_pruning_steps)
    {
      ResetExprContext(pprune->initial_context.planstate->ps_ExprContext);
    }
  }

                                                                     
  result = bms_add_members(result, prunestate->other_subplans);

  MemoryContextSwitchTo(oldcontext);

                                                              
  result = bms_copy(result);

  MemoryContextReset(prunestate->prune_context);

     
                                                                            
                                                                           
                                                                            
                                 
     
                                                                          
                                                                            
                                                      
     
  if (prunestate->do_exec_prune && bms_num_members(result) < nsubplans)
  {
    int *new_subplan_indexes;
    Bitmapset *new_other_subplans;
    int i;
    int newidx;

       
                                                                    
                                                                       
                                                                  
       
    new_subplan_indexes = (int *)palloc0(sizeof(int) * nsubplans);
    newidx = 1;
    i = -1;
    while ((i = bms_next_member(result, i)) >= 0)
    {
      Assert(i < nsubplans);
      new_subplan_indexes[i] = newidx++;
    }

       
                                                                         
                                                                      
               
       
    for (i = 0; i < prunestate->num_partprunedata; i++)
    {
      PartitionPruningData *prunedata = prunestate->partprunedata[i];
      int j;

         
                                                                      
                                                                       
                                                                   
                                                                       
                                                                
         
      for (j = prunedata->num_partrelprunedata - 1; j >= 0; j--)
      {
        PartitionedRelPruningData *pprune = &prunedata->partrelprunedata[j];
        int nparts = pprune->nparts;
        int k;

                                                        
        bms_free(pprune->present_parts);
        pprune->present_parts = NULL;

        for (k = 0; k < nparts; k++)
        {
          int oldidx = pprune->subplan_map[k];
          int subidx;

             
                                                                    
                                                                  
                                                                    
                                                                    
                                                                  
                                                                 
                                   
             
          if (oldidx >= 0)
          {
            Assert(oldidx < nsubplans);
            pprune->subplan_map[k] = new_subplan_indexes[oldidx] - 1;

            if (new_subplan_indexes[oldidx] > 0)
            {
              pprune->present_parts = bms_add_member(pprune->present_parts, k);
            }
          }
          else if ((subidx = pprune->subpart_map[k]) >= 0)
          {
            PartitionedRelPruningData *subprune;

            subprune = &prunedata->partrelprunedata[subidx];

            if (!bms_is_empty(subprune->present_parts))
            {
              pprune->present_parts = bms_add_member(pprune->present_parts, k);
            }
          }
        }
      }
    }

       
                                                                          
                   
       
    new_other_subplans = NULL;
    i = -1;
    while ((i = bms_next_member(prunestate->other_subplans, i)) >= 0)
    {
      new_other_subplans = bms_add_member(new_other_subplans, new_subplan_indexes[i] - 1);
    }

    bms_free(prunestate->other_subplans);
    prunestate->other_subplans = new_other_subplans;

    pfree(new_subplan_indexes);
  }

  return result;
}

   
                            
                                                                 
                                                               
   
                                                     
   
Bitmapset *
ExecFindMatchingSubPlans(PartitionPruneState *prunestate)
{
  Bitmapset *result = NULL;
  MemoryContext oldcontext;
  int i;

     
                                                   
                                                                      
                                             
     
  Assert(prunestate->do_exec_prune);

     
                                                                        
                                    
     
  oldcontext = MemoryContextSwitchTo(prunestate->prune_context);

     
                                                                    
                                    
     
  for (i = 0; i < prunestate->num_partprunedata; i++)
  {
    PartitionPruningData *prunedata;
    PartitionedRelPruningData *pprune;

    prunedata = prunestate->partprunedata[i];
    pprune = &prunedata->partrelprunedata[0];

    find_matching_subplans_recurse(prunedata, pprune, false, &result);

                                                                          
    if (pprune->exec_pruning_steps)
    {
      ResetExprContext(pprune->exec_context.planstate->ps_ExprContext);
    }
  }

                                                                     
  result = bms_add_members(result, prunestate->other_subplans);

  MemoryContextSwitchTo(oldcontext);

                                                              
  result = bms_copy(result);

  MemoryContextReset(prunestate->prune_context);

  return result;
}

   
                                  
                                                               
                                    
   
                                                           
   
static void
find_matching_subplans_recurse(PartitionPruningData *prunedata, PartitionedRelPruningData *pprune, bool initial_prune, Bitmapset **validsubplans)
{
  Bitmapset *partset;
  int i;

                                                                            
  check_stack_depth();

                                                            
  if (initial_prune && pprune->initial_pruning_steps)
  {
    partset = get_matching_partitions(&pprune->initial_context, pprune->initial_pruning_steps);
  }
  else if (!initial_prune && pprune->exec_pruning_steps)
  {
    partset = get_matching_partitions(&pprune->exec_context, pprune->exec_pruning_steps);
  }
  else
  {
       
                                                                        
              
       
    partset = pprune->present_parts;
  }

                                              
  i = -1;
  while ((i = bms_next_member(partset, i)) >= 0)
  {
    if (pprune->subplan_map[i] >= 0)
    {
      *validsubplans = bms_add_member(*validsubplans, pprune->subplan_map[i]);
    }
    else
    {
      int partidx = pprune->subpart_map[i];

      if (partidx >= 0)
      {
        find_matching_subplans_recurse(prunedata, &prunedata->partrelprunedata[partidx], initial_prune, validsubplans);
      }
      else
      {
           
                                                                  
                                                                
                                                                   
                                                                  
                                                                   
           
      }
    }
  }
}
