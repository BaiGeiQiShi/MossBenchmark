                                                                            
   
             
                                                 
   
   
                                                                         
                                                                        
   
   
                  
                                          
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/nbtree.h"
#include "access/tableam.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/transam.h"
#include "access/xlog.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/heap.h"
#include "catalog/pg_am.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_statistic_ext.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/supportnodes.h"
#include "optimizer/clauses.h"
#include "optimizer/cost.h"
#include "optimizer/optimizer.h"
#include "optimizer/plancat.h"
#include "optimizer/prep.h"
#include "partitioning/partdesc.h"
#include "parser/parse_relation.h"
#include "parser/parsetree.h"
#include "rewrite/rewriteManip.h"
#include "statistics/statistics.h"
#include "storage/bufmgr.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/syscache.h"
#include "utils/snapmgr.h"

                   
int constraint_exclusion = CONSTRAINT_EXCLUSION_PARTITION;

                                                            
get_relation_info_hook_type get_relation_info_hook = NULL;

static void
get_relation_foreign_keys(PlannerInfo *root, RelOptInfo *rel, Relation relation, bool inhparent);
static bool
infer_collation_opclass_match(InferenceElem *elem, Relation idxRel, List *idxExprs);
static List *
get_relation_constraints(PlannerInfo *root, Oid relationObjectId, RelOptInfo *rel, bool include_noinherit, bool include_notnull, bool include_partition);
static List *
build_index_tlist(PlannerInfo *root, IndexOptInfo *index, Relation heapRelation);
static List *
get_relation_statistics(RelOptInfo *rel, Relation relation);
static void
set_relation_partition_info(PlannerInfo *root, RelOptInfo *rel, Relation relation);
static PartitionScheme
find_partition_scheme(PlannerInfo *root, Relation rel);
static void
set_baserel_partition_key_exprs(Relation relation, RelOptInfo *rel);

   
                       
                                                         
   
                                                                        
                             
   
                                    
                                     
                                                          
                                                                      
                                                    
                                                                 
                          
                            
                                                                
   
                                                                               
   
                                                                         
                                                                         
                                                                     
   
                                                                      
                                                                             
                                                                           
                     
   
void
get_relation_info(PlannerInfo *root, Oid relationObjectId, bool inhparent, RelOptInfo *rel)
{
  Index varno = rel->relid;
  Relation relation;
  bool hasindex;
  List *indexinfos = NIL;

     
                                                                          
                                                                             
                 
     
  relation = table_open(relationObjectId, NoLock);

     
                                                                             
                                                                             
                                                                       
                                                                
     
  if (!relation->rd_tableam)
  {
    if (!(relation->rd_rel->relkind == RELKIND_FOREIGN_TABLE || relation->rd_rel->relkind == RELKIND_PARTITIONED_TABLE))
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("cannot open relation \"%s\"", RelationGetRelationName(relation))));
    }
  }

                                                                          
  if (!RelationNeedsWAL(relation) && RecoveryInProgress())
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot access temporary or unlogged relations during recovery")));
  }

  rel->min_attr = FirstLowInvalidHeapAttributeNumber + 1;
  rel->max_attr = RelationGetNumberOfAttributes(relation);
  rel->reltablespace = RelationGetForm(relation)->reltablespace;

  Assert(rel->max_attr >= rel->min_attr);
  rel->attr_needed = (Relids *)palloc0((rel->max_attr - rel->min_attr + 1) * sizeof(Relids));
  rel->attr_widths = (int32 *)palloc0((rel->max_attr - rel->min_attr + 1) * sizeof(int32));

     
                                                                            
                                                                         
                                                                        
     
  if (!inhparent)
  {
    estimate_rel_size(relation, rel->attr_widths - rel->min_attr, &rel->pages, &rel->tuples, &rel->allvisfrac);
  }

                                                                  
  rel->rel_parallel_workers = RelationGetParallelWorkers(relation, -1);

     
                                                                          
                                                                  
     
  if (inhparent || (IgnoreSystemIndexes && IsSystemRelation(relation)))
  {
    hasindex = false;
  }
  else
  {
    hasindex = relation->rd_rel->relhasindex;
  }

  if (hasindex)
  {
    List *indexoidlist;
    LOCKMODE lmode;
    ListCell *l;

    indexoidlist = RelationGetIndexList(relation);

       
                                                                           
                                                                         
                                                               
                                                                        
                                                                         
                                                             
       
    lmode = root->simple_rte_array[varno]->rellockmode;

    foreach (l, indexoidlist)
    {
      Oid indexoid = lfirst_oid(l);
      Relation indexRelation;
      Form_pg_index index;
      IndexAmRoutine *amroutine;
      IndexOptInfo *info;
      int ncolumns, nkeycolumns;
      int i;

         
                                                                  
         
      indexRelation = index_open(indexoid, lmode);
      index = indexRelation->rd_index;

         
                                                                     
                                                                      
                                                                       
                                                                         
                     
         
      if (!index->indisvalid)
      {
        index_close(indexRelation, NoLock);
        continue;
      }

         
                                                                   
                  
         
      if (indexRelation->rd_rel->relkind == RELKIND_PARTITIONED_INDEX)
      {
        index_close(indexRelation, NoLock);
        continue;
      }

         
                                                                       
                                                           
                                                            
         
      if (index->indcheckxmin && !TransactionIdPrecedes(HeapTupleHeaderGetXmin(indexRelation->rd_indextuple->t_data), TransactionXmin))
      {
        root->glob->transientPlan = true;
        index_close(indexRelation, NoLock);
        continue;
      }

      info = makeNode(IndexOptInfo);

      info->indexoid = index->indexrelid;
      info->reltablespace = RelationGetForm(indexRelation)->reltablespace;
      info->rel = rel;
      info->ncolumns = ncolumns = index->indnatts;
      info->nkeycolumns = nkeycolumns = index->indnkeyatts;

      info->indexkeys = (int *)palloc(sizeof(int) * ncolumns);
      info->indexcollations = (Oid *)palloc(sizeof(Oid) * nkeycolumns);
      info->opfamily = (Oid *)palloc(sizeof(Oid) * nkeycolumns);
      info->opcintype = (Oid *)palloc(sizeof(Oid) * nkeycolumns);
      info->canreturn = (bool *)palloc(sizeof(bool) * ncolumns);

      for (i = 0; i < ncolumns; i++)
      {
        info->indexkeys[i] = index->indkey.values[i];
        info->canreturn[i] = index_can_return(indexRelation, i + 1);
      }

      for (i = 0; i < nkeycolumns; i++)
      {
        info->opfamily[i] = indexRelation->rd_opfamily[i];
        info->opcintype[i] = indexRelation->rd_opcintype[i];
        info->indexcollations[i] = indexRelation->rd_indcollation[i];
      }

      info->relam = indexRelation->rd_rel->relam;

                                                                
      amroutine = indexRelation->rd_indam;
      info->amcanorderbyop = amroutine->amcanorderbyop;
      info->amoptionalkey = amroutine->amoptionalkey;
      info->amsearcharray = amroutine->amsearcharray;
      info->amsearchnulls = amroutine->amsearchnulls;
      info->amcanparallel = amroutine->amcanparallel;
      info->amhasgettuple = (amroutine->amgettuple != NULL);
      info->amhasgetbitmap = amroutine->amgetbitmap != NULL && relation->rd_tableam->scan_bitmap_next_block != NULL;
      info->amcanmarkpos = (amroutine->ammarkpos != NULL && amroutine->amrestrpos != NULL);
      info->amcostestimate = amroutine->amcostestimate;
      Assert(info->amcostestimate != NULL);

         
                                                               
         
      if (info->relam == BTREE_AM_OID)
      {
           
                                                               
                                                        
           
        Assert(amroutine->amcanorder);

        info->sortopfamily = info->opfamily;
        info->reverse_sort = (bool *)palloc(sizeof(bool) * nkeycolumns);
        info->nulls_first = (bool *)palloc(sizeof(bool) * nkeycolumns);

        for (i = 0; i < nkeycolumns; i++)
        {
          int16 opt = indexRelation->rd_indoption[i];

          info->reverse_sort[i] = (opt & INDOPTION_DESC) != 0;
          info->nulls_first[i] = (opt & INDOPTION_NULLS_FIRST) != 0;
        }
      }
      else if (amroutine->amcanorder)
      {
           
                                                                     
                                                                       
                                                                      
                              
           
                                                                
                                                                      
                                                                       
                                                                      
                                                                      
                                                                      
                                               
           
        info->sortopfamily = (Oid *)palloc(sizeof(Oid) * nkeycolumns);
        info->reverse_sort = (bool *)palloc(sizeof(bool) * nkeycolumns);
        info->nulls_first = (bool *)palloc(sizeof(bool) * nkeycolumns);

        for (i = 0; i < nkeycolumns; i++)
        {
          int16 opt = indexRelation->rd_indoption[i];
          Oid ltopr;
          Oid btopfamily;
          Oid btopcintype;
          int16 btstrategy;

          info->reverse_sort[i] = (opt & INDOPTION_DESC) != 0;
          info->nulls_first[i] = (opt & INDOPTION_NULLS_FIRST) != 0;

          ltopr = get_opfamily_member(info->opfamily[i], info->opcintype[i], info->opcintype[i], BTLessStrategyNumber);
          if (OidIsValid(ltopr) && get_ordering_op_properties(ltopr, &btopfamily, &btopcintype, &btstrategy) && btopcintype == info->opcintype[i] && btstrategy == BTLessStrategyNumber)
          {
                                    
            info->sortopfamily[i] = btopfamily;
          }
          else
          {
                                                           
            info->sortopfamily = NULL;
            info->reverse_sort = NULL;
            info->nulls_first = NULL;
            break;
          }
        }
      }
      else
      {
        info->sortopfamily = NULL;
        info->reverse_sort = NULL;
        info->nulls_first = NULL;
      }

         
                                                                     
                                                                   
                                                                      
                                         
         
      info->indexprs = RelationGetIndexExpressions(indexRelation);
      info->indpred = RelationGetIndexPredicate(indexRelation);
      if (info->indexprs && varno != 1)
      {
        ChangeVarNodes((Node *)info->indexprs, 1, varno, 0);
      }
      if (info->indpred && varno != 1)
      {
        ChangeVarNodes((Node *)info->indpred, 1, varno, 0);
      }

                                                              
      info->indextlist = build_index_tlist(root, info, relation);

      info->indrestrictinfo = NIL;                               
      info->predOK = false;                                      
      info->unique = index->indisunique;
      info->immediate = index->indimmediate;
      info->hypothetical = false;

         
                                                                        
                                                                        
                                                                         
                                                                     
                         
         
      if (info->indpred == NIL)
      {
        info->pages = RelationGetNumberOfBlocks(indexRelation);
        info->tuples = rel->tuples;
      }
      else
      {
        double allvisfrac;            

        estimate_rel_size(indexRelation, NULL, &info->pages, &info->tuples, &allvisfrac);
        if (info->tuples > rel->tuples)
        {
          info->tuples = rel->tuples;
        }
      }

      if (info->relam == BTREE_AM_OID)
      {
                                                                      
        info->tree_height = _bt_getrootheight(indexRelation);
      }
      else
      {
                                                                     
        info->tree_height = -1;
      }

      index_close(indexRelation, NoLock);

      indexinfos = lcons(info, indexinfos);
    }

    list_free(indexoidlist);
  }

  rel->indexlist = indexinfos;

  rel->statlist = get_relation_statistics(rel, relation);

                                                                    
  if (relation->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
  {
    rel->serverid = GetForeignServerIdByRelId(RelationGetRelid(relation));
    rel->fdwroutine = GetFdwRoutineForRelation(relation, true);
  }
  else
  {
    rel->serverid = InvalidOid;
    rel->fdwroutine = NULL;
  }

                                                               
  get_relation_foreign_keys(root, rel, relation, inhparent);

     
                                                                     
                                             
     
  if (inhparent && relation->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    set_relation_partition_info(root, rel, relation);
  }

  table_close(relation, NoLock);

     
                                                                     
                                                                          
                                                                         
     
  if (get_relation_info_hook)
  {
    (*get_relation_info_hook)(root, relationObjectId, inhparent, rel);
  }
}

   
                               
                                                             
   
                                                                         
                                                                           
                                                                           
                                                                         
                                                       
   
static void
get_relation_foreign_keys(PlannerInfo *root, RelOptInfo *rel, Relation relation, bool inhparent)
{
  List *rtable = root->parse->rtable;
  List *cachedfkeys;
  ListCell *lc;

     
                                                                             
                                                                            
                                           
     
  if (rel->reloptkind != RELOPT_BASEREL || list_length(rtable) < 2)
  {
    return;
  }

     
                                                                          
                                                                         
                                                                         
                                                  
     
  if (inhparent)
  {
    return;
  }

     
                                                                          
                                                                           
                                                                     
     
  cachedfkeys = RelationGetFKeyList(relation);

     
                                                                     
                                                                        
                                                                        
                                                                           
                                                          
     
                                                                             
                                                                             
                                                                           
                 
     
  foreach (lc, cachedfkeys)
  {
    ForeignKeyCacheInfo *cachedfk = (ForeignKeyCacheInfo *)lfirst(lc);
    Index rti;
    ListCell *lc2;

                                                                       
    Assert(cachedfk->conrelid == RelationGetRelid(relation));

                                                    
    rti = 0;
    foreach (lc2, rtable)
    {
      RangeTblEntry *rte = (RangeTblEntry *)lfirst(lc2);
      ForeignKeyOptInfo *info;

      rti++;
                                           
      if (rte->rtekind != RTE_RELATION || rte->relid != cachedfk->confrelid)
      {
        continue;
      }
                                                                      
      if (rte->inh)
      {
        continue;
      }
                                                                 
      if (rti == rel->relid)
      {
        continue;
      }

                                   
      info = makeNode(ForeignKeyOptInfo);
      info->con_relid = rel->relid;
      info->ref_relid = rti;
      info->nkeys = cachedfk->nkeys;
      memcpy(info->conkey, cachedfk->conkey, sizeof(info->conkey));
      memcpy(info->confkey, cachedfk->confkey, sizeof(info->confkey));
      memcpy(info->conpfeqop, cachedfk->conpfeqop, sizeof(info->conpfeqop));
                                                                       
      info->nmatched_ec = 0;
      info->nmatched_rcols = 0;
      info->nmatched_ri = 0;
      memset(info->eclass, 0, sizeof(info->eclass));
      memset(info->rinfos, 0, sizeof(info->rinfos));

      root->fkey_list = lappend(root->fkey_list, info);
    }
  }
}

   
                           
                                                                           
   
                                                                            
                                                                            
                                                                               
                                                                               
                                                                              
                                                                       
             
   
                                                                        
                                                                              
                                                                       
                                                                               
                                                                               
                                                                           
                                                             
   
List *
infer_arbiter_indexes(PlannerInfo *root)
{
  OnConflictExpr *onconflict = root->parse->onConflict;

                       
  RangeTblEntry *rte;
  Relation relation;
  Oid indexOidFromConstraint = InvalidOid;
  List *indexList;
  ListCell *l;

                                                                  
  Bitmapset *inferAttrs = NULL;
  List *inferElems = NIL;

               
  List *results = NIL;

     
                                                                        
                                                                          
                                                                           
                    
     
  if (onconflict->arbiterElems == NIL && onconflict->constraint == InvalidOid)
  {
    return NIL;
  }

     
                                                                          
                                                                             
                 
     
  rte = rt_fetch(root->parse->resultRelation, root->parse->rtable);

  relation = table_open(rte->relid, NoLock);

     
                                                                         
                                                                            
                                          
     
  foreach (l, onconflict->arbiterElems)
  {
    InferenceElem *elem = (InferenceElem *)lfirst(l);
    Var *var;
    int attno;

    if (!IsA(elem->expr, Var))
    {
                                                                   
      inferElems = lappend(inferElems, elem->expr);
      continue;
    }

    var = (Var *)elem->expr;
    attno = var->varattno;

    if (attno == 0)
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("whole row unique index inference specifications are not supported")));
    }

    inferAttrs = bms_add_member(inferAttrs, attno - FirstLowInvalidHeapAttributeNumber);
  }

     
                                                                        
                                                         
     
  if (onconflict->constraint != InvalidOid)
  {
    indexOidFromConstraint = get_constraint_index(onconflict->constraint);

    if (indexOidFromConstraint == InvalidOid)
    {
      ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("constraint in ON CONFLICT clause has no associated index")));
    }
  }

     
                                                                           
                                             
     
  indexList = RelationGetIndexList(relation);

  foreach (l, indexList)
  {
    Oid indexoid = lfirst_oid(l);
    Relation idxRel;
    Form_pg_index idxForm;
    Bitmapset *indexedAttrs;
    List *idxExprs;
    List *predExprs;
    AttrNumber natt;
    ListCell *el;

       
                                                                        
                                                                 
       
                                                                        
                                                                           
                
       
    idxRel = index_open(indexoid, rte->rellockmode);
    idxForm = idxRel->rd_index;

    if (!idxForm->indisvalid)
    {
      goto next;
    }

       
                                                                           
                                                                  
                                                                        
                       
       

       
                                                                        
                                                               
       
    if (indexOidFromConstraint == idxForm->indexrelid)
    {
      if (!idxForm->indisunique && onconflict->action == ONCONFLICT_UPDATE)
      {
        ereport(ERROR, (errcode(ERRCODE_WRONG_OBJECT_TYPE), errmsg("ON CONFLICT DO UPDATE not supported with exclusion constraints")));
      }

      results = lappend_oid(results, idxForm->indexrelid);
      list_free(indexList);
      index_close(idxRel, NoLock);
      table_close(relation, NoLock);
      return results;
    }
    else if (indexOidFromConstraint != InvalidOid)
    {
                                                                       
      goto next;
    }

       
                                                                        
                                                                     
                                  
       
    if (!idxForm->indisunique)
    {
      goto next;
    }

                                                                        
    indexedAttrs = NULL;
    for (natt = 0; natt < idxForm->indnkeyatts; natt++)
    {
      int attno = idxRel->rd_index->indkey.values[natt];

      if (attno != 0)
      {
        indexedAttrs = bms_add_member(indexedAttrs, attno - FirstLowInvalidHeapAttributeNumber);
      }
    }

                                                       
    if (!bms_equal(indexedAttrs, inferAttrs))
    {
      goto next;
    }

                                                   
    idxExprs = RelationGetIndexExpressions(idxRel);
    foreach (el, onconflict->arbiterElems)
    {
      InferenceElem *elem = (InferenceElem *)lfirst(el);

         
                                                                       
                                                                      
                                                                      
                                                                 
                                                     
         
      if (!infer_collation_opclass_match(elem, idxRel, idxExprs))
      {
        goto next;
      }

         
                                                                        
                                                               
                                                             
         
      if (IsA(elem->expr, Var))
      {
        continue;
      }

         
                                                                     
                                                                      
                                                                       
                           
         
      if (elem->infercollid != InvalidOid || elem->inferopclass != InvalidOid || list_member(idxExprs, elem->expr))
      {
        continue;
      }

      goto next;
    }

       
                                                                     
                                                                     
                                                                     
                                                                       
                                                                     
       
    if (list_difference(idxExprs, inferElems) != NIL)
    {
      goto next;
    }

       
                                                                        
                                
       
    predExprs = RelationGetIndexPredicate(idxRel);

    if (!predicate_implied_by(predExprs, (List *)onconflict->arbiterWhere, false))
    {
      goto next;
    }

    results = lappend_oid(results, idxForm->indexrelid);
  next:
    index_close(idxRel, NoLock);
  }

  list_free(indexList);
  table_close(relation, NoLock);

  if (results == NIL)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_COLUMN_REFERENCE), errmsg("there is no unique or exclusion constraint matching the ON CONFLICT specification")));
  }

  return results;
}

   
                                                                                
   
                                                                         
                                                                              
                                                                              
                                                                               
                                                                              
                                            
   
                                                                           
                                                                        
                                                  
   
                                                                              
                                                                      
                                                                         
                                                                      
                                                                               
                                                                              
                                                                   
   
                                                                             
                                                                           
                                                                           
                       
   
static bool
infer_collation_opclass_match(InferenceElem *elem, Relation idxRel, List *idxExprs)
{
  AttrNumber natt;
  Oid inferopfamily = InvalidOid;                                  
  Oid inferopcinputtype = InvalidOid;                                
  int nplain = 0;                                                 

     
                                                                         
                                    
     
  if (elem->infercollid == InvalidOid && elem->inferopclass == InvalidOid)
  {
    return true;
  }

     
                                                          
     
  if (elem->inferopclass)
  {
    inferopfamily = get_opclass_family(elem->inferopclass);
    inferopcinputtype = get_opclass_input_type(elem->inferopclass);
  }

  for (natt = 1; natt <= idxRel->rd_att->natts; natt++)
  {
    Oid opfamily = idxRel->rd_opfamily[natt - 1];
    Oid opcinputtype = idxRel->rd_opcintype[natt - 1];
    Oid collation = idxRel->rd_indcollation[natt - 1];
    int attno = idxRel->rd_index->indkey.values[natt - 1];

    if (attno != 0)
    {
      nplain++;
    }

    if (elem->inferopclass != InvalidOid && (inferopfamily != opfamily || inferopcinputtype != opcinputtype))
    {
                                                         
      continue;
    }

    if (elem->infercollid != InvalidOid && elem->infercollid != collation)
    {
                                                           
      continue;
    }

                                                                     
    if (IsA(elem->expr, Var))
    {
      if (((Var *)elem->expr)->varattno == attno)
      {
        return true;
      }
    }
    else if (attno == 0)
    {
      Node *nattExpr = list_nth(idxExprs, (natt - 1) - nplain);

         
                                                                    
                                                                  
                                                                  
         
      if (equal(elem->expr, nattExpr))
      {
        return true;
      }
    }
  }

  return false;
}

   
                                                                         
   
                                                                             
                                                                  
   
                                                                       
                                                                              
                                                 
   
void
estimate_rel_size(Relation rel, int32 *attr_widths, BlockNumber *pages, double *tuples, double *allvisfrac)
{
  BlockNumber curpages;
  BlockNumber relpages;
  double reltuples;
  BlockNumber relallvisible;
  double density;

  switch (rel->rd_rel->relkind)
  {
  case RELKIND_RELATION:
  case RELKIND_MATVIEW:
  case RELKIND_TOASTVALUE:
    table_relation_estimate_size(rel, attr_widths, pages, tuples, allvisfrac);
    break;

  case RELKIND_INDEX:

       
                                                                
                                                                 
       

                                             
    curpages = RelationGetNumberOfBlocks(rel);

                                                           
    relpages = (BlockNumber)rel->rd_rel->relpages;
    reltuples = (double)rel->rd_rel->reltuples;
    relallvisible = (BlockNumber)rel->rd_rel->relallvisible;

                                  
    *pages = curpages;
                                            
    if (curpages == 0)
    {
      *tuples = 0;
      *allvisfrac = 0;
      break;
    }
                                                           
    relpages = (BlockNumber)rel->rd_rel->relpages;
    reltuples = (double)rel->rd_rel->reltuples;
    relallvisible = (BlockNumber)rel->rd_rel->relallvisible;

       
                                                                    
                                                                      
                                                                    
                                             
       
    if (relpages > 0)
    {
      curpages--;
      relpages--;
    }

                                                               
    if (relpages > 0)
    {
      density = reltuples / (double)relpages;
    }
    else
    {
         
                                                                  
                                                                   
                                                                  
                                                                     
                                                               
                                                               
                              
         
                                                            
                                                                    
                                                                   
                                                                   
                                               
         
                                                        
         
      int32 tuple_width;

      tuple_width = get_rel_data_width(rel, attr_widths);
      tuple_width += MAXALIGN(SizeofHeapTupleHeader);
      tuple_width += sizeof(ItemIdData);
                                                      
      density = (BLCKSZ - SizeOfPageHeaderData) / tuple_width;
    }
    *tuples = rint(density * (double)curpages);

       
                                                                     
                                                                  
                                                                    
                                                                      
       
    if (relallvisible == 0 || curpages <= 0)
    {
      *allvisfrac = 0;
    }
    else if ((double)relallvisible >= curpages)
    {
      *allvisfrac = 1;
    }
    else
    {
      *allvisfrac = (double)relallvisible / curpages;
    }
    break;

  case RELKIND_SEQUENCE:
                                            
    *pages = 1;
    *tuples = 1;
    *allvisfrac = 0;
    break;
  case RELKIND_FOREIGN_TABLE:
                                         
    *pages = rel->rd_rel->relpages;
    *tuples = rel->rd_rel->reltuples;
    *allvisfrac = 0;
    break;
  default:
                                                                   
    *pages = 0;
    *tuples = 0;
    *allvisfrac = 0;
    break;
  }
}

   
                      
   
                                                                           
   
                                                                       
                                                                             
   
                                                                          
                                                                         
                                                                        
                                       
   
int32
get_rel_data_width(Relation rel, int32 *attr_widths)
{
  int32 tuple_width = 0;
  int i;

  for (i = 1; i <= RelationGetNumberOfAttributes(rel); i++)
  {
    Form_pg_attribute att = TupleDescAttr(rel->rd_att, i - 1);
    int32 item_width;

    if (att->attisdropped)
    {
      continue;
    }

                                            
    if (attr_widths != NULL && attr_widths[i] > 0)
    {
      tuple_width += attr_widths[i];
      continue;
    }

                                                         
    item_width = get_attavgwidth(RelationGetRelid(rel), i);
    if (item_width <= 0)
    {
      item_width = get_typavgwidth(att->atttypid, att->atttypmod);
      Assert(item_width > 0);
    }
    if (attr_widths != NULL)
    {
      attr_widths[i] = item_width;
    }
    tuple_width += item_width;
  }

  return tuple_width;
}

   
                           
   
                                                                        
                            
   
int32
get_relation_data_width(Oid relid, int32 *attr_widths)
{
  int32 result;
  Relation relation;

                                                   
  relation = table_open(relid, NoLock);

  result = get_rel_data_width(relation, attr_widths);

  table_close(relation, NoLock);

  return result;
}

   
                            
   
                                                                         
   
                                                                        
                                                                      
                                                                      
                                             
   
                                                                       
                          
   
                                                                           
                                                                     
   
                                                                  
                                              
   
                                                                          
                                                                        
                                            
   
static List *
get_relation_constraints(PlannerInfo *root, Oid relationObjectId, RelOptInfo *rel, bool include_noinherit, bool include_notnull, bool include_partition)
{
  List *result = NIL;
  Index varno = rel->relid;
  Relation relation;
  TupleConstr *constr;

     
                                                            
     
  relation = table_open(relationObjectId, NoLock);

  constr = relation->rd_att->constr;
  if (constr != NULL)
  {
    int num_check = constr->num_check;
    int i;

    for (i = 0; i < num_check; i++)
    {
      Node *cexpr;

         
                                                                     
                                                                        
                           
         
      if (!constr->check[i].ccvalid)
      {
        continue;
      }
      if (constr->check[i].ccnoinherit && !include_noinherit)
      {
        continue;
      }

      cexpr = stringToNode(constr->check[i].ccbin);

         
                                                              
                                                                     
                                                       
                                                                        
                                                                       
                                                                    
                                                                       
                                
         
      cexpr = eval_const_expressions(root, cexpr);

      cexpr = (Node *)canonicalize_qual((Expr *)cexpr, true);

                                              
      if (varno != 1)
      {
        ChangeVarNodes(cexpr, 1, varno, 0);
      }

         
                                                                       
                                                          
         
      result = list_concat(result, make_ands_implicit((Expr *)cexpr));
    }

                                                                   
    if (include_notnull && constr->has_not_null)
    {
      int natts = relation->rd_att->natts;

      for (i = 1; i <= natts; i++)
      {
        Form_pg_attribute att = TupleDescAttr(relation->rd_att, i - 1);

        if (att->attnotnull && !att->attisdropped)
        {
          NullTest *ntest = makeNode(NullTest);

          ntest->arg = (Expr *)makeVar(varno, i, att->atttypid, att->atttypmod, att->attcollation, 0);
          ntest->nulltesttype = IS_NOT_NULL;

             
                                                                    
                                                                     
                                                                   
             
          ntest->argisrow = false;
          ntest->location = -1;
          result = lappend(result, ntest);
        }
      }
    }
  }

     
                                                 
     
  if (include_partition && relation->rd_rel->relispartition)
  {
    List *pcqual = RelationGetPartitionQual(relation);

    if (pcqual)
    {
         
                                                                         
                                                                        
                                                                    
                                                                
                                                                      
         
      pcqual = (List *)eval_const_expressions(root, (Node *)pcqual);

                                              
      if (varno != 1)
      {
        ChangeVarNodes((Node *)pcqual, 1, varno, 0);
      }

      result = list_concat(result, pcqual);
    }
  }

  table_close(relation, NoLock);

  return result;
}

   
                           
                                                       
   
                                                                          
                                                                            
                                                                             
   
static List *
get_relation_statistics(RelOptInfo *rel, Relation relation)
{
  List *statoidlist;
  List *stainfos = NIL;
  ListCell *l;

  statoidlist = RelationGetStatExtList(relation);

  foreach (l, statoidlist)
  {
    Oid statOid = lfirst_oid(l);
    Form_pg_statistic_ext staForm;
    HeapTuple htup;
    HeapTuple dtup;
    Bitmapset *keys = NULL;
    int i;

    htup = SearchSysCache1(STATEXTOID, ObjectIdGetDatum(statOid));
    if (!HeapTupleIsValid(htup))
    {
      elog(ERROR, "cache lookup failed for statistics object %u", statOid);
    }
    staForm = (Form_pg_statistic_ext)GETSTRUCT(htup);

    dtup = SearchSysCache1(STATEXTDATASTXOID, ObjectIdGetDatum(statOid));
    if (!HeapTupleIsValid(dtup))
    {
      elog(ERROR, "cache lookup failed for statistics object %u", statOid);
    }

       
                                                                      
                                                                          
                                                       
       
    for (i = 0; i < staForm->stxkeys.dim1; i++)
    {
      keys = bms_add_member(keys, staForm->stxkeys.values[i]);
    }

                                                      
    if (statext_is_kind_built(dtup, STATS_EXT_NDISTINCT))
    {
      StatisticExtInfo *info = makeNode(StatisticExtInfo);

      info->statOid = statOid;
      info->rel = rel;
      info->kind = STATS_EXT_NDISTINCT;
      info->keys = bms_copy(keys);

      stainfos = lcons(info, stainfos);
    }

    if (statext_is_kind_built(dtup, STATS_EXT_DEPENDENCIES))
    {
      StatisticExtInfo *info = makeNode(StatisticExtInfo);

      info->statOid = statOid;
      info->rel = rel;
      info->kind = STATS_EXT_DEPENDENCIES;
      info->keys = bms_copy(keys);

      stainfos = lcons(info, stainfos);
    }

    if (statext_is_kind_built(dtup, STATS_EXT_MCV))
    {
      StatisticExtInfo *info = makeNode(StatisticExtInfo);

      info->statOid = statOid;
      info->rel = rel;
      info->kind = STATS_EXT_MCV;
      info->keys = bms_copy(keys);

      stainfos = lcons(info, stainfos);
    }

    ReleaseSysCache(htup);
    ReleaseSysCache(dtup);
    bms_free(keys);
  }

  list_free(statoidlist);

  return stainfos;
}

   
                                    
   
                                                                         
                                                                         
                                      
   
                                                             
                                                                       
                                   
   
bool
relation_excluded_by_constraints(PlannerInfo *root, RelOptInfo *rel, RangeTblEntry *rte)
{
  bool include_noinherit;
  bool include_notnull;
  bool include_partition = false;
  List *safe_restrictions;
  List *constraint_pred;
  List *safe_constraints;
  ListCell *lc;

                                                                         
  Assert(IS_SIMPLE_REL(rel));

     
                                                                          
                                          
     
  if (rel->baserestrictinfo == NIL)
  {
    return false;
  }

     
                                                               
                                                                             
                                                                       
                                                                           
                                                                          
                                                                            
                               
     
  if (list_length(rel->baserestrictinfo) == 1)
  {
    RestrictInfo *rinfo = (RestrictInfo *)linitial(rel->baserestrictinfo);
    Expr *clause = rinfo->clause;

    if (clause && IsA(clause, Const) && (((Const *)clause)->constisnull || !DatumGetBool(((Const *)clause)->constvalue)))
    {
      return true;
    }
  }

     
                                                            
     
  switch (constraint_exclusion)
  {
  case CONSTRAINT_EXCLUSION_OFF:
                                                     
    return false;

  case CONSTRAINT_EXCLUSION_PARTITION:

       
                                                                      
                                                                      
                                                                     
                                                                  
                                                              
                             
       
                                                                      
                                                                    
       
    if (rel->reloptkind == RELOPT_OTHER_MEMBER_REL || (rel->relid == root->parse->resultRelation && root->inhTargetKind != INHKIND_NONE))
    {
      break;                                      
    }
    return false;

  case CONSTRAINT_EXCLUSION_ON:

       
                                                                   
                                                               
                                                                      
                                                                 
                                            
       
    if (rel->reloptkind == RELOPT_BASEREL && !(rel->relid == root->parse->resultRelation && root->inhTargetKind != INHKIND_NONE))
    {
      include_partition = true;
    }
    break;                            
  }

     
                                                                         
                                                                             
                                                                          
     
                                                                       
                                                     
     
  safe_restrictions = NIL;
  foreach (lc, rel->baserestrictinfo)
  {
    RestrictInfo *rinfo = (RestrictInfo *)lfirst(lc);

    if (!contain_mutable_functions((Node *)rinfo->clause))
    {
      safe_restrictions = lappend(safe_restrictions, rinfo->clause);
    }
  }

     
                                                                        
                                       
     
  if (predicate_refuted_by(safe_restrictions, safe_restrictions, true))
  {
    return true;
  }

     
                                                                             
     
  if (rte->rtekind != RTE_RELATION)
  {
    return false;
  }

     
                                                                            
                                                                         
                                                                             
                                  
     
  include_noinherit = !rte->inh;

     
                                                                            
                                                                  
                                                                      
     
  include_notnull = (!rte->inh || rte->relkind == RELKIND_PARTITIONED_TABLE);

     
                                                          
     
  constraint_pred = get_relation_constraints(root, rte->relid, rel, include_noinherit, include_notnull, include_partition);

     
                                                                     
                                                                           
                                                                             
                                                                            
                                
     
  safe_constraints = NIL;
  foreach (lc, constraint_pred)
  {
    Node *pred = (Node *)lfirst(lc);

    if (!contain_mutable_functions(pred))
    {
      safe_constraints = lappend(safe_constraints, pred);
    }
  }

     
                                                                           
                                                                             
                                                   
     
                                                                             
                                                                            
                                                                   
                                            
     
                                                                             
                                       
     
  if (predicate_refuted_by(safe_constraints, rel->baserestrictinfo, false))
  {
    return true;
  }

  return false;
}

   
                        
   
                                                                            
                                                                              
                                                                         
   
                                                                              
                                                                        
                                                                              
                                                                             
                                                                        
                                                                       
                                                                          
                 
   
                                                                          
                                                                              
                                                                        
                                                 
   
List *
build_physical_tlist(PlannerInfo *root, RelOptInfo *rel)
{
  List *tlist = NIL;
  Index varno = rel->relid;
  RangeTblEntry *rte = planner_rt_fetch(varno, root);
  Relation relation;
  Query *subquery;
  Var *var;
  ListCell *l;
  int attrno, numattrs;
  List *colvars;

  switch (rte->rtekind)
  {
  case RTE_RELATION:
                                              
    relation = table_open(rte->relid, NoLock);

    numattrs = RelationGetNumberOfAttributes(relation);
    for (attrno = 1; attrno <= numattrs; attrno++)
    {
      Form_pg_attribute att_tup = TupleDescAttr(relation->rd_att, attrno - 1);

      if (att_tup->attisdropped || att_tup->atthasmissing)
      {
                                                     
        tlist = NIL;
        break;
      }

      var = makeVar(varno, attrno, att_tup->atttypid, att_tup->atttypmod, att_tup->attcollation, 0);

      tlist = lappend(tlist, makeTargetEntry((Expr *)var, attrno, NULL, false));
    }

    table_close(relation, NoLock);
    break;

  case RTE_SUBQUERY:
    subquery = rte->subquery;
    foreach (l, subquery->targetList)
    {
      TargetEntry *tle = (TargetEntry *)lfirst(l);

         
                                                              
                                                          
         
      var = makeVarFromTargetEntry(varno, tle);

      tlist = lappend(tlist, makeTargetEntry((Expr *)var, tle->resno, NULL, tle->resjunk));
    }
    break;

  case RTE_FUNCTION:
  case RTE_TABLEFUNC:
  case RTE_VALUES:
  case RTE_CTE:
  case RTE_NAMEDTUPLESTORE:
  case RTE_RESULT:
                                                                       
    expandRTE(rte, varno, 0, -1, true                      , NULL, &colvars);
    foreach (l, colvars)
    {
      var = (Var *)lfirst(l);

         
                                                                 
                    
         
      if (!IsA(var, Var))
      {
        tlist = NIL;
        break;
      }

      tlist = lappend(tlist, makeTargetEntry((Expr *)var, var->varattno, NULL, false));
    }
    break;

  default:
                      
    elog(ERROR, "unsupported RTE kind %d in build_physical_tlist", (int)rte->rtekind);
    break;
  }

  return tlist;
}

   
                     
   
                                                                       
                                                                           
                                                                   
   
                                                             
                                                  
   
static List *
build_index_tlist(PlannerInfo *root, IndexOptInfo *index, Relation heapRelation)
{
  List *tlist = NIL;
  Index varno = index->rel->relid;
  ListCell *indexpr_item;
  int i;

  indexpr_item = list_head(index->indexprs);
  for (i = 0; i < index->ncolumns; i++)
  {
    int indexkey = index->indexkeys[i];
    Expr *indexvar;

    if (indexkey != 0)
    {
                         
      const FormData_pg_attribute *att_tup;

      if (indexkey < 0)
      {
        att_tup = SystemAttributeDefinition(indexkey);
      }
      else
      {
        att_tup = TupleDescAttr(heapRelation->rd_att, indexkey - 1);
      }

      indexvar = (Expr *)makeVar(varno, indexkey, att_tup->atttypid, att_tup->atttypmod, att_tup->attcollation, 0);
    }
    else
    {
                             
      if (indexpr_item == NULL)
      {
        elog(ERROR, "wrong number of index expressions");
      }
      indexvar = (Expr *)lfirst(indexpr_item);
      indexpr_item = lnext(indexpr_item);
    }

    tlist = lappend(tlist, makeTargetEntry(indexvar, i + 1, NULL, false));
  }
  if (indexpr_item != NULL)
  {
    elog(ERROR, "wrong number of index expressions");
  }

  return tlist;
}

   
                           
   
                                                                       
                                                          
                                                       
   
                                                                          
   
Selectivity
restriction_selectivity(PlannerInfo *root, Oid operatorid, List *args, Oid inputcollid, int varRelid)
{
  RegProcedure oprrest = get_oprrest(operatorid);
  float8 result;

     
                                                                    
                        
     
  if (!oprrest)
  {
    return (Selectivity)0.5;
  }

  result = DatumGetFloat8(OidFunctionCall4Coll(oprrest, inputcollid, PointerGetDatum(root), ObjectIdGetDatum(operatorid), PointerGetDatum(args), Int32GetDatum(varRelid)));

  if (result < 0.0 || result > 1.0)
  {
    elog(ERROR, "invalid restriction selectivity: %f", result);
  }

  return (Selectivity)result;
}

   
                    
   
                                                                
                                                          
                                                       
   
                                                                          
   
Selectivity
join_selectivity(PlannerInfo *root, Oid operatorid, List *args, Oid inputcollid, JoinType jointype, SpecialJoinInfo *sjinfo)
{
  RegProcedure oprjoin = get_oprjoin(operatorid);
  float8 result;

     
                                                                    
                        
     
  if (!oprjoin)
  {
    return (Selectivity)0.5;
  }

  result = DatumGetFloat8(OidFunctionCall5Coll(oprjoin, inputcollid, PointerGetDatum(root), ObjectIdGetDatum(operatorid), PointerGetDatum(args), Int16GetDatum(jointype), PointerGetDatum(sjinfo)));

  if (result < 0.0 || result > 1.0)
  {
    elog(ERROR, "invalid join selectivity: %f", result);
  }

  return (Selectivity)result;
}

   
                        
   
                                                                   
                                                          
                                                      
   
                                                                          
   
Selectivity
function_selectivity(PlannerInfo *root, Oid funcid, List *args, Oid inputcollid, bool is_join, int varRelid, JoinType jointype, SpecialJoinInfo *sjinfo)
{
  RegProcedure prosupport = get_func_support(funcid);
  SupportRequestSelectivity req;
  SupportRequestSelectivity *sresult;

     
                                                                    
                                                                        
                                                                          
                                                                          
                                      
     
  if (!prosupport)
  {
    return (Selectivity)0.3333333;
  }

  req.type = T_SupportRequestSelectivity;
  req.root = root;
  req.funcid = funcid;
  req.args = args;
  req.inputcollid = inputcollid;
  req.is_join = is_join;
  req.varRelid = varRelid;
  req.jointype = jointype;
  req.sjinfo = sjinfo;
  req.selectivity = -1;                                        

  sresult = (SupportRequestSelectivity *)DatumGetPointer(OidFunctionCall1(prosupport, PointerGetDatum(&req)));

                                              
  if (sresult != &req)
  {
    return (Selectivity)0.3333333;
  }

  if (req.selectivity < 0.0 || req.selectivity > 1.0)
  {
    elog(ERROR, "invalid function selectivity: %f", req.selectivity);
  }

  return (Selectivity)req.selectivity;
}

   
                     
   
                                                                        
                                                                      
                                              
   
                                                                     
                                                                  
                                                          
   
                                           
   
void
add_function_cost(PlannerInfo *root, Oid funcid, Node *node, QualCost *cost)
{
  HeapTuple proctup;
  Form_pg_proc procform;

  proctup = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(proctup))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }
  procform = (Form_pg_proc)GETSTRUCT(proctup);

  if (OidIsValid(procform->prosupport))
  {
    SupportRequestCost req;
    SupportRequestCost *sresult;

    req.type = T_SupportRequestCost;
    req.root = root;
    req.funcid = funcid;
    req.node = node;

                                                                         
    req.startup = 0;
    req.per_tuple = 0;

    sresult = (SupportRequestCost *)DatumGetPointer(OidFunctionCall1(procform->prosupport, PointerGetDatum(&req)));

    if (sresult == &req)
    {
                                                                         
      cost->startup += req.startup;
      cost->per_tuple += req.per_tuple;
      ReleaseSysCache(proctup);
      return;
    }
  }

                                                             
  cost->per_tuple += procform->procost * cpu_operator_cost;

  ReleaseSysCache(proctup);
}

   
                     
   
                                                                               
   
                                                                           
                                                                     
                                                  
   
                                           
   
                                                                             
                                                                           
                                    
   
double
get_function_rows(PlannerInfo *root, Oid funcid, Node *node)
{
  HeapTuple proctup;
  Form_pg_proc procform;
  double result;

  proctup = SearchSysCache1(PROCOID, ObjectIdGetDatum(funcid));
  if (!HeapTupleIsValid(proctup))
  {
    elog(ERROR, "cache lookup failed for function %u", funcid);
  }
  procform = (Form_pg_proc)GETSTRUCT(proctup);

  Assert(procform->proretset);                        

  if (OidIsValid(procform->prosupport))
  {
    SupportRequestRows req;
    SupportRequestRows *sresult;

    req.type = T_SupportRequestRows;
    req.root = root;
    req.funcid = funcid;
    req.node = node;

    req.rows = 0;                      

    sresult = (SupportRequestRows *)DatumGetPointer(OidFunctionCall1(procform->prosupport, PointerGetDatum(&req)));

    if (sresult == &req)
    {
                   
      ReleaseSysCache(proctup);
      return req.rows;
    }
  }

                                                             
  result = procform->prorows;

  ReleaseSysCache(proctup);

  return result;
}

   
                    
   
                                                                     
                                                                    
                                                        
   
                                                                         
                                                                         
                                                                           
                                             
   
bool
has_unique_index(RelOptInfo *rel, AttrNumber attno)
{
  ListCell *ilist;

  foreach (ilist, rel->indexlist)
  {
    IndexOptInfo *index = (IndexOptInfo *)lfirst(ilist);

       
                                                                           
                                                                          
                                                                     
                                                                         
                                                                          
                                          
       
    if (index->unique && index->nkeycolumns == 1 && index->indexkeys[0] == attno && (index->indpred == NIL || index->predOK))
    {
      return true;
    }
  }
  return false;
}

   
                    
   
                                                                               
   
bool
has_row_triggers(PlannerInfo *root, Index rti, CmdType event)
{
  RangeTblEntry *rte = planner_rt_fetch(rti, root);
  Relation relation;
  TriggerDesc *trigDesc;
  bool result = false;

                                            
  relation = table_open(rte->relid, NoLock);

  trigDesc = relation->trigdesc;
  switch (event)
  {
  case CMD_INSERT:
    if (trigDesc && (trigDesc->trig_insert_after_row || trigDesc->trig_insert_before_row))
    {
      result = true;
    }
    break;
  case CMD_UPDATE:
    if (trigDesc && (trigDesc->trig_update_after_row || trigDesc->trig_update_before_row))
    {
      result = true;
    }
    break;
  case CMD_DELETE:
    if (trigDesc && (trigDesc->trig_delete_after_row || trigDesc->trig_delete_before_row))
    {
      result = true;
    }
    break;
  default:
    elog(ERROR, "unrecognized CmdType: %d", (int)event);
    break;
  }

  table_close(relation, NoLock);
  return result;
}

bool
has_stored_generated_columns(PlannerInfo *root, Index rti)
{
  RangeTblEntry *rte = planner_rt_fetch(rti, root);
  Relation relation;
  TupleDesc tupdesc;
  bool result = false;

                                            
  relation = heap_open(rte->relid, NoLock);

  tupdesc = RelationGetDescr(relation);
  result = tupdesc->constr && tupdesc->constr->has_generated_stored;

  heap_close(relation, NoLock);

  return result;
}

   
                               
   
                                                                            
   
static void
set_relation_partition_info(PlannerInfo *root, RelOptInfo *rel, Relation relation)
{
  PartitionDesc partdesc;

                                                                         
  if (root->glob->partition_directory == NULL)
  {
    root->glob->partition_directory = CreatePartitionDirectory(CurrentMemoryContext);
  }

  partdesc = PartitionDirectoryLookup(root->glob->partition_directory, relation);
  rel->part_scheme = find_partition_scheme(root, relation);
  Assert(partdesc != NULL && rel->part_scheme != NULL);
  rel->boundinfo = partdesc->boundinfo;
  rel->nparts = partdesc->nparts;
  set_baserel_partition_key_exprs(relation, rel);
  rel->partition_qual = RelationGetPartitionQual(relation);
}

   
                         
   
                                                       
   
static PartitionScheme
find_partition_scheme(PlannerInfo *root, Relation relation)
{
  PartitionKey partkey = RelationGetPartitionKey(relation);
  ListCell *lc;
  int partnatts, i;
  PartitionScheme part_scheme;

                                                        
  Assert(partkey != NULL);

  partnatts = partkey->partnatts;

                                                                       
  foreach (lc, root->part_schemes)
  {
    part_scheme = lfirst(lc);

                                                         
    if (partkey->strategy != part_scheme->strategy || partnatts != part_scheme->partnatts)
    {
      continue;
    }

                                              
    if (memcmp(partkey->partopfamily, part_scheme->partopfamily, sizeof(Oid) * partnatts) != 0 || memcmp(partkey->partopcintype, part_scheme->partopcintype, sizeof(Oid) * partnatts) != 0 || memcmp(partkey->partcollation, part_scheme->partcollation, sizeof(Oid) * partnatts) != 0)
    {
      continue;
    }

       
                                                                    
                
       
    Assert(memcmp(partkey->parttyplen, part_scheme->parttyplen, sizeof(int16) * partnatts) == 0);
    Assert(memcmp(partkey->parttypbyval, part_scheme->parttypbyval, sizeof(bool) * partnatts) == 0);

       
                                                                     
                                                                     
                                                                         
                                                                           
             
       
#ifdef USE_ASSERT_CHECKING
    for (i = 0; i < partkey->partnatts; i++)
    {
      Assert(partkey->partsupfunc[i].fn_oid == part_scheme->partsupfunc[i].fn_oid);
    }
#endif

                                          
    return part_scheme;
  }

     
                                                                         
                                                                        
                                                                             
               
     
  part_scheme = (PartitionScheme)palloc0(sizeof(PartitionSchemeData));
  part_scheme->strategy = partkey->strategy;
  part_scheme->partnatts = partkey->partnatts;

  part_scheme->partopfamily = (Oid *)palloc(sizeof(Oid) * partnatts);
  memcpy(part_scheme->partopfamily, partkey->partopfamily, sizeof(Oid) * partnatts);

  part_scheme->partopcintype = (Oid *)palloc(sizeof(Oid) * partnatts);
  memcpy(part_scheme->partopcintype, partkey->partopcintype, sizeof(Oid) * partnatts);

  part_scheme->partcollation = (Oid *)palloc(sizeof(Oid) * partnatts);
  memcpy(part_scheme->partcollation, partkey->partcollation, sizeof(Oid) * partnatts);

  part_scheme->parttyplen = (int16 *)palloc(sizeof(int16) * partnatts);
  memcpy(part_scheme->parttyplen, partkey->parttyplen, sizeof(int16) * partnatts);

  part_scheme->parttypbyval = (bool *)palloc(sizeof(bool) * partnatts);
  memcpy(part_scheme->parttypbyval, partkey->parttypbyval, sizeof(bool) * partnatts);

  part_scheme->partsupfunc = (FmgrInfo *)palloc(sizeof(FmgrInfo) * partnatts);
  for (i = 0; i < partnatts; i++)
  {
    fmgr_info_copy(&part_scheme->partsupfunc[i], &partkey->partsupfunc[i], CurrentMemoryContext);
  }

                                                   
  root->part_schemes = lappend(root->part_schemes, part_scheme);

  return part_scheme;
}

   
                                   
   
                                                                              
                                                                               
                                                                         
   
static void
set_baserel_partition_key_exprs(Relation relation, RelOptInfo *rel)
{
  PartitionKey partkey = RelationGetPartitionKey(relation);
  int partnatts;
  int cnt;
  List **partexprs;
  ListCell *lc;
  Index varno = rel->relid;

  Assert(IS_SIMPLE_REL(rel) && rel->relid > 0);

                                                        
  Assert(partkey != NULL);

  partnatts = partkey->partnatts;
  partexprs = (List **)palloc(sizeof(List *) * partnatts);
  lc = list_head(partkey->partexprs);

  for (cnt = 0; cnt < partnatts; cnt++)
  {
    Expr *partexpr;
    AttrNumber attno = partkey->partattrs[cnt];

    if (attno != InvalidAttrNumber)
    {
                                                                
      Assert(attno > 0);

      partexpr = (Expr *)makeVar(varno, attno, partkey->parttypid[cnt], partkey->parttypmod[cnt], partkey->parttypcoll[cnt], 0);
    }
    else
    {
      if (lc == NULL)
      {
        elog(ERROR, "wrong number of partition key expressions");
      }

                                                     
      partexpr = (Expr *)copyObject(lfirst(lc));
      ChangeVarNodes((Node *)partexpr, 1, varno, 0);
      lc = lnext(lc);
    }

    partexprs[cnt] = list_make1(partexpr);
  }

  rel->partexprs = partexprs;

     
                                                                         
                                                                           
                                                                             
                                     
     
  rel->nullable_partexprs = (List **)palloc0(sizeof(List *) * partnatts);
}
