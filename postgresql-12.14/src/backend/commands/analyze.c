                                                                            
   
             
                                       
   
                                                                         
                                                                        
   
   
                  
                                    
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "access/genam.h"
#include "access/multixact.h"
#include "access/relation.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/tupconvert.h"
#include "access/tuptoaster.h"
#include "access/visibilitymap.h"
#include "access/xact.h"
#include "catalog/catalog.h"
#include "catalog/index.h"
#include "catalog/indexing.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_statistic_ext.h"
#include "commands/dbcommands.h"
#include "commands/tablecmds.h"
#include "commands/vacuum.h"
#include "executor/executor.h"
#include "foreign/fdwapi.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "parser/parse_oper.h"
#include "parser/parse_relation.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "statistics/extended_stats_internal.h"
#include "statistics/statistics.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/acl.h"
#include "utils/attoptcache.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_rusage.h"
#include "utils/sampling.h"
#include "utils/sortsupport.h"
#include "utils/syscache.h"
#include "utils/timestamp.h"

                                
typedef struct AnlIndexData
{
  IndexInfo *indexInfo;                                   
  double tupleFract;                                                   
  VacAttrStats **vacattrstats;                             
  int attr_cnt;
} AnlIndexData;

                                               
int default_statistics_target = 100;

                                                                        
static MemoryContext anl_context = NULL;
static BufferAccessStrategy vac_strategy;

static void
do_analyze_rel(Relation onerel, VacuumParams *params, List *va_cols, AcquireSampleRowsFunc acquirefunc, BlockNumber relpages, bool inh, bool in_outer_xact, int elevel);
static void
compute_index_stats(Relation onerel, double totalrows, AnlIndexData *indexdata, int nindexes, HeapTuple *rows, int numrows, MemoryContext col_context);
static VacAttrStats *
examine_attribute(Relation onerel, int attnum, Node *index_expr);
static int
acquire_sample_rows(Relation onerel, int elevel, HeapTuple *rows, int targrows, double *totalrows, double *totaldeadrows);
static int
compare_rows(const void *a, const void *b);
static int
acquire_inherited_sample_rows(Relation onerel, int elevel, HeapTuple *rows, int targrows, double *totalrows, double *totaldeadrows);
static void
update_attstats(Oid relid, bool inh, int natts, VacAttrStats **vacattrstats);
static Datum
std_fetch_func(VacAttrStatsP stats, int rownum, bool *isNull);
static Datum
ind_fetch_func(VacAttrStatsP stats, int rownum, bool *isNull);

   
                                         
   
                                                                           
                                                                           
                                                                           
   
void
analyze_rel(Oid relid, RangeVar *relation, VacuumParams *params, List *va_cols, bool in_outer_xact, BufferAccessStrategy bstrategy)
{
  Relation onerel;
  int elevel;
  AcquireSampleRowsFunc acquirefunc = NULL;
  BlockNumber relpages = 0;

                            
  if (params->options & VACOPT_VERBOSE)
  {
    elevel = INFO;
  }
  else
  {
    elevel = DEBUG2;
  }

                               
  vac_strategy = bstrategy;

     
                                     
     
  CHECK_FOR_INTERRUPTS();

     
                                                                            
                                                                    
                                                                          
                                                                           
                                                                         
     
                                                               
     
  onerel = vacuum_open_relation(relid, relation, params->options & ~(VACOPT_VACUUM), params->log_min_duration >= 0, ShareUpdateExclusiveLock);

                                                       
  if (!onerel)
  {
    return;
  }

     
                                                                           
                                                                          
                                                                        
                                                                             
                                                                         
                
     
  if (!vacuum_is_relation_owner(RelationGetRelid(onerel), onerel->rd_rel, params->options & VACOPT_ANALYZE))
  {
    relation_close(onerel, ShareUpdateExclusiveLock);
    return;
  }

     
                                                                       
                                                                           
                                                                          
                                                                 
     
  if (RELATION_IS_OTHER_TEMP(onerel))
  {
    relation_close(onerel, ShareUpdateExclusiveLock);
    return;
  }

     
                                                                       
     
  if (RelationGetRelid(onerel) == StatisticRelationId)
  {
    relation_close(onerel, ShareUpdateExclusiveLock);
    return;
  }

     
                                                                         
     
  if (onerel->rd_rel->relkind == RELKIND_RELATION || onerel->rd_rel->relkind == RELKIND_MATVIEW)
  {
                                                                          
    acquirefunc = acquire_sample_rows;
                                       
    relpages = RelationGetNumberOfBlocks(onerel);
  }
  else if (onerel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
  {
       
                                                                           
                          
       
    FdwRoutine *fdwroutine;
    bool ok = false;

    fdwroutine = GetFdwRoutineForRelation(onerel, false);

    if (fdwroutine->AnalyzeForeignTable != NULL)
    {
      ok = fdwroutine->AnalyzeForeignTable(onerel, &acquirefunc, &relpages);
    }

    if (!ok)
    {
      ereport(WARNING, (errmsg("skipping \"%s\" --- cannot analyze this foreign table", RelationGetRelationName(onerel))));
      relation_close(onerel, ShareUpdateExclusiveLock);
      return;
    }
  }
  else if (onerel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
       
                                                                          
       
  }
  else
  {
                                                                      
    if (!(params->options & VACOPT_VACUUM))
    {
      ereport(WARNING, (errmsg("skipping \"%s\" --- cannot analyze non-tables or special system tables", RelationGetRelationName(onerel))));
    }
    relation_close(onerel, ShareUpdateExclusiveLock);
    return;
  }

     
                                                                     
     
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
  MyPgXact->vacuumFlags |= PROC_IN_ANALYZE;
  LWLockRelease(ProcArrayLock);

     
                                                                            
                                           
     
  if (onerel->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
  {
    do_analyze_rel(onerel, params, va_cols, acquirefunc, relpages, false, in_outer_xact, elevel);
  }

     
                                                      
     
  if (onerel->rd_rel->relhassubclass)
  {
    do_analyze_rel(onerel, params, va_cols, acquirefunc, relpages, true, in_outer_xact, elevel);
  }

     
                                                                        
                                                                             
                                                                            
                                                                  
     
  relation_close(onerel, NoLock);

     
                                                                            
                                                                 
     
  LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
  MyPgXact->vacuumFlags &= ~PROC_IN_ANALYZE;
  LWLockRelease(ProcArrayLock);
}

   
                                                                
   
                                                                        
                                                                          
                                                 
   
static void
do_analyze_rel(Relation onerel, VacuumParams *params, List *va_cols, AcquireSampleRowsFunc acquirefunc, BlockNumber relpages, bool inh, bool in_outer_xact, int elevel)
{
  int attr_cnt, tcnt, i, ind;
  Relation *Irel;
  int nindexes;
  bool hasindex;
  VacAttrStats **vacattrstats;
  AnlIndexData *indexdata;
  int targrows, numrows;
  double totalrows, totaldeadrows;
  HeapTuple *rows;
  PGRUsage ru0;
  TimestampTz starttime = 0;
  MemoryContext caller_context;
  Oid save_userid;
  int save_sec_context;
  int save_nestlevel;

  if (inh)
  {
    ereport(elevel, (errmsg("analyzing \"%s.%s\" inheritance tree", get_namespace_name(RelationGetNamespace(onerel)), RelationGetRelationName(onerel))));
  }
  else
  {
    ereport(elevel, (errmsg("analyzing \"%s.%s\"", get_namespace_name(RelationGetNamespace(onerel)), RelationGetRelationName(onerel))));
  }

     
                                                                            
              
     
  anl_context = AllocSetContextCreate(CurrentMemoryContext, "Analyze", ALLOCSET_DEFAULT_SIZES);
  caller_context = MemoryContextSwitchTo(anl_context);

     
                                                                             
                                                                      
                                                                 
     
  GetUserIdAndSecContext(&save_userid, &save_sec_context);
  SetUserIdAndSecContext(onerel->rd_rel->relowner, save_sec_context | SECURITY_RESTRICTED_OPERATION);
  save_nestlevel = NewGUCNestLevel();

                                                               
  if (IsAutoVacuumWorkerProcess() && params->log_min_duration >= 0)
  {
    pg_rusage_init(&ru0);
    if (params->log_min_duration > 0)
    {
      starttime = GetCurrentTimestamp();
    }
  }

     
                                        
     
                                                                            
                                                                          
                                                                         
                                                                           
     
  if (va_cols != NIL)
  {
    Bitmapset *unique_cols = NULL;
    ListCell *le;

    vacattrstats = (VacAttrStats **)palloc(list_length(va_cols) * sizeof(VacAttrStats *));
    tcnt = 0;
    foreach (le, va_cols)
    {
      char *col = strVal(lfirst(le));

      i = attnameAttNum(onerel, col, false);
      if (i == InvalidAttrNumber)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_COLUMN), errmsg("column \"%s\" of relation \"%s\" does not exist", col, RelationGetRelationName(onerel))));
      }
      if (bms_is_member(i, unique_cols))
      {
        ereport(ERROR, (errcode(ERRCODE_DUPLICATE_COLUMN), errmsg("column \"%s\" of relation \"%s\" appears more than once", col, RelationGetRelationName(onerel))));
      }
      unique_cols = bms_add_member(unique_cols, i);

      vacattrstats[tcnt] = examine_attribute(onerel, i, NULL);
      if (vacattrstats[tcnt] != NULL)
      {
        tcnt++;
      }
    }
    attr_cnt = tcnt;
  }
  else
  {
    attr_cnt = onerel->rd_att->natts;
    vacattrstats = (VacAttrStats **)palloc(attr_cnt * sizeof(VacAttrStats *));
    tcnt = 0;
    for (i = 1; i <= attr_cnt; i++)
    {
      vacattrstats[tcnt] = examine_attribute(onerel, i, NULL);
      if (vacattrstats[tcnt] != NULL)
      {
        tcnt++;
      }
    }
    attr_cnt = tcnt;
  }

     
                                                                           
                                                                           
                                                                         
                                                                            
          
     
  if (!inh)
  {
    vac_open_indexes(onerel, AccessShareLock, &nindexes, &Irel);
  }
  else
  {
    Irel = NULL;
    nindexes = 0;
  }
  hasindex = (nindexes > 0);
  indexdata = NULL;
  if (hasindex)
  {
    indexdata = (AnlIndexData *)palloc0(nindexes * sizeof(AnlIndexData));
    for (ind = 0; ind < nindexes; ind++)
    {
      AnlIndexData *thisdata = &indexdata[ind];
      IndexInfo *indexInfo;

      thisdata->indexInfo = indexInfo = BuildIndexInfo(Irel[ind]);
      thisdata->tupleFract = 1.0;                           
      if (indexInfo->ii_Expressions != NIL && va_cols == NIL)
      {
        ListCell *indexpr_item = list_head(indexInfo->ii_Expressions);

        thisdata->vacattrstats = (VacAttrStats **)palloc(indexInfo->ii_NumIndexAttrs * sizeof(VacAttrStats *));
        tcnt = 0;
        for (i = 0; i < indexInfo->ii_NumIndexAttrs; i++)
        {
          int keycol = indexInfo->ii_IndexAttrNumbers[i];

          if (keycol == 0)
          {
                                           
            Node *indexkey;

            if (indexpr_item == NULL)                       
            {
              elog(ERROR, "too few entries in indexprs list");
            }
            indexkey = (Node *)lfirst(indexpr_item);
            indexpr_item = lnext(indexpr_item);
            thisdata->vacattrstats[tcnt] = examine_attribute(Irel[ind], i + 1, indexkey);
            if (thisdata->vacattrstats[tcnt] != NULL)
            {
              tcnt++;
            }
          }
        }
        thisdata->attr_cnt = tcnt;
      }
    }
  }

     
                                                                          
                                                                        
                                                                            
                                                                       
     
  targrows = 100;
  for (i = 0; i < attr_cnt; i++)
  {
    if (targrows < vacattrstats[i]->minrows)
    {
      targrows = vacattrstats[i]->minrows;
    }
  }
  for (ind = 0; ind < nindexes; ind++)
  {
    AnlIndexData *thisdata = &indexdata[ind];

    for (i = 0; i < thisdata->attr_cnt; i++)
    {
      if (targrows < thisdata->vacattrstats[i]->minrows)
      {
        targrows = thisdata->vacattrstats[i]->minrows;
      }
    }
  }

     
                             
     
  rows = (HeapTuple *)palloc(targrows * sizeof(HeapTuple));
  if (inh)
  {
    numrows = acquire_inherited_sample_rows(onerel, elevel, rows, targrows, &totalrows, &totaldeadrows);
  }
  else
  {
    numrows = (*acquirefunc)(onerel, elevel, rows, targrows, &totalrows, &totaldeadrows);
  }

     
                                                                            
                                                                       
                                                                             
                                            
     
  if (numrows > 0)
  {
    MemoryContext col_context, old_context;
    bool build_ext_stats;

    col_context = AllocSetContextCreate(anl_context, "Analyze Column", ALLOCSET_DEFAULT_SIZES);
    old_context = MemoryContextSwitchTo(col_context);

    for (i = 0; i < attr_cnt; i++)
    {
      VacAttrStats *stats = vacattrstats[i];
      AttributeOpts *aopt;

      stats->rows = rows;
      stats->tupDesc = onerel->rd_att;
      stats->compute_stats(stats, std_fetch_func, numrows, totalrows);

         
                                                               
                                                           
         
      aopt = get_attribute_options(onerel->rd_id, stats->attr->attnum);
      if (aopt != NULL)
      {
        float8 n_distinct;

        n_distinct = inh ? aopt->n_distinct_inherited : aopt->n_distinct;
        if (n_distinct != 0.0)
        {
          stats->stadistinct = n_distinct;
        }
      }

      MemoryContextResetAndDeleteChildren(col_context);
    }

    if (hasindex)
    {
      compute_index_stats(onerel, totalrows, indexdata, nindexes, rows, numrows, col_context);
    }

    MemoryContextSwitchTo(old_context);
    MemoryContextDelete(col_context);

       
                                                                      
                                                                           
                                                                         
       
    update_attstats(RelationGetRelid(onerel), inh, attr_cnt, vacattrstats);

    for (ind = 0; ind < nindexes; ind++)
    {
      AnlIndexData *thisdata = &indexdata[ind];

      update_attstats(RelationGetRelid(Irel[ind]), false, thisdata->attr_cnt, thisdata->vacattrstats);
    }

       
                                                              
       
                                                                       
                                                                      
                                                                          
                                                                          
                                                                      
                                                                      
                                                                       
                                                                        
                                                                     
       
    build_ext_stats = (onerel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE) ? inh : (!inh);

       
                                                     
       
                                                                          
                                                         
       
    if (build_ext_stats)
    {
      BuildRelationExtStatistics(onerel, totalrows, numrows, rows, attr_cnt, vacattrstats);
    }
  }

     
                                                                      
                      
     
  if (!inh)
  {
    BlockNumber relallvisible;

    visibilitymap_count(onerel, &relallvisible, NULL);

    vac_update_relstats(onerel, relpages, totalrows, relallvisible, hasindex, InvalidTransactionId, InvalidMultiXactId, in_outer_xact);
  }

     
                                                                            
                                                                            
             
     
  if (!inh && !(params->options & VACOPT_VACUUM))
  {
    for (ind = 0; ind < nindexes; ind++)
    {
      AnlIndexData *thisdata = &indexdata[ind];
      double totalindexrows;

      totalindexrows = ceil(thisdata->tupleFract * totalrows);
      vac_update_relstats(Irel[ind], RelationGetNumberOfBlocks(Irel[ind]), totalindexrows, 0, false, InvalidTransactionId, InvalidMultiXactId, in_outer_xact);
    }
  }

     
                                                                    
                                                                           
                                                                           
                                                                    
                         
     
  if (!inh)
  {
    pgstat_report_analyze(onerel, totalrows, totaldeadrows, (va_cols == NIL));
  }

                                                                      
  if (!(params->options & VACOPT_VACUUM))
  {
    for (ind = 0; ind < nindexes; ind++)
    {
      IndexBulkDeleteResult *stats;
      IndexVacuumInfo ivinfo;

      ivinfo.index = Irel[ind];
      ivinfo.analyze_only = true;
      ivinfo.estimated_count = true;
      ivinfo.message_level = elevel;
      ivinfo.num_heap_tuples = onerel->rd_rel->reltuples;
      ivinfo.strategy = vac_strategy;

      stats = index_vacuum_cleanup(&ivinfo, NULL);

      if (stats)
      {
        pfree(stats);
      }
    }
  }

                         
  vac_close_indexes(nindexes, Irel, NoLock);

                                     
  if (IsAutoVacuumWorkerProcess() && params->log_min_duration >= 0)
  {
    if (params->log_min_duration == 0 || TimestampDifferenceExceeds(starttime, GetCurrentTimestamp(), params->log_min_duration))
    {
      ereport(LOG, (errmsg("automatic analyze of table \"%s.%s.%s\" system usage: %s", get_database_name(MyDatabaseId), get_namespace_name(RelationGetNamespace(onerel)), RelationGetRelationName(onerel), pg_rusage_show(&ru0))));
    }
  }

                                                             
  AtEOXact_GUC(false, save_nestlevel);

                                           
  SetUserIdAndSecContext(save_userid, save_sec_context);

                                                  
  MemoryContextSwitchTo(caller_context);
  MemoryContextDelete(anl_context);
  anl_context = NULL;
}

   
                                                  
   
static void
compute_index_stats(Relation onerel, double totalrows, AnlIndexData *indexdata, int nindexes, HeapTuple *rows, int numrows, MemoryContext col_context)
{
  MemoryContext ind_context, old_context;
  Datum values[INDEX_MAX_KEYS];
  bool isnull[INDEX_MAX_KEYS];
  int ind, i;

  ind_context = AllocSetContextCreate(anl_context, "Analyze Index", ALLOCSET_DEFAULT_SIZES);
  old_context = MemoryContextSwitchTo(ind_context);

  for (ind = 0; ind < nindexes; ind++)
  {
    AnlIndexData *thisdata = &indexdata[ind];
    IndexInfo *indexInfo = thisdata->indexInfo;
    int attr_cnt = thisdata->attr_cnt;
    TupleTableSlot *slot;
    EState *estate;
    ExprContext *econtext;
    ExprState *predicate;
    Datum *exprvals;
    bool *exprnulls;
    int numindexrows, tcnt, rowno;
    double totalindexrows;

                                                               
    if (attr_cnt == 0 && indexInfo->ii_Predicate == NIL)
    {
      continue;
    }

       
                                                              
                                                                           
                                                          
       
    estate = CreateExecutorState();
    econtext = GetPerTupleExprContext(estate);
                                                         
    slot = MakeSingleTupleTableSlot(RelationGetDescr(onerel), &TTSOpsHeapTuple);

                                                                      
    econtext->ecxt_scantuple = slot;

                                               
    predicate = ExecPrepareQual(indexInfo->ii_Predicate, estate);

                                                  
    exprvals = (Datum *)palloc(numrows * attr_cnt * sizeof(Datum));
    exprnulls = (bool *)palloc(numrows * attr_cnt * sizeof(bool));
    numindexrows = 0;
    tcnt = 0;
    for (rowno = 0; rowno < numrows; rowno++)
    {
      HeapTuple heapTuple = rows[rowno];

      vacuum_delay_point();

         
                                                                     
                                                                       
         
      ResetExprContext(econtext);

                                                         
      ExecStoreHeapTuple(heapTuple, slot, false);

                                                
      if (predicate != NULL)
      {
        if (!ExecQual(predicate, econtext))
        {
          continue;
        }
      }
      numindexrows++;

      if (attr_cnt > 0)
      {
           
                                                                   
                                                                    
           
        FormIndexDatum(indexInfo, slot, estate, values, isnull);

           
                                                                    
                                                                 
           
        for (i = 0; i < attr_cnt; i++)
        {
          VacAttrStats *stats = thisdata->vacattrstats[i];
          int attnum = stats->attr->attnum;

          if (isnull[attnum - 1])
          {
            exprvals[tcnt] = (Datum)0;
            exprnulls[tcnt] = true;
          }
          else
          {
            exprvals[tcnt] = datumCopy(values[attnum - 1], stats->attrtype->typbyval, stats->attrtype->typlen);
            exprnulls[tcnt] = false;
          }
          tcnt++;
        }
      }
    }

       
                                                                        
                                                                      
       
    thisdata->tupleFract = (double)numindexrows / (double)numrows;
    totalindexrows = ceil(thisdata->tupleFract * totalrows);

       
                                                                     
       
    if (numindexrows > 0)
    {
      MemoryContextSwitchTo(col_context);
      for (i = 0; i < attr_cnt; i++)
      {
        VacAttrStats *stats = thisdata->vacattrstats[i];
        AttributeOpts *aopt = get_attribute_options(stats->attr->attrelid, stats->attr->attnum);

        stats->exprvals = exprvals + i;
        stats->exprnulls = exprnulls + i;
        stats->rowstride = attr_cnt;
        stats->compute_stats(stats, ind_fetch_func, numindexrows, totalindexrows);

           
                                                                   
                                                               
                                                 
           
        if (aopt != NULL && aopt->n_distinct != 0.0)
        {
          stats->stadistinct = aopt->n_distinct;
        }

        MemoryContextResetAndDeleteChildren(col_context);
      }
    }

                      
    MemoryContextSwitchTo(ind_context);

    ExecDropSingleTupleTableSlot(slot);
    FreeExecutorState(estate);
    MemoryContextResetAndDeleteChildren(ind_context);
  }

  MemoryContextSwitchTo(old_context);
  MemoryContextDelete(ind_context);
}

   
                                                        
   
                                                                            
                                                       
   
                                                                               
                                                                         
   
static VacAttrStats *
examine_attribute(Relation onerel, int attnum, Node *index_expr)
{
  Form_pg_attribute attr = TupleDescAttr(onerel->rd_att, attnum - 1);
  HeapTuple typtuple;
  VacAttrStats *stats;
  int i;
  bool ok;

                                     
  if (attr->attisdropped)
  {
    return NULL;
  }

                                                         
  if (attr->attstattarget == 0)
  {
    return NULL;
  }

     
                                                                           
                                             
     
  stats = (VacAttrStats *)palloc0(sizeof(VacAttrStats));
  stats->attr = (Form_pg_attribute)palloc(ATTRIBUTE_FIXED_PART_SIZE);
  memcpy(stats->attr, attr, ATTRIBUTE_FIXED_PART_SIZE);

     
                                                                            
                                                                            
                                                                             
                                                                        
                                                                            
                                                                       
                                                 
     
  if (index_expr)
  {
    stats->attrtypid = exprType(index_expr);
    stats->attrtypmod = exprTypmod(index_expr);

       
                                                                           
                                                                         
                                    
       
    if (OidIsValid(onerel->rd_indcollation[attnum - 1]))
    {
      stats->attrcollid = onerel->rd_indcollation[attnum - 1];
    }
    else
    {
      stats->attrcollid = exprCollation(index_expr);
    }
  }
  else
  {
    stats->attrtypid = attr->atttypid;
    stats->attrtypmod = attr->atttypmod;
    stats->attrcollid = attr->attcollation;
  }

  typtuple = SearchSysCacheCopy1(TYPEOID, ObjectIdGetDatum(stats->attrtypid));
  if (!HeapTupleIsValid(typtuple))
  {
    elog(ERROR, "cache lookup failed for type %u", stats->attrtypid);
  }
  stats->attrtype = (Form_pg_type)GETSTRUCT(typtuple);
  stats->anl_context = anl_context;
  stats->tupattnum = attnum;

     
                                                                            
                                                                           
                                                                   
     
  for (i = 0; i < STATISTIC_NUM_SLOTS; i++)
  {
    stats->statypid[i] = stats->attrtypid;
    stats->statyplen[i] = stats->attrtype->typlen;
    stats->statypbyval[i] = stats->attrtype->typbyval;
    stats->statypalign[i] = stats->attrtype->typalign;
  }

     
                                                                            
                       
     
  if (OidIsValid(stats->attrtype->typanalyze))
  {
    ok = DatumGetBool(OidFunctionCall1(stats->attrtype->typanalyze, PointerGetDatum(stats)));
  }
  else
  {
    ok = std_typanalyze(stats);
  }

  if (!ok || stats->compute_stats == NULL || stats->minrows <= 0)
  {
    heap_freetuple(typtuple);
    pfree(stats->attr);
    pfree(stats);
    return NULL;
  }

  return stats;
}

   
                                                                         
   
                                                                          
                                        
                                                                          
                                                                          
                                                                     
   
                                                                              
                                                                 
   
                                                                       
                                                                       
                                                                        
                                                                       
                                                                        
                                                                        
                                                                      
                    
   
                                                                    
                                                                   
                                                                      
                                                                        
                                                                         
   
                                                                       
                                                                 
                                                                       
                                                                         
                                        
   
static int
acquire_sample_rows(Relation onerel, int elevel, HeapTuple *rows, int targrows, double *totalrows, double *totaldeadrows)
{
  int numrows = 0;                                     
  double samplerows = 0;                              
  double liverows = 0;                          
  double deadrows = 0;                          
  double rowstoskip = -1;                           
  BlockNumber totalblocks;
  TransactionId OldestXmin;
  BlockSamplerData bs;
  ReservoirStateData rstate;
  TupleTableSlot *slot;
  TableScanDesc scan;

  Assert(targrows > 0);

  totalblocks = RelationGetNumberOfBlocks(onerel);

                                                       
  OldestXmin = GetOldestXmin(onerel, PROCARRAY_FLAGS_VACUUM);

                                          
  BlockSampler_Init(&bs, totalblocks, targrows, random());
                                 
  reservoir_init_selection_state(&rstate, targrows);

  scan = table_beginscan_analyze(onerel);
  slot = table_slot_create(onerel, NULL);

                                        
  while (BlockSampler_HasMore(&bs))
  {
    BlockNumber targblock = BlockSampler_Next(&bs);

    vacuum_delay_point();

    if (!table_scan_analyze_next_block(scan, targblock, vac_strategy))
    {
      continue;
    }

    while (table_scan_analyze_next_tuple(scan, OldestXmin, &liverows, &deadrows, slot))
    {
         
                                                                   
                                                                       
                                                                        
                                                                         
                                                                    
                                                                    
                                                                         
                                                                   
                                                                         
                     
         
      if (numrows < targrows)
      {
        rows[numrows++] = ExecCopySlotHeapTuple(slot);
      }
      else
      {
           
                                                                
                                                                    
                                                                 
           
        if (rowstoskip < 0)
        {
          rowstoskip = reservoir_get_next_S(&rstate, samplerows, targrows);
        }

        if (rowstoskip <= 0)
        {
             
                                                                   
                             
             
          int k = (int)(targrows * sampler_random_fract(rstate.randstate));

          Assert(k >= 0 && k < targrows);
          heap_freetuple(rows[k]);
          rows[k] = ExecCopySlotHeapTuple(slot);
        }

        rowstoskip -= 1;
      }

      samplerows += 1;
    }
  }

  ExecDropSingleTupleTableSlot(slot);
  table_endscan(scan);

     
                                                                            
                                                
     
                                                                
                                                                         
                                
     
  if (numrows == targrows)
  {
    qsort((void *)rows, numrows, sizeof(HeapTuple), compare_rows);
  }

     
                                                                             
                                                                         
                                                                             
                                                                         
                 
     
  if (bs.m > 0)
  {
    *totalrows = floor((liverows / bs.m) * totalblocks + 0.5);
    *totaldeadrows = floor((deadrows / bs.m) * totalblocks + 0.5);
  }
  else
  {
    *totalrows = 0.0;
    *totaldeadrows = 0.0;
  }

     
                                         
     
  ereport(elevel, (errmsg("\"%s\": scanned %d of %u pages, "
                          "containing %.0f live rows and %.0f dead rows; "
                          "%d rows in sample, %.0f estimated total rows",
                      RelationGetRelationName(onerel), bs.m, totalblocks, liverows, deadrows, numrows, *totalrows)));

  return numrows;
}

   
                                             
   
static int
compare_rows(const void *a, const void *b)
{
  HeapTuple ha = *(const HeapTuple *)a;
  HeapTuple hb = *(const HeapTuple *)b;
  BlockNumber ba = ItemPointerGetBlockNumber(&ha->t_self);
  OffsetNumber oa = ItemPointerGetOffsetNumber(&ha->t_self);
  BlockNumber bb = ItemPointerGetBlockNumber(&hb->t_self);
  OffsetNumber ob = ItemPointerGetOffsetNumber(&hb->t_self);

  if (ba < bb)
  {
    return -1;
  }
  if (ba > bb)
  {
    return 1;
  }
  if (oa < ob)
  {
    return -1;
  }
  if (oa > ob)
  {
    return 1;
  }
  return 0;
}

   
                                                                              
   
                                                                      
                                                                           
                                                                           
                                                           
   
static int
acquire_inherited_sample_rows(Relation onerel, int elevel, HeapTuple *rows, int targrows, double *totalrows, double *totaldeadrows)
{
  List *tableOIDs;
  Relation *rels;
  AcquireSampleRowsFunc *acquirefuncs;
  double *relblocks;
  double totalblocks;
  int numrows, nrels, i;
  ListCell *lc;
  bool has_child;

     
                                                                           
                   
     
  tableOIDs = find_all_inheritors(RelationGetRelid(onerel), AccessShareLock, NULL);

     
                                                                        
                                                                            
                                                               
                                                                          
                                                              
     
  if (list_length(tableOIDs) < 2)
  {
                                                                         
    CommandCounterIncrement();
    SetRelationHasSubclass(RelationGetRelid(onerel), false);
    ereport(elevel, (errmsg("skipping analyze of \"%s.%s\" inheritance tree --- this inheritance tree contains no child tables", get_namespace_name(RelationGetNamespace(onerel)), RelationGetRelationName(onerel))));
    return 0;
  }

     
                                                                          
                                                                         
     
  rels = (Relation *)palloc(list_length(tableOIDs) * sizeof(Relation));
  acquirefuncs = (AcquireSampleRowsFunc *)palloc(list_length(tableOIDs) * sizeof(AcquireSampleRowsFunc));
  relblocks = (double *)palloc(list_length(tableOIDs) * sizeof(double));
  totalblocks = 0;
  nrels = 0;
  has_child = false;
  foreach (lc, tableOIDs)
  {
    Oid childOID = lfirst_oid(lc);
    Relation childrel;
    AcquireSampleRowsFunc acquirefunc = NULL;
    BlockNumber relpages = 0;

                                        
    childrel = table_open(childOID, NoLock);

                                                 
    if (RELATION_IS_OTHER_TEMP(childrel))
    {
                                          
      Assert(childrel != onerel);
      table_close(childrel, AccessShareLock);
      continue;
    }

                                                                          
    if (childrel->rd_rel->relkind == RELKIND_RELATION || childrel->rd_rel->relkind == RELKIND_MATVIEW)
    {
                                                                      
      acquirefunc = acquire_sample_rows;
      relpages = RelationGetNumberOfBlocks(childrel);
    }
    else if (childrel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
    {
         
                                                                  
                                       
         
      FdwRoutine *fdwroutine;
      bool ok = false;

      fdwroutine = GetFdwRoutineForRelation(childrel, false);

      if (fdwroutine->AnalyzeForeignTable != NULL)
      {
        ok = fdwroutine->AnalyzeForeignTable(childrel, &acquirefunc, &relpages);
      }

      if (!ok)
      {
                                                
        Assert(childrel != onerel);
        table_close(childrel, AccessShareLock);
        continue;
      }
    }
    else
    {
         
                                                                      
                            
         
      Assert(childrel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE);
      if (childrel != onerel)
      {
        table_close(childrel, AccessShareLock);
      }
      else
      {
        table_close(childrel, NoLock);
      }
      continue;
    }

                                      
    has_child = true;
    rels[nrels] = childrel;
    acquirefuncs[nrels] = acquirefunc;
    relblocks[nrels] = (double)relpages;
    totalblocks += (double)relpages;
    nrels++;
  }

     
                                                                          
                                                                         
     
  if (!has_child)
  {
    ereport(elevel, (errmsg("skipping analyze of \"%s.%s\" inheritance tree --- this inheritance tree contains no analyzable child tables", get_namespace_name(RelationGetNamespace(onerel)), RelationGetRelationName(onerel))));
    return 0;
  }

     
                                                                           
                                                                             
                                                                        
                                            
     
  numrows = 0;
  *totalrows = 0;
  *totaldeadrows = 0;
  for (i = 0; i < nrels; i++)
  {
    Relation childrel = rels[i];
    AcquireSampleRowsFunc acquirefunc = acquirefuncs[i];
    double childblocks = relblocks[i];

    if (childblocks > 0)
    {
      int childtargrows;

      childtargrows = (int)rint(targrows * childblocks / totalblocks);
                                                            
      childtargrows = Min(childtargrows, targrows - numrows);
      if (childtargrows > 0)
      {
        int childrows;
        double trows, tdrows;

                                                       
        childrows = (*acquirefunc)(childrel, elevel, rows + numrows, childtargrows, &trows, &tdrows);

                                                                     
        if (childrows > 0 && !equalTupleDescs(RelationGetDescr(childrel), RelationGetDescr(onerel)))
        {
          TupleConversionMap *map;

          map = convert_tuples_by_name(RelationGetDescr(childrel), RelationGetDescr(onerel), gettext_noop("could not convert row type"));
          if (map != NULL)
          {
            int j;

            for (j = 0; j < childrows; j++)
            {
              HeapTuple newtup;

              newtup = execute_attr_map_tuple(rows[numrows + j], map);
              heap_freetuple(rows[numrows + j]);
              rows[numrows + j] = newtup;
            }
            free_conversion_map(map);
          }
        }

                               
        numrows += childrows;
        *totalrows += trows;
        *totaldeadrows += tdrows;
      }
    }

       
                                                                        
                                                           
       
    table_close(childrel, NoLock);
  }

  return numrows;
}

   
                                                                     
   
                                                                      
                                                                
                                                                   
                                                                         
   
                                                                      
                                                                     
                                                                          
   
                                                                   
                                                                      
                                                                         
                                                                   
                                   
   
                                                                     
                                                                      
                                                                      
   
static void
update_attstats(Oid relid, bool inh, int natts, VacAttrStats **vacattrstats)
{
  Relation sd;
  int attno;

  if (natts <= 0)
  {
    return;                    
  }

  sd = table_open(StatisticRelationId, RowExclusiveLock);

  for (attno = 0; attno < natts; attno++)
  {
    VacAttrStats *stats = vacattrstats[attno];
    HeapTuple stup, oldtup;
    int i, k, n;
    Datum values[Natts_pg_statistic];
    bool nulls[Natts_pg_statistic];
    bool replaces[Natts_pg_statistic];

                                                         
    if (!stats->stats_valid)
    {
      continue;
    }

       
                                          
       
    for (i = 0; i < Natts_pg_statistic; ++i)
    {
      nulls[i] = false;
      replaces[i] = true;
    }

    values[Anum_pg_statistic_starelid - 1] = ObjectIdGetDatum(relid);
    values[Anum_pg_statistic_staattnum - 1] = Int16GetDatum(stats->attr->attnum);
    values[Anum_pg_statistic_stainherit - 1] = BoolGetDatum(inh);
    values[Anum_pg_statistic_stanullfrac - 1] = Float4GetDatum(stats->stanullfrac);
    values[Anum_pg_statistic_stawidth - 1] = Int32GetDatum(stats->stawidth);
    values[Anum_pg_statistic_stadistinct - 1] = Float4GetDatum(stats->stadistinct);
    i = Anum_pg_statistic_stakind1 - 1;
    for (k = 0; k < STATISTIC_NUM_SLOTS; k++)
    {
      values[i++] = Int16GetDatum(stats->stakind[k]);               
    }
    i = Anum_pg_statistic_staop1 - 1;
    for (k = 0; k < STATISTIC_NUM_SLOTS; k++)
    {
      values[i++] = ObjectIdGetDatum(stats->staop[k]);             
    }
    i = Anum_pg_statistic_stacoll1 - 1;
    for (k = 0; k < STATISTIC_NUM_SLOTS; k++)
    {
      values[i++] = ObjectIdGetDatum(stats->stacoll[k]);               
    }
    i = Anum_pg_statistic_stanumbers1 - 1;
    for (k = 0; k < STATISTIC_NUM_SLOTS; k++)
    {
      int nnum = stats->numnumbers[k];

      if (nnum > 0)
      {
        Datum *numdatums = (Datum *)palloc(nnum * sizeof(Datum));
        ArrayType *arry;

        for (n = 0; n < nnum; n++)
        {
          numdatums[n] = Float4GetDatum(stats->stanumbers[k][n]);
        }
                                                              
        arry = construct_array(numdatums, nnum, FLOAT4OID, sizeof(float4), FLOAT4PASSBYVAL, 'i');
        values[i++] = PointerGetDatum(arry);                  
      }
      else
      {
        nulls[i] = true;
        values[i++] = (Datum)0;
      }
    }
    i = Anum_pg_statistic_stavalues1 - 1;
    for (k = 0; k < STATISTIC_NUM_SLOTS; k++)
    {
      if (stats->numvalues[k] > 0)
      {
        ArrayType *arry;

        arry = construct_array(stats->stavalues[k], stats->numvalues[k], stats->statypid[k], stats->statyplen[k], stats->statypbyval[k], stats->statypalign[k]);
        values[i++] = PointerGetDatum(arry);                 
      }
      else
      {
        nulls[i] = true;
        values[i++] = (Datum)0;
      }
    }

                                                                   
    oldtup = SearchSysCache3(STATRELATTINH, ObjectIdGetDatum(relid), Int16GetDatum(stats->attr->attnum), BoolGetDatum(inh));

    if (HeapTupleIsValid(oldtup))
    {
                           
      stup = heap_modify_tuple(oldtup, RelationGetDescr(sd), values, nulls, replaces);
      ReleaseSysCache(oldtup);
      CatalogTupleUpdate(sd, &stup->t_self, stup);
    }
    else
    {
                                
      stup = heap_form_tuple(RelationGetDescr(sd), values, nulls);
      CatalogTupleInsert(sd, stup);
    }

    heap_freetuple(stup);
  }

  table_close(sd, RowExclusiveLock);
}

   
                                                                 
   
                                                                         
                                              
   
static Datum
std_fetch_func(VacAttrStatsP stats, int rownum, bool *isNull)
{
  int attnum = stats->tupattnum;
  HeapTuple tuple = stats->rows[rownum];
  TupleDesc tupDesc = stats->tupDesc;

  return heap_getattr(tuple, attnum, tupDesc, isNull);
}

   
                                                   
   
                                                                       
                         
   
static Datum
ind_fetch_func(VacAttrStatsP stats, int rownum, bool *isNull)
{
  int i;

                                                                   
  i = rownum * stats->rowstride;
  *isNull = stats->exprnulls[i];
  return stats->exprvals[i];
}

                                                                             
   
                                                                            
                                                                            
                                                     
   
                                                                             
   

   
                                                                            
                                                                               
                                                                         
                                                                        
                                                                              
                                                                       
                         
   
#define WIDTH_THRESHOLD 1024

#define swapInt(a, b)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int _tmp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    _tmp = a;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    a = b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    b = _tmp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  } while (0)
#define swapDatum(a, b)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    Datum _tmp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    _tmp = a;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
    a = b;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
    b = _tmp;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
  } while (0)

   
                                                           
   
typedef struct
{
  int count;                      
  int first;                                         
} ScalarMCVItem;

typedef struct
{
  SortSupport ssup;
  int *tupnoLink;
} CompareScalarsContext;

static void
compute_trivial_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows);
static void
compute_distinct_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows);
static void
compute_scalar_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows);
static int
compare_scalars(const void *a, const void *b, void *arg);
static int
compare_mcvs(const void *a, const void *b);
static int
analyze_mcv_list(int *mcv_counts, int num_mcv, double stadistinct, double stanullfrac, int samplerows, double totalrows);

   
                                                                   
   
bool
std_typanalyze(VacAttrStats *stats)
{
  Form_pg_attribute attr = stats->attr;
  Oid ltopr;
  Oid eqopr;
  StdAnalyzeData *mystats;

                                                                      
                                                                   
  if (attr->attstattarget < 0)
  {
    attr->attstattarget = default_statistics_target;
  }

                                                                
  get_sort_group_operators(stats->attrtypid, false, false, false, &ltopr, &eqopr, NULL, NULL);

                                                         
  mystats = (StdAnalyzeData *)palloc(sizeof(StdAnalyzeData));
  mystats->eqopr = eqopr;
  mystats->eqfunc = OidIsValid(eqopr) ? get_opcode(eqopr) : InvalidOid;
  mystats->ltopr = ltopr;
  stats->extra_data = mystats;

     
                                                          
     
  if (OidIsValid(eqopr) && OidIsValid(ltopr))
  {
                                       
    stats->compute_stats = compute_scalar_stats;
                           
                                                             
                                                                         
                                                                    
                                                                        
                                                                     
                                                                      
                                                                     
                             
                                        
                                                              
                       
                                                                     
                                                                   
                                                                         
                                                                       
                              
                           
       
    stats->minrows = 300 * attr->attstattarget;
  }
  else if (OidIsValid(eqopr))
  {
                                                
    stats->compute_stats = compute_distinct_stats;
                                                     
    stats->minrows = 300 * attr->attstattarget;
  }
  else
  {
                                             
    stats->compute_stats = compute_trivial_stats;
                                                     
    stats->minrows = 300 * attr->attstattarget;
  }

  return true;
}

   
                                                                   
   
                                                                         
   
                                                                           
   
static void
compute_trivial_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows)
{
  int i;
  int null_cnt = 0;
  int nonnull_cnt = 0;
  double total_width = 0;
  bool is_varlena = (!stats->attrtype->typbyval && stats->attrtype->typlen == -1);
  bool is_varwidth = (!stats->attrtype->typbyval && stats->attrtype->typlen < 0);

  for (i = 0; i < samplerows; i++)
  {
    Datum value;
    bool isnull;

    vacuum_delay_point();

    value = fetchfunc(stats, i, &isnull);

                                
    if (isnull)
    {
      null_cnt++;
      continue;
    }
    nonnull_cnt++;

       
                                                                       
                                                                           
                                                                           
             
       
    if (is_varlena)
    {
      total_width += VARSIZE_ANY(DatumGetPointer(value));
    }
    else if (is_varwidth)
    {
                           
      total_width += strlen(DatumGetCString(value)) + 1;
    }
  }

                                                                           
  if (nonnull_cnt > 0)
  {
    stats->stats_valid = true;
                                                 
    stats->stanullfrac = (double)null_cnt / (double)samplerows;
    if (is_varwidth)
    {
      stats->stawidth = total_width / (double)nonnull_cnt;
    }
    else
    {
      stats->stawidth = stats->attrtype->typlen;
    }
    stats->stadistinct = 0.0;                
  }
  else if (null_cnt > 0)
  {
                                                                 
    stats->stats_valid = true;
    stats->stanullfrac = 1.0;
    if (is_varwidth)
    {
      stats->stawidth = 0;                
    }
    else
    {
      stats->stawidth = stats->attrtype->typlen;
    }
    stats->stadistinct = 0.0;                
  }
}

   
                                                                             
   
                                                                       
   
                                                                      
                                                                      
   
                                                                        
                                                                          
                                                                    
                                                                          
                                                                          
                                                                   
   
static void
compute_distinct_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows)
{
  int i;
  int null_cnt = 0;
  int nonnull_cnt = 0;
  int toowide_cnt = 0;
  double total_width = 0;
  bool is_varlena = (!stats->attrtype->typbyval && stats->attrtype->typlen == -1);
  bool is_varwidth = (!stats->attrtype->typbyval && stats->attrtype->typlen < 0);
  FmgrInfo f_cmpeq;
  typedef struct
  {
    Datum value;
    int count;
  } TrackItem;
  TrackItem *track;
  int track_cnt, track_max;
  int num_mcv = stats->attr->attstattarget;
  StdAnalyzeData *mystats = (StdAnalyzeData *)stats->extra_data;

     
                                                                          
     
  track_max = 2 * num_mcv;
  if (track_max < 10)
  {
    track_max = 10;
  }
  track = (TrackItem *)palloc(track_max * sizeof(TrackItem));
  track_cnt = 0;

  fmgr_info(mystats->eqfunc, &f_cmpeq);

  for (i = 0; i < samplerows; i++)
  {
    Datum value;
    bool isnull;
    bool match;
    int firstcount1, j;

    vacuum_delay_point();

    value = fetchfunc(stats, i, &isnull);

                                
    if (isnull)
    {
      null_cnt++;
      continue;
    }
    nonnull_cnt++;

       
                                                                       
                                                                           
                                                                           
             
       
    if (is_varlena)
    {
      total_width += VARSIZE_ANY(DatumGetPointer(value));

         
                                                                     
                                                                      
                                                                     
                                                                   
                           
         
      if (toast_raw_datum_size(value) > WIDTH_THRESHOLD)
      {
        toowide_cnt++;
        continue;
      }
      value = PointerGetDatum(PG_DETOAST_DATUM(value));
    }
    else if (is_varwidth)
    {
                           
      total_width += strlen(DatumGetCString(value)) + 1;
    }

       
                                                                 
       
    match = false;
    firstcount1 = track_cnt;
    for (j = 0; j < track_cnt; j++)
    {
      if (DatumGetBool(FunctionCall2Coll(&f_cmpeq, stats->attrcollid, value, track[j].value)))
      {
        match = true;
        break;
      }
      if (j < firstcount1 && track[j].count == 1)
      {
        firstcount1 = j;
      }
    }

    if (match)
    {
                         
      track[j].count++;
                                                                    
      while (j > 0 && track[j].count > track[j - 1].count)
      {
        swapDatum(track[j].value, track[j - 1].value);
        swapInt(track[j].count, track[j - 1].count);
        j--;
      }
    }
    else
    {
                                                     
      if (track_cnt < track_max)
      {
        track_cnt++;
      }
      for (j = track_cnt - 1; j > firstcount1; j--)
      {
        track[j].value = track[j - 1].value;
        track[j].count = track[j - 1].count;
      }
      if (firstcount1 < track_cnt)
      {
        track[firstcount1].value = value;
        track[firstcount1].count = 1;
      }
    }
  }

                                                                        
  if (nonnull_cnt > 0)
  {
    int nmultiple, summultiple;

    stats->stats_valid = true;
                                                 
    stats->stanullfrac = (double)null_cnt / (double)samplerows;
    if (is_varwidth)
    {
      stats->stawidth = total_width / (double)nonnull_cnt;
    }
    else
    {
      stats->stawidth = stats->attrtype->typlen;
    }

                                                            
    summultiple = 0;
    for (nmultiple = 0; nmultiple < track_cnt; nmultiple++)
    {
      if (track[nmultiple].count == 1)
      {
        break;
      }
      summultiple += track[nmultiple].count;
    }

    if (nmultiple == 0)
    {
         
                                                                       
                                                                 
         
      stats->stadistinct = -1.0 * (1.0 - stats->stanullfrac);
    }
    else if (track_cnt < track_max && toowide_cnt == 0 && nmultiple == track_cnt)
    {
         
                                                                      
                                                                    
                                                                    
                                                                       
                                                                        
                                                                         
                                       
         
      stats->stadistinct = track_cnt;
    }
    else
    {
                   
                                                                    
                                                                      
                                  
                                                                 
                                                                   
                                                                     
                                                                 
                                                                      
                                                      
         
                                                                      
                                                                       
                                                                     
                                                                         
                                                 
         
                                                                        
                                                                       
                                                                  
                                                                    
                      
                   
         
      int f1 = nonnull_cnt - summultiple;
      int d = f1 + nmultiple;
      double n = samplerows - null_cnt;
      double N = totalrows * (1.0 - stats->stanullfrac);
      double stadistinct;

                                                         
      if (N > 0)
      {
        stadistinct = (n * d) / ((n - f1) + f1 * n / N);
      }
      else
      {
        stadistinct = 0;
      }

                                                         
      if (stadistinct < d)
      {
        stadistinct = d;
      }
      if (stadistinct > N)
      {
        stadistinct = N;
      }
                                
      stats->stadistinct = floor(stadistinct + 0.5);
    }

       
                                                                         
                                                                      
                                                                          
              
       
    if (stats->stadistinct > 0.1 * totalrows)
    {
      stats->stadistinct = -(stats->stadistinct / totalrows);
    }

       
                                                                          
                                                                          
                                                                           
                                                                
                                                                  
       
                                                                       
                                                                     
                                                                           
                                                                          
                                                                          
                                                                     
                                                                      
               
       
    if (track_cnt < track_max && toowide_cnt == 0 && stats->stadistinct > 0 && track_cnt <= num_mcv)
    {
                                                                 
      num_mcv = track_cnt;
    }
    else
    {
      int *mcv_counts;

                                                                     
      if (num_mcv > track_cnt)
      {
        num_mcv = track_cnt;
      }

      if (num_mcv > 0)
      {
        mcv_counts = (int *)palloc(num_mcv * sizeof(int));
        for (i = 0; i < num_mcv; i++)
        {
          mcv_counts[i] = track[i].count;
        }

        num_mcv = analyze_mcv_list(mcv_counts, num_mcv, stats->stadistinct, stats->stanullfrac, samplerows, totalrows);
      }
    }

                                 
    if (num_mcv > 0)
    {
      MemoryContext old_context;
      Datum *mcv_values;
      float4 *mcv_freqs;

                                                        
      old_context = MemoryContextSwitchTo(stats->anl_context);
      mcv_values = (Datum *)palloc(num_mcv * sizeof(Datum));
      mcv_freqs = (float4 *)palloc(num_mcv * sizeof(float4));
      for (i = 0; i < num_mcv; i++)
      {
        mcv_values[i] = datumCopy(track[i].value, stats->attrtype->typbyval, stats->attrtype->typlen);
        mcv_freqs[i] = (double)track[i].count / (double)samplerows;
      }
      MemoryContextSwitchTo(old_context);

      stats->stakind[0] = STATISTIC_KIND_MCV;
      stats->staop[0] = mystats->eqopr;
      stats->stacoll[0] = stats->attrcollid;
      stats->stanumbers[0] = mcv_freqs;
      stats->numnumbers[0] = num_mcv;
      stats->stavalues[0] = mcv_values;
      stats->numvalues[0] = num_mcv;

         
                                                                       
                                                       
         
    }
  }
  else if (null_cnt > 0)
  {
                                                                 
    stats->stats_valid = true;
    stats->stanullfrac = 1.0;
    if (is_varwidth)
    {
      stats->stawidth = 0;                
    }
    else
    {
      stats->stawidth = stats->attrtype->typlen;
    }
    stats->stadistinct = 0.0;                
  }

                                                                         
}

   
                                                       
   
                                                                        
   
                                                                      
                                                                      
                                                                             
   
                                                                       
                           
   
static void
compute_scalar_stats(VacAttrStatsP stats, AnalyzeAttrFetchFunc fetchfunc, int samplerows, double totalrows)
{
  int i;
  int null_cnt = 0;
  int nonnull_cnt = 0;
  int toowide_cnt = 0;
  double total_width = 0;
  bool is_varlena = (!stats->attrtype->typbyval && stats->attrtype->typlen == -1);
  bool is_varwidth = (!stats->attrtype->typbyval && stats->attrtype->typlen < 0);
  double corr_xysum;
  SortSupportData ssup;
  ScalarItem *values;
  int values_cnt = 0;
  int *tupnoLink;
  ScalarMCVItem *track;
  int track_cnt = 0;
  int num_mcv = stats->attr->attstattarget;
  int num_bins = stats->attr->attstattarget;
  StdAnalyzeData *mystats = (StdAnalyzeData *)stats->extra_data;

  values = (ScalarItem *)palloc(samplerows * sizeof(ScalarItem));
  tupnoLink = (int *)palloc(samplerows * sizeof(int));
  track = (ScalarMCVItem *)palloc(num_mcv * sizeof(ScalarMCVItem));

  memset(&ssup, 0, sizeof(ssup));
  ssup.ssup_cxt = CurrentMemoryContext;
  ssup.ssup_collation = stats->attrcollid;
  ssup.ssup_nulls_first = false;

     
                                                                            
                                                                         
                                                                         
     
  ssup.abbreviate = false;

  PrepareSortSupportFromOrderingOp(mystats->ltopr, &ssup);

                                            
  for (i = 0; i < samplerows; i++)
  {
    Datum value;
    bool isnull;

    vacuum_delay_point();

    value = fetchfunc(stats, i, &isnull);

                                
    if (isnull)
    {
      null_cnt++;
      continue;
    }
    nonnull_cnt++;

       
                                                                       
                                                                           
                                                                           
             
       
    if (is_varlena)
    {
      total_width += VARSIZE_ANY(DatumGetPointer(value));

         
                                                                     
                                                                      
                                                                     
                                                                   
                           
         
      if (toast_raw_datum_size(value) > WIDTH_THRESHOLD)
      {
        toowide_cnt++;
        continue;
      }
      value = PointerGetDatum(PG_DETOAST_DATUM(value));
    }
    else if (is_varwidth)
    {
                           
      total_width += strlen(DatumGetCString(value)) + 1;
    }

                                         
    values[values_cnt].value = value;
    values[values_cnt].tupno = values_cnt;
    tupnoLink[values_cnt] = values_cnt;
    values_cnt++;
  }

                                                                        
  if (values_cnt > 0)
  {
    int ndistinct,                                  
        nmultiple,                                   
        num_hist, dups_cnt;
    int slot_idx = 0;
    CompareScalarsContext cxt;

                                   
    cxt.ssup = &ssup;
    cxt.tupnoLink = tupnoLink;
    qsort_arg((void *)values, values_cnt, sizeof(ScalarItem), compare_scalars, (void *)&cxt);

       
                                                                         
                                                   
       
                                                                      
                                                                           
                                                                           
                                                                        
                                                                           
                                                                           
                                                                          
                                                                          
                                                                        
                                                                  
                                                                      
                                                                           
                                                                         
                             
       
    corr_xysum = 0;
    ndistinct = 0;
    nmultiple = 0;
    dups_cnt = 0;
    for (i = 0; i < values_cnt; i++)
    {
      int tupno = values[i].tupno;

      corr_xysum += ((double)i) * ((double)tupno);
      dups_cnt++;
      if (tupnoLink[tupno] == tupno)
      {
                                                     
        ndistinct++;
        if (dups_cnt > 1)
        {
          nmultiple++;
          if (track_cnt < num_mcv || dups_cnt > track[track_cnt - 1].count)
          {
               
                                                           
                                                                 
                                                                   
                     
               
            int j;

            if (track_cnt < num_mcv)
            {
              track_cnt++;
            }
            for (j = track_cnt - 1; j > 0; j--)
            {
              if (dups_cnt <= track[j - 1].count)
              {
                break;
              }
              track[j].count = track[j - 1].count;
              track[j].first = track[j - 1].first;
            }
            track[j].count = dups_cnt;
            track[j].first = i + 1 - dups_cnt;
          }
        }
        dups_cnt = 0;
      }
    }

    stats->stats_valid = true;
                                                 
    stats->stanullfrac = (double)null_cnt / (double)samplerows;
    if (is_varwidth)
    {
      stats->stawidth = total_width / (double)nonnull_cnt;
    }
    else
    {
      stats->stawidth = stats->attrtype->typlen;
    }

    if (nmultiple == 0)
    {
         
                                                                       
                                                                 
         
      stats->stadistinct = -1.0 * (1.0 - stats->stanullfrac);
    }
    else if (toowide_cnt == 0 && nmultiple == ndistinct)
    {
         
                                                                        
                                                                       
                                                                    
                                                                       
                                                                       
                                                          
         
      stats->stadistinct = ndistinct;
    }
    else
    {
                   
                                                                    
                                                                      
                                  
                                                                 
                                                                   
                                                                     
                                                                 
                                                                      
                                                      
         
                                                                      
                                                                       
                                                                     
                                                                         
                                                 
         
                                                             
                   
         
      int f1 = ndistinct - nmultiple + toowide_cnt;
      int d = f1 + nmultiple;
      double n = samplerows - null_cnt;
      double N = totalrows * (1.0 - stats->stanullfrac);
      double stadistinct;

                                                         
      if (N > 0)
      {
        stadistinct = (n * d) / ((n - f1) + f1 * n / N);
      }
      else
      {
        stadistinct = 0;
      }

                                                         
      if (stadistinct < d)
      {
        stadistinct = d;
      }
      if (stadistinct > N)
      {
        stadistinct = N;
      }
                                
      stats->stadistinct = floor(stadistinct + 0.5);
    }

       
                                                                         
                                                                      
                                                                          
              
       
    if (stats->stadistinct > 0.1 * totalrows)
    {
      stats->stadistinct = -(stats->stadistinct / totalrows);
    }

       
                                                                          
                                                                          
                                                                           
                                                                
                                                                  
       
                                                                       
                                                                     
                                                                           
                                                                          
                                                                          
                                                                     
                                                                      
               
       
    if (track_cnt == ndistinct && toowide_cnt == 0 && stats->stadistinct > 0 && track_cnt <= num_mcv)
    {
                                                                 
      num_mcv = track_cnt;
    }
    else
    {
      int *mcv_counts;

                                                                     
      if (num_mcv > track_cnt)
      {
        num_mcv = track_cnt;
      }

      if (num_mcv > 0)
      {
        mcv_counts = (int *)palloc(num_mcv * sizeof(int));
        for (i = 0; i < num_mcv; i++)
        {
          mcv_counts[i] = track[i].count;
        }

        num_mcv = analyze_mcv_list(mcv_counts, num_mcv, stats->stadistinct, stats->stanullfrac, samplerows, totalrows);
      }
    }

                                 
    if (num_mcv > 0)
    {
      MemoryContext old_context;
      Datum *mcv_values;
      float4 *mcv_freqs;

                                                        
      old_context = MemoryContextSwitchTo(stats->anl_context);
      mcv_values = (Datum *)palloc(num_mcv * sizeof(Datum));
      mcv_freqs = (float4 *)palloc(num_mcv * sizeof(float4));
      for (i = 0; i < num_mcv; i++)
      {
        mcv_values[i] = datumCopy(values[track[i].first].value, stats->attrtype->typbyval, stats->attrtype->typlen);
        mcv_freqs[i] = (double)track[i].count / (double)samplerows;
      }
      MemoryContextSwitchTo(old_context);

      stats->stakind[slot_idx] = STATISTIC_KIND_MCV;
      stats->staop[slot_idx] = mystats->eqopr;
      stats->stacoll[slot_idx] = stats->attrcollid;
      stats->stanumbers[slot_idx] = mcv_freqs;
      stats->numnumbers[slot_idx] = num_mcv;
      stats->stavalues[slot_idx] = mcv_values;
      stats->numvalues[slot_idx] = num_mcv;

         
                                                                       
                                                       
         
      slot_idx++;
    }

       
                                                                          
                                                                    
                                                          
       
    num_hist = ndistinct - num_mcv;
    if (num_hist > num_bins)
    {
      num_hist = num_bins + 1;
    }
    if (num_hist >= 2)
    {
      MemoryContext old_context;
      Datum *hist_values;
      int nvals;
      int pos, posfrac, delta, deltafrac;

                                                                     
      qsort((void *)track, num_mcv, sizeof(ScalarMCVItem), compare_mcvs);

         
                                                             
         
                                                                         
                                                                    
                                                                    
         
      if (num_mcv > 0)
      {
        int src, dest;
        int j;

        src = dest = 0;
        j = 0;                                         
        while (src < values_cnt)
        {
          int ncopy;

          if (j < num_mcv)
          {
            int first = track[j].first;

            if (src >= first)
            {
                                              
              src = first + track[j].count;
              j++;
              continue;
            }
            ncopy = first - src;
          }
          else
          {
            ncopy = values_cnt - src;
          }
          memmove(&values[dest], &values[src], ncopy * sizeof(ScalarItem));
          src += ncopy;
          dest += ncopy;
        }
        nvals = dest;
      }
      else
      {
        nvals = values_cnt;
      }
      Assert(nvals >= num_hist);

                                                        
      old_context = MemoryContextSwitchTo(stats->anl_context);
      hist_values = (Datum *)palloc(num_hist * sizeof(Datum));

         
                                                                        
                                                                     
                                                                        
                                                                       
                                                                      
                                                                        
                                                                  
         
      delta = (nvals - 1) / (num_hist - 1);
      deltafrac = (nvals - 1) % (num_hist - 1);
      pos = posfrac = 0;

      for (i = 0; i < num_hist; i++)
      {
        hist_values[i] = datumCopy(values[pos].value, stats->attrtype->typbyval, stats->attrtype->typlen);
        pos += delta;
        posfrac += deltafrac;
        if (posfrac >= (num_hist - 1))
        {
                                                                
          pos++;
          posfrac -= (num_hist - 1);
        }
      }

      MemoryContextSwitchTo(old_context);

      stats->stakind[slot_idx] = STATISTIC_KIND_HISTOGRAM;
      stats->staop[slot_idx] = mystats->ltopr;
      stats->stacoll[slot_idx] = stats->attrcollid;
      stats->stavalues[slot_idx] = hist_values;
      stats->numvalues[slot_idx] = num_hist;

         
                                                                       
                                                       
         
      slot_idx++;
    }

                                                                   
    if (values_cnt > 1)
    {
      MemoryContext old_context;
      float4 *corrs;
      double corr_xsum, corr_x2sum;

                                                        
      old_context = MemoryContextSwitchTo(stats->anl_context);
      corrs = (float4 *)palloc(sizeof(float4));
      MemoryContextSwitchTo(old_context);

                   
                                                       
                                  
                                   
                                        
                                   
                                                          
                   
         
      corr_xsum = ((double)(values_cnt - 1)) * ((double)values_cnt) / 2.0;
      corr_x2sum = ((double)(values_cnt - 1)) * ((double)values_cnt) * (double)(2 * values_cnt - 1) / 6.0;

                                                      
      corrs[0] = (values_cnt * corr_xysum - corr_xsum * corr_xsum) / (values_cnt * corr_x2sum - corr_xsum * corr_xsum);

      stats->stakind[slot_idx] = STATISTIC_KIND_CORRELATION;
      stats->staop[slot_idx] = mystats->ltopr;
      stats->stacoll[slot_idx] = stats->attrcollid;
      stats->stanumbers[slot_idx] = corrs;
      stats->numnumbers[slot_idx] = 1;
      slot_idx++;
    }
  }
  else if (nonnull_cnt > 0)
  {
                                                                   
    Assert(nonnull_cnt == toowide_cnt);
    stats->stats_valid = true;
                                                 
    stats->stanullfrac = (double)null_cnt / (double)samplerows;
    if (is_varwidth)
    {
      stats->stawidth = total_width / (double)nonnull_cnt;
    }
    else
    {
      stats->stawidth = stats->attrtype->typlen;
    }
                                                                          
    stats->stadistinct = -1.0 * (1.0 - stats->stanullfrac);
  }
  else if (null_cnt > 0)
  {
                                                                 
    stats->stats_valid = true;
    stats->stanullfrac = 1.0;
    if (is_varwidth)
    {
      stats->stawidth = 0;                
    }
    else
    {
      stats->stawidth = stats->attrtype->typlen;
    }
    stats->stadistinct = 0.0;                
  }

                                                                         
}

   
                                                
   
                                                                 
                                                                          
                                                                     
                                                                            
                                                                 
   
static int
compare_scalars(const void *a, const void *b, void *arg)
{
  Datum da = ((const ScalarItem *)a)->value;
  int ta = ((const ScalarItem *)a)->tupno;
  Datum db = ((const ScalarItem *)b)->value;
  int tb = ((const ScalarItem *)b)->tupno;
  CompareScalarsContext *cxt = (CompareScalarsContext *)arg;
  int compare;

  compare = ApplySortComparator(da, false, db, false, cxt->ssup);
  if (compare != 0)
  {
    return compare;
  }

     
                                                           
     
  if (cxt->tupnoLink[ta] < tb)
  {
    cxt->tupnoLink[ta] = tb;
  }
  if (cxt->tupnoLink[tb] < ta)
  {
    cxt->tupnoLink[tb] = ta;
  }

     
                                     
     
  return ta - tb;
}

   
                                                           
   
static int
compare_mcvs(const void *a, const void *b)
{
  int da = ((const ScalarMCVItem *)a)->first;
  int db = ((const ScalarMCVItem *)b)->first;

  return da - db;
}

   
                                                                           
                                          
   
                                                                              
                                                                               
                                                                              
                                                                         
   
static int
analyze_mcv_list(int *mcv_counts, int num_mcv, double stadistinct, double stanullfrac, int samplerows, double totalrows)
{
  double ndistinct_table;
  double sumcount;
  int i;

     
                                                                      
                                                             
     
  if (samplerows == totalrows || totalrows <= 1.0)
  {
    return num_mcv;
  }

                                                                           
  ndistinct_table = stadistinct;
  if (ndistinct_table < 0)
  {
    ndistinct_table = -ndistinct_table * totalrows;
  }

     
                                                                        
                                                                         
                                                                             
                                                                          
                                                                    
     
                                                                             
                                                                             
                               
     
                                                                        
                                                                      
                                                                            
                                                                           
                                                                          
                                                                            
                                                        
     
  sumcount = 0.0;
  for (i = 0; i < num_mcv - 1; i++)
  {
    sumcount += mcv_counts[i];
  }

  while (num_mcv > 0)
  {
    double selec, otherdistinct, N, n, K, variance, stddev;

       
                                                                     
                                              
       
    selec = 1.0 - sumcount / samplerows - stanullfrac;
    if (selec < 0.0)
    {
      selec = 0.0;
    }
    if (selec > 1.0)
    {
      selec = 1.0;
    }
    otherdistinct = ndistinct_table - (num_mcv - 1);
    if (otherdistinct > 1)
    {
      selec /= otherdistinct;
    }

       
                                                                         
                                                                         
                                                                      
                                                                       
                                                                        
                                                                         
                                                                        
                                                                       
                                                                          
                                              
       
                                                                        
                                                                        
                                                                   
       
    N = totalrows;
    n = samplerows;
    K = N * mcv_counts[num_mcv - 1] / n;
    variance = n * K * (N - K) * (N - n) / (N * N * (N - 1));
    stddev = sqrt(variance);

    if (mcv_counts[num_mcv - 1] > selec * samplerows + 2 * stddev + 0.5)
    {
         
                                                                 
                                                                     
                                    
         
      break;
    }
    else
    {
                                                                       
      num_mcv--;
      if (num_mcv == 0)
      {
        break;
      }
      sumcount -= mcv_counts[num_mcv - 1];
    }
  }
  return num_mcv;
}
