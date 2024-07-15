                                                                            
   
            
                                  
   
                                                                        
                                                                     
                                                                          
                 
   
   
                                                                         
                                                                        
   
   
                  
                                   
   
                                                                            
   
#include "postgres.h"

#include <math.h>

#include "access/clog.h"
#include "access/commit_ts.h"
#include "access/genam.h"
#include "access/heapam.h"
#include "access/htup_details.h"
#include "access/multixact.h"
#include "access/tableam.h"
#include "access/transam.h"
#include "access/xact.h"
#include "catalog/indexing.h"
#include "catalog/namespace.h"
#include "catalog/pg_database.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_namespace.h"
#include "commands/cluster.h"
#include "commands/defrem.h"
#include "commands/vacuum.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "pgstat.h"
#include "postmaster/autovacuum.h"
#include "storage/bufmgr.h"
#include "storage/lmgr.h"
#include "storage/proc.h"
#include "storage/procarray.h"
#include "utils/acl.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/memutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

   
                  
   
int vacuum_freeze_min_age;
int vacuum_freeze_table_age;
int vacuum_multixact_freeze_min_age;
int vacuum_multixact_freeze_table_age;

                                                                        
static MemoryContext vac_context = NULL;
static BufferAccessStrategy vac_strategy;

                                    
static List *
expand_vacuum_rel(VacuumRelation *vrel, int options);
static List *
get_all_vacuum_rels(int options);
static void
vac_truncate_clog(TransactionId frozenXID, MultiXactId minMulti, TransactionId lastSaneFrozenXid, MultiXactId lastSaneMinMulti);
static bool
vacuum_rel(Oid relid, RangeVar *relation, VacuumParams *params);
static VacOptTernaryValue
get_vacopt_ternary_value(DefElem *def);

   
                                                              
   
                                                                          
                       
   
void
ExecVacuum(ParseState *pstate, VacuumStmt *vacstmt, bool isTopLevel)
{
  VacuumParams params;
  bool verbose = false;
  bool skip_locked = false;
  bool analyze = false;
  bool freeze = false;
  bool full = false;
  bool disable_page_skipping = false;
  ListCell *lc;

                         
  params.index_cleanup = VACOPT_TERNARY_DEFAULT;
  params.truncate = VACOPT_TERNARY_DEFAULT;

                          
  foreach (lc, vacstmt->options)
  {
    DefElem *opt = (DefElem *)lfirst(lc);

                                                     
    if (strcmp(opt->defname, "verbose") == 0)
    {
      verbose = defGetBoolean(opt);
    }
    else if (strcmp(opt->defname, "skip_locked") == 0)
    {
      skip_locked = defGetBoolean(opt);
    }
    else if (!vacstmt->is_vacuumcmd)
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("unrecognized ANALYZE option \"%s\"", opt->defname), parser_errposition(pstate, opt->location)));
    }

                                           
    else if (strcmp(opt->defname, "analyze") == 0)
    {
      analyze = defGetBoolean(opt);
    }
    else if (strcmp(opt->defname, "freeze") == 0)
    {
      freeze = defGetBoolean(opt);
    }
    else if (strcmp(opt->defname, "full") == 0)
    {
      full = defGetBoolean(opt);
    }
    else if (strcmp(opt->defname, "disable_page_skipping") == 0)
    {
      disable_page_skipping = defGetBoolean(opt);
    }
    else if (strcmp(opt->defname, "index_cleanup") == 0)
    {
      params.index_cleanup = get_vacopt_ternary_value(opt);
    }
    else if (strcmp(opt->defname, "truncate") == 0)
    {
      params.truncate = get_vacopt_ternary_value(opt);
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("unrecognized VACUUM option \"%s\"", opt->defname), parser_errposition(pstate, opt->location)));
    }
  }

                          
  params.options = (vacstmt->is_vacuumcmd ? VACOPT_VACUUM : VACOPT_ANALYZE) | (verbose ? VACOPT_VERBOSE : 0) | (skip_locked ? VACOPT_SKIP_LOCKED : 0) | (analyze ? VACOPT_ANALYZE : 0) | (freeze ? VACOPT_FREEZE : 0) | (full ? VACOPT_FULL : 0) | (disable_page_skipping ? VACOPT_DISABLE_PAGE_SKIPPING : 0);

                                
  Assert(params.options & (VACOPT_VACUUM | VACOPT_ANALYZE));
  Assert((params.options & VACOPT_VACUUM) || !(params.options & (VACOPT_FULL | VACOPT_FREEZE)));
  Assert(!(params.options & VACOPT_SKIPTOAST));

     
                                                                            
     
  if (!(params.options & VACOPT_ANALYZE))
  {
    ListCell *lc;

    foreach (lc, vacstmt->rels)
    {
      VacuumRelation *vrel = lfirst_node(VacuumRelation, lc);

      if (vrel->va_cols != NIL)
      {
        ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("ANALYZE option must be specified when a column list is provided")));
      }
    }
  }

     
                                                                            
                                                       
     
  if (params.options & VACOPT_FREEZE)
  {
    params.freeze_min_age = 0;
    params.freeze_table_age = 0;
    params.multixact_freeze_min_age = 0;
    params.multixact_freeze_table_age = 0;
  }
  else
  {
    params.freeze_min_age = -1;
    params.freeze_table_age = -1;
    params.multixact_freeze_min_age = -1;
    params.multixact_freeze_table_age = -1;
  }

                                                     
  params.is_wraparound = false;

                                                     
  params.log_min_duration = -1;

                                         
  vacuum(vacstmt->rels, &params, NULL, isTopLevel);
}

   
                                                         
   
                                                                             
                                                                             
                                                                           
                                                                       
   
                                                                         
             
   
                                                                           
                                                                             
   
                                                         
   
                                                                            
                                                                 
   
void
vacuum(List *relations, VacuumParams *params, BufferAccessStrategy bstrategy, bool isTopLevel)
{
  static bool in_vacuum = false;

  const char *stmttype;
  volatile bool in_outer_xact, use_own_xacts;

  Assert(params != NULL);

  stmttype = (params->options & VACOPT_VACUUM) ? "VACUUM" : "ANALYZE";

     
                                                                             
                                                                         
                                                                         
                                
     
                                                  
     
  if (params->options & VACOPT_VACUUM)
  {
    PreventInTransactionBlock(isTopLevel, stmttype);
    in_outer_xact = false;
  }
  else
  {
    in_outer_xact = IsInTransactionBlock(isTopLevel);
  }

     
                                                                        
                                                                          
                                                                 
     
  if (in_vacuum)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("%s cannot be executed from VACUUM or ANALYZE", stmttype)));
  }

     
                                                
     
  if ((params->options & VACOPT_FULL) != 0 && (params->options & VACOPT_DISABLE_PAGE_SKIPPING) != 0)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("VACUUM option DISABLE_PAGE_SKIPPING cannot be used with FULL")));
  }

     
                                                                             
                                                          
     
  if ((params->options & VACOPT_VACUUM) && !IsAutoVacuumWorkerProcess())
  {
    pgstat_vacuum_stat();
  }

     
                                                                  
     
                                                                           
                                                                             
     
  vac_context = AllocSetContextCreate(PortalContext, "Vacuum", ALLOCSET_DEFAULT_SIZES);

     
                                                                        
                                       
     
  if (bstrategy == NULL)
  {
    MemoryContext old_context = MemoryContextSwitchTo(vac_context);

    bstrategy = GetAccessStrategy(BAS_VACUUM);
    MemoryContextSwitchTo(old_context);
  }
  vac_strategy = bstrategy;

     
                                                                   
                                  
     
  if (relations != NIL)
  {
    List *newrels = NIL;
    ListCell *lc;

    foreach (lc, relations)
    {
      VacuumRelation *vrel = lfirst_node(VacuumRelation, lc);
      List *sublist;
      MemoryContext old_context;

      sublist = expand_vacuum_rel(vrel, params->options);
      old_context = MemoryContextSwitchTo(vac_context);
      newrels = list_concat(newrels, sublist);
      MemoryContextSwitchTo(old_context);
    }
    relations = newrels;
  }
  else
  {
    relations = get_all_vacuum_rels(params->options);
  }

     
                                                                  
     
                                                                        
                                                                          
                                                                            
                   
     
                                                                       
                                                                           
                                                                             
                                                                  
                                                  
     
  if (params->options & VACOPT_VACUUM)
  {
    use_own_xacts = true;
  }
  else
  {
    Assert(params->options & VACOPT_ANALYZE);
    if (IsAutoVacuumWorkerProcess())
    {
      use_own_xacts = true;
    }
    else if (in_outer_xact)
    {
      use_own_xacts = false;
    }
    else if (list_length(relations) > 1)
    {
      use_own_xacts = true;
    }
    else
    {
      use_own_xacts = false;
    }
  }

     
                                                                          
                                                                        
                                                                       
                                                                      
                                                                           
                     
     
  if (use_own_xacts)
  {
    Assert(!in_outer_xact);

                                                 
    if (ActiveSnapshotSet())
    {
      PopActiveSnapshot();
    }

                                                        
    CommitTransactionCommand();
  }

                                                                      
  PG_TRY();
  {
    ListCell *cur;

    in_vacuum = true;
    VacuumCostActive = (VacuumCostDelay > 0);
    VacuumCostBalance = 0;
    VacuumPageHit = 0;
    VacuumPageMiss = 0;
    VacuumPageDirty = 0;

       
                                               
       
    foreach (cur, relations)
    {
      VacuumRelation *vrel = lfirst_node(VacuumRelation, cur);

      if (params->options & VACOPT_VACUUM)
      {
        if (!vacuum_rel(vrel->oid, vrel->relation, params))
        {
          continue;
        }
      }

      if (params->options & VACOPT_ANALYZE)
      {
           
                                                                      
                                             
           
        if (use_own_xacts)
        {
          StartTransactionCommand();
                                                            
          PushActiveSnapshot(GetTransactionSnapshot());
        }

        analyze_rel(vrel->oid, vrel->relation, params, vrel->va_cols, in_outer_xact, vac_strategy);

        if (use_own_xacts)
        {
          PopActiveSnapshot();
          CommitTransactionCommand();
        }
        else
        {
             
                                                                    
                                                                     
                                  
             
          CommandCounterIncrement();
        }
      }
    }
  }
  PG_CATCH();
  {
    in_vacuum = false;
    VacuumCostActive = false;
    PG_RE_THROW();
  }
  PG_END_TRY();

  in_vacuum = false;
  VacuumCostActive = false;

     
                           
     
  if (use_own_xacts)
  {
                                           

       
                                                            
                       
       
    StartTransactionCommand();
  }

  if ((params->options & VACOPT_VACUUM) && !IsAutoVacuumWorkerProcess())
  {
       
                                                                          
                                            
       
    vac_update_datfrozenxid();
  }

     
                                                             
                                                                           
              
     
  MemoryContextDelete(vac_context);
  vac_context = NULL;
}

   
                                                                         
                                                                          
                                                                       
                                                                          
            
   
bool
vacuum_is_relation_owner(Oid relid, Form_pg_class reltuple, int options)
{
  char *relname;

  Assert((options & (VACOPT_VACUUM | VACOPT_ANALYZE)) != 0);

     
                        
     
                                                                            
                                                                         
                                                                    
                     
     
                                                                       
                                                                             
     
  if (pg_class_ownercheck(relid, GetUserId()) || (pg_database_ownercheck(MyDatabaseId, GetUserId()) && !reltuple->relisshared))
  {
    return true;
  }

  relname = NameStr(reltuple->relname);

  if ((options & VACOPT_VACUUM) != 0)
  {
    if (reltuple->relisshared)
    {
      ereport(WARNING, (errmsg("skipping \"%s\" --- only superuser can vacuum it", relname)));
    }
    else if (reltuple->relnamespace == PG_CATALOG_NAMESPACE)
    {
      ereport(WARNING, (errmsg("skipping \"%s\" --- only superuser or database owner can vacuum it", relname)));
    }
    else
    {
      ereport(WARNING, (errmsg("skipping \"%s\" --- only table or database owner can vacuum it", relname)));
    }

       
                                                                      
                                                                   
                  
       
    return false;
  }

  if ((options & VACOPT_ANALYZE) != 0)
  {
    if (reltuple->relisshared)
    {
      ereport(WARNING, (errmsg("skipping \"%s\" --- only superuser can analyze it", relname)));
    }
    else if (reltuple->relnamespace == PG_CATALOG_NAMESPACE)
    {
      ereport(WARNING, (errmsg("skipping \"%s\" --- only superuser or database owner can analyze it", relname)));
    }
    else
    {
      ereport(WARNING, (errmsg("skipping \"%s\" --- only table or database owner can analyze it", relname)));
    }
  }

  return false;
}

   
                        
   
                                                                         
                                                                          
                                            
   
Relation
vacuum_open_relation(Oid relid, RangeVar *relation, int options, bool verbose, LOCKMODE lmode)
{
  Relation onerel;
  bool rel_lock = true;
  int elevel;

  Assert((options & (VACOPT_VACUUM | VACOPT_ANALYZE)) != 0);

     
                                                           
     
                                                                          
                                                                             
     
                                                                             
                                                               
     
  if (!(options & VACOPT_SKIP_LOCKED))
  {
    onerel = try_relation_open(relid, lmode);
  }
  else if (ConditionalLockRelationOid(relid, lmode))
  {
    onerel = try_relation_open(relid, NoLock);
  }
  else
  {
    onerel = NULL;
    rel_lock = false;
  }

                                    
  if (onerel)
  {
    return onerel;
  }

     
                                                                    
                                 
     
                                                                          
                                                                          
                                                                         
                      
     
  if (relation == NULL)
  {
    return NULL;
  }

     
                              
     
                                                                      
                                                                            
                   
     
  if (!IsAutoVacuumWorkerProcess())
  {
    elevel = WARNING;
  }
  else if (verbose)
  {
    elevel = LOG;
  }
  else
  {
    return NULL;
  }

  if ((options & VACOPT_VACUUM) != 0)
  {
    if (!rel_lock)
    {
      ereport(elevel, (errcode(ERRCODE_LOCK_NOT_AVAILABLE), errmsg("skipping vacuum of \"%s\" --- lock not available", relation->relname)));
    }
    else
    {
      ereport(elevel, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("skipping vacuum of \"%s\" --- relation no longer exists", relation->relname)));
    }

       
                                                                      
                                                                   
                  
       
    return NULL;
  }

  if ((options & VACOPT_ANALYZE) != 0)
  {
    if (!rel_lock)
    {
      ereport(elevel, (errcode(ERRCODE_LOCK_NOT_AVAILABLE), errmsg("skipping analyze of \"%s\" --- lock not available", relation->relname)));
    }
    else
    {
      ereport(elevel, (errcode(ERRCODE_UNDEFINED_TABLE), errmsg("skipping analyze of \"%s\" --- relation no longer exists", relation->relname)));
    }
  }

  return NULL;
}

   
                                                                         
                                                                   
   
                                                                          
                                                                         
                                                                       
                                                     
   
                                                                          
                                                                        
                                                                         
                            
   
static List *
expand_vacuum_rel(VacuumRelation *vrel, int options)
{
  List *vacrels = NIL;
  MemoryContext oldcontext;

                                                                
  if (OidIsValid(vrel->oid))
  {
    oldcontext = MemoryContextSwitchTo(vac_context);
    vacrels = lappend(vacrels, vrel);
    MemoryContextSwitchTo(oldcontext);
  }
  else
  {
                                                                      
    Oid relid;
    HeapTuple tuple;
    Form_pg_class classForm;
    bool include_parts;
    int rvr_opts;

       
                                                                      
                                                 
       
    Assert(!IsAutoVacuumWorkerProcess());

       
                                                                          
                                                                           
                                                 
       
    rvr_opts = (options & VACOPT_SKIP_LOCKED) ? RVR_SKIP_LOCKED : 0;
    relid = RangeVarGetRelidExtended(vrel->relation, AccessShareLock, rvr_opts, NULL, NULL);

       
                                                                    
                                             
       
    if (!OidIsValid(relid))
    {
      if (options & VACOPT_VACUUM)
      {
        ereport(WARNING, (errcode(ERRCODE_LOCK_NOT_AVAILABLE), errmsg("skipping vacuum of \"%s\" --- lock not available", vrel->relation->relname)));
      }
      else
      {
        ereport(WARNING, (errcode(ERRCODE_LOCK_NOT_AVAILABLE), errmsg("skipping analyze of \"%s\" --- lock not available", vrel->relation->relname)));
      }
      return vacrels;
    }

       
                                                                    
                                            
       
    tuple = SearchSysCache1(RELOID, ObjectIdGetDatum(relid));
    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "cache lookup failed for relation %u", relid);
    }
    classForm = (Form_pg_class)GETSTRUCT(tuple);

       
                                                                         
              
       
    if (vacuum_is_relation_owner(relid, classForm, options))
    {
      oldcontext = MemoryContextSwitchTo(vac_context);
      vacrels = lappend(vacrels, makeVacuumRelation(vrel->relation, relid, vrel->va_cols));
      MemoryContextSwitchTo(oldcontext);
    }

    include_parts = (classForm->relkind == RELKIND_PARTITIONED_TABLE);
    ReleaseSysCache(tuple);

       
                                                                           
                                                                         
                                                                          
                                                                  
                                                                        
                                                                           
                                                            
       
    if (include_parts)
    {
      List *part_oids = find_all_inheritors(relid, NoLock, NULL);
      ListCell *part_lc;

      foreach (part_lc, part_oids)
      {
        Oid part_oid = lfirst_oid(part_lc);

        if (part_oid == relid)
        {
          continue;                            
        }

           
                                                                  
                                                                 
                  
           
        oldcontext = MemoryContextSwitchTo(vac_context);
        vacrels = lappend(vacrels, makeVacuumRelation(NULL, part_oid, vrel->va_cols));
        MemoryContextSwitchTo(oldcontext);
      }
    }

       
                                                                           
                                                                           
                                                                     
                                                                        
                                                                           
                                                                         
                                                                        
                                                                         
                    
       
    UnlockRelationOid(relid, AccessShareLock);
  }

  return vacrels;
}

   
                                                                  
                                                            
   
static List *
get_all_vacuum_rels(int options)
{
  List *vacrels = NIL;
  Relation pgclass;
  TableScanDesc scan;
  HeapTuple tuple;

  pgclass = table_open(RelationRelationId, AccessShareLock);

  scan = table_beginscan_catalog(pgclass, 0, NULL);

  while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    Form_pg_class classForm = (Form_pg_class)GETSTRUCT(tuple);
    MemoryContext oldcontext;
    Oid relid = classForm->oid;

                                       
    if (!vacuum_is_relation_owner(relid, classForm, options))
    {
      continue;
    }

       
                                                                           
                                                                        
             
       
    if (classForm->relkind != RELKIND_RELATION && classForm->relkind != RELKIND_MATVIEW && classForm->relkind != RELKIND_PARTITIONED_TABLE)
    {
      continue;
    }

       
                                                                          
                                                                       
                                                           
       
    oldcontext = MemoryContextSwitchTo(vac_context);
    vacrels = lappend(vacrels, makeVacuumRelation(NULL, relid, NIL));
    MemoryContextSwitchTo(oldcontext);
  }

  table_endscan(scan);
  table_close(pgclass, AccessShareLock);

  return vacrels;
}

   
                                                                           
   
                              
                                                                           
                                                          
                                                                 
                                       
                                                                 
                                                                             
                                                                              
                                                                          
                  
                                                                                
          
                                                                               
                                                                       
   
                                                                              
                   
   
void
vacuum_set_xid_limits(Relation rel, int freeze_min_age, int freeze_table_age, int multixact_freeze_min_age, int multixact_freeze_table_age, TransactionId *oldestXmin, TransactionId *freezeLimit, TransactionId *xidFullScanLimit, MultiXactId *multiXactCutoff, MultiXactId *mxactFullScanLimit)
{
  int freezemin;
  int mxid_freezemin;
  int effective_multixact_freeze_max_age;
  TransactionId limit;
  TransactionId safeLimit;
  MultiXactId oldestMxact;
  MultiXactId mxactLimit;
  MultiXactId safeMxactLimit;

     
                                                                             
                                                                         
                                                                             
                                                                             
                                                                         
                                                                        
                                        
     
  *oldestXmin = TransactionIdLimitedForOldSnapshots(GetOldestXmin(rel, PROCARRAY_FLAGS_VACUUM), rel);

  Assert(TransactionIdIsNormal(*oldestXmin));

     
                                                                             
                                                               
                                                                   
                                            
     
  freezemin = freeze_min_age;
  if (freezemin < 0)
  {
    freezemin = vacuum_freeze_min_age;
  }
  freezemin = Min(freezemin, autovacuum_freeze_max_age / 2);
  Assert(freezemin >= 0);

     
                                                                             
     
  limit = *oldestXmin - freezemin;
  if (!TransactionIdIsNormal(limit))
  {
    limit = FirstNormalTransactionId;
  }

     
                                                            
                                                                           
                         
     
  safeLimit = ReadNewTransactionId() - autovacuum_freeze_max_age;
  if (!TransactionIdIsNormal(safeLimit))
  {
    safeLimit = FirstNormalTransactionId;
  }

  if (TransactionIdPrecedes(limit, safeLimit))
  {
    ereport(WARNING, (errmsg("oldest xmin is far in the past"), errhint("Close open transactions soon to avoid wraparound problems.\n"
                                                                        "You might also need to commit or roll back old prepared transactions, or drop stale replication slots.")));
    limit = *oldestXmin;
  }

  *freezeLimit = limit;

     
                                                                      
                                                                             
                                      
     
  effective_multixact_freeze_max_age = MultiXactMemberFreezeThreshold();

     
                                                                        
                                                                          
                                                                          
                                                              
     
  mxid_freezemin = multixact_freeze_min_age;
  if (mxid_freezemin < 0)
  {
    mxid_freezemin = vacuum_multixact_freeze_min_age;
  }
  mxid_freezemin = Min(mxid_freezemin, effective_multixact_freeze_max_age / 2);
  Assert(mxid_freezemin >= 0);

                                                                         
  oldestMxact = GetOldestMultiXactId();
  mxactLimit = oldestMxact - mxid_freezemin;
  if (mxactLimit < FirstMultiXactId)
  {
    mxactLimit = FirstMultiXactId;
  }

  safeMxactLimit = ReadNextMultiXactId() - effective_multixact_freeze_max_age;
  if (safeMxactLimit < FirstMultiXactId)
  {
    safeMxactLimit = FirstMultiXactId;
  }

  if (MultiXactIdPrecedes(mxactLimit, safeMxactLimit))
  {
    ereport(WARNING, (errmsg("oldest multixact is far in the past"), errhint("Close open transactions with multixacts soon to avoid wraparound problems.")));
                                                                    
    if (MultiXactIdPrecedes(oldestMxact, safeMxactLimit))
    {
      mxactLimit = oldestMxact;
    }
    else
    {
      mxactLimit = safeMxactLimit;
    }
  }

  *multiXactCutoff = mxactLimit;

  if (xidFullScanLimit != NULL)
  {
    int freezetable;

    Assert(mxactFullScanLimit != NULL);

       
                                                                          
                                                                 
                                                                         
                                                                          
                                                      
       
    freezetable = freeze_table_age;
    if (freezetable < 0)
    {
      freezetable = vacuum_freeze_table_age;
    }
    freezetable = Min(freezetable, autovacuum_freeze_max_age * 0.95);
    Assert(freezetable >= 0);

       
                                                                           
                                   
       
    limit = ReadNewTransactionId() - freezetable;
    if (!TransactionIdIsNormal(limit))
    {
      limit = FirstNormalTransactionId;
    }

    *xidFullScanLimit = limit;

       
                                                                       
                                                  
                                                                        
                                                                         
                                                                         
                                                                        
       
    freezetable = multixact_freeze_table_age;
    if (freezetable < 0)
    {
      freezetable = vacuum_multixact_freeze_table_age;
    }
    freezetable = Min(freezetable, effective_multixact_freeze_max_age * 0.95);
    Assert(freezetable >= 0);

       
                                                                          
                                            
       
    mxactLimit = ReadNextMultiXactId() - freezetable;
    if (mxactLimit < FirstMultiXactId)
    {
      mxactLimit = FirstMultiXactId;
    }

    *mxactFullScanLimit = mxactLimit;
  }
  else
  {
    Assert(mxactFullScanLimit == NULL);
  }
}

   
                                                                             
   
                                                                          
                                                                           
                                                                      
                                                                         
                                                                
                                          
   
                                                                
                                            
   
double
vac_estimate_reltuples(Relation relation, BlockNumber total_pages, BlockNumber scanned_pages, double scanned_tuples)
{
  BlockNumber old_rel_pages = relation->rd_rel->relpages;
  double old_rel_tuples = relation->rd_rel->reltuples;
  double old_density;
  double unscanned_pages;
  double total_tuples;

                                                                
  if (scanned_pages >= total_pages)
  {
    return scanned_tuples;
  }

     
                                                                             
                                                                      
                                                                     
                
     
  if (scanned_pages == 0)
  {
    return old_rel_tuples;
  }

     
                                                                        
                                                                        
     
  if (old_rel_pages == 0)
  {
    return floor((scanned_tuples / scanned_pages) * total_pages + 0.5);
  }

     
                                                                         
                                                                      
                                                                             
                                                                   
     
  old_density = old_rel_tuples / old_rel_pages;
  unscanned_pages = (double)total_pages - (double)scanned_pages;
  total_tuples = old_density * unscanned_pages + scanned_tuples;
  return floor(total_tuples + 0.5);
}

   
                                                               
   
                                                                       
                                                                    
                                                                         
                                                          
   
                                                                   
                                                                     
                                                                          
                                                                        
                                                                        
                                                                      
                                                                       
                                                                            
   
                                                                       
                                                                           
                                                                      
                                
   
                                                                     
                                                                          
                                                                          
                                                                     
                                                                        
                                                                     
                                                                         
                                                                       
                                                                          
                                                                       
                                                                      
                      
   
                                                            
                                            
   
                                                  
   
void
vac_update_relstats(Relation relation, BlockNumber num_pages, double num_tuples, BlockNumber num_all_visible_pages, bool hasindex, TransactionId frozenxid, MultiXactId minmulti, bool in_outer_xact)
{
  Oid relid = RelationGetRelid(relation);
  Relation rd;
  HeapTuple ctup;
  Form_pg_class pgcform;
  bool dirty;

  rd = table_open(RelationRelationId, RowExclusiveLock);

                                                
  ctup = SearchSysCacheCopy1(RELOID, ObjectIdGetDatum(relid));
  if (!HeapTupleIsValid(ctup))
  {
    elog(ERROR, "pg_class entry for relid %u vanished during vacuuming", relid);
  }
  pgcform = (Form_pg_class)GETSTRUCT(ctup);

                                                          

  dirty = false;
  if (pgcform->relpages != (int32)num_pages)
  {
    pgcform->relpages = (int32)num_pages;
    dirty = true;
  }
  if (pgcform->reltuples != (float4)num_tuples)
  {
    pgcform->reltuples = (float4)num_tuples;
    dirty = true;
  }
  if (pgcform->relallvisible != (int32)num_all_visible_pages)
  {
    pgcform->relallvisible = (int32)num_all_visible_pages;
    dirty = true;
  }

                                                                          

  if (!in_outer_xact)
  {
       
                                                         
       
    if (pgcform->relhasindex && !hasindex)
    {
      pgcform->relhasindex = false;
      dirty = true;
    }

                                                                
    if (pgcform->relhasrules && relation->rd_rules == NULL)
    {
      pgcform->relhasrules = false;
      dirty = true;
    }
    if (pgcform->relhastriggers && relation->trigdesc == NULL)
    {
      pgcform->relhastriggers = false;
      dirty = true;
    }
  }

     
                                                                    
                                    
     
                                                                       
                                                                            
                                                                        
                                                                           
                                                                            
                                                                          
                                                                             
                            
     
  if (TransactionIdIsNormal(frozenxid) && pgcform->relfrozenxid != frozenxid && (TransactionIdPrecedes(pgcform->relfrozenxid, frozenxid) || TransactionIdPrecedes(ReadNewTransactionId(), pgcform->relfrozenxid)))
  {
    pgcform->relfrozenxid = frozenxid;
    dirty = true;
  }

                                
  if (MultiXactIdIsValid(minmulti) && pgcform->relminmxid != minmulti && (MultiXactIdPrecedes(pgcform->relminmxid, minmulti) || MultiXactIdPrecedes(ReadNextMultiXactId(), pgcform->relminmxid)))
  {
    pgcform->relminmxid = minmulti;
    dirty = true;
  }

                                                 
  if (dirty)
  {
    heap_inplace_update(rd, ctup);
  }

  table_close(rd, RowExclusiveLock);
}

   
                                                                           
   
                                                                       
                                                 
   
                                                              
                                
   
                                                                    
                                       
   
                                                                        
                                                                        
                                                                          
                                                                           
                           
   
void
vac_update_datfrozenxid(void)
{
  HeapTuple tuple;
  Form_pg_database dbform;
  Relation relation;
  SysScanDesc scan;
  HeapTuple classTup;
  TransactionId newFrozenXid;
  MultiXactId newMinMulti;
  TransactionId lastSaneFrozenXid;
  MultiXactId lastSaneMinMulti;
  bool bogus = false;
  bool dirty = false;
  ScanKeyData key[1];

     
                                                                       
                                                                         
                                                                        
                                                                 
     
  LockDatabaseFrozenIds(ExclusiveLock);

     
                                                                     
                                                                       
                                                                           
                                                                 
     
  newFrozenXid = GetOldestXmin(NULL, PROCARRAY_FLAGS_VACUUM);

     
                                                                            
                                                                  
     
  newMinMulti = GetOldestMultiXactId();

     
                                                                          
                                                                           
                                               
     
  lastSaneFrozenXid = ReadNewTransactionId();
  lastSaneMinMulti = ReadNextMultiXactId();

     
                                                                           
                                  
     
  relation = table_open(RelationRelationId, AccessShareLock);

  scan = systable_beginscan(relation, InvalidOid, false, NULL, 0, NULL);

  while ((classTup = systable_getnext(scan)) != NULL)
  {
    Form_pg_class classForm = (Form_pg_class)GETSTRUCT(classTup);

       
                                                                         
                                                                 
       
    if (classForm->relkind != RELKIND_RELATION && classForm->relkind != RELKIND_MATVIEW && classForm->relkind != RELKIND_TOASTVALUE)
    {
      Assert(!TransactionIdIsValid(classForm->relfrozenxid));
      Assert(!MultiXactIdIsValid(classForm->relminmxid));
      continue;
    }

       
                                                                           
                                                                          
                                                               
                                                                         
            
       
                                                                 
                                                                          
                                                                        
                                                                          
                                                                        
                                                                
       

    if (TransactionIdIsValid(classForm->relfrozenxid))
    {
      Assert(TransactionIdIsNormal(classForm->relfrozenxid));

                                          
      if (TransactionIdPrecedes(lastSaneFrozenXid, classForm->relfrozenxid))
      {
        bogus = true;
        break;
      }

                                 
      if (TransactionIdPrecedes(classForm->relfrozenxid, newFrozenXid))
      {
        newFrozenXid = classForm->relfrozenxid;
      }
    }

    if (MultiXactIdIsValid(classForm->relminmxid))
    {
                                          
      if (MultiXactIdPrecedes(lastSaneMinMulti, classForm->relminmxid))
      {
        bogus = true;
        break;
      }

                                 
      if (MultiXactIdPrecedes(classForm->relminmxid, newMinMulti))
      {
        newMinMulti = classForm->relminmxid;
      }
    }
  }

                                
  systable_endscan(scan);
  table_close(relation, AccessShareLock);

                                       
  if (bogus)
  {
    return;
  }

  Assert(TransactionIdIsNormal(newFrozenXid));
  Assert(MultiXactIdIsValid(newMinMulti));

                                                          
  relation = table_open(DatabaseRelationId, RowExclusiveLock);

     
                                                                        
                                                                        
                                     
     
  ScanKeyInit(&key[0], Anum_pg_database_oid, BTEqualStrategyNumber, F_OIDEQ, ObjectIdGetDatum(MyDatabaseId));

  scan = systable_beginscan(relation, DatabaseOidIndexId, true, NULL, 1, key);
  tuple = systable_getnext(scan);
  tuple = heap_copytuple(tuple);
  systable_endscan(scan);

  if (!HeapTupleIsValid(tuple))
  {
    elog(ERROR, "could not find tuple for database %u", MyDatabaseId);
  }

  dbform = (Form_pg_database)GETSTRUCT(tuple);

     
                                                                  
                                                                           
                                                
     
  if (dbform->datfrozenxid != newFrozenXid && (TransactionIdPrecedes(dbform->datfrozenxid, newFrozenXid) || TransactionIdPrecedes(lastSaneFrozenXid, dbform->datfrozenxid)))
  {
    dbform->datfrozenxid = newFrozenXid;
    dirty = true;
  }
  else
  {
    newFrozenXid = dbform->datfrozenxid;
  }

                            
  if (dbform->datminmxid != newMinMulti && (MultiXactIdPrecedes(dbform->datminmxid, newMinMulti) || MultiXactIdPrecedes(lastSaneMinMulti, dbform->datminmxid)))
  {
    dbform->datminmxid = newMinMulti;
    dirty = true;
  }
  else
  {
    newMinMulti = dbform->datminmxid;
  }

  if (dirty)
  {
    heap_inplace_update(relation, tuple);
  }

  heap_freetuple(tuple);
  table_close(relation, RowExclusiveLock);

     
                                                                          
                                                                     
                                                                           
     
  if (dirty || ForceTransactionIdLimitUpdate())
  {
    vac_truncate_clog(newFrozenXid, newMinMulti, lastSaneFrozenXid, lastSaneMinMulti);
  }
}

   
                                                             
   
                                                                       
                                                                 
                                                                
                             
   
                                                                        
                                                                          
                                                                      
                           
   
                                                                  
                                                                     
                                  
   
static void
vac_truncate_clog(TransactionId frozenXID, MultiXactId minMulti, TransactionId lastSaneFrozenXid, MultiXactId lastSaneMinMulti)
{
  TransactionId nextXID = ReadNewTransactionId();
  Relation relation;
  TableScanDesc scan;
  HeapTuple tuple;
  Oid oldestxid_datoid;
  Oid minmulti_datoid;
  bool bogus = false;
  bool frozenAlreadyWrapped = false;

                                                                          
  LWLockAcquire(WrapLimitsVacuumLock, LW_EXCLUSIVE);

                                                                     
  oldestxid_datoid = MyDatabaseId;
  minmulti_datoid = MyDatabaseId;

     
                                                                     
     
                                                                             
                                                                         
                                                                         
                                                                             
                         
     
                                                                           
                                                                           
                                                                           
                                                                         
                                                                            
                                                                        
                                                                             
                     
     
  relation = table_open(DatabaseRelationId, AccessShareLock);

  scan = table_beginscan_catalog(relation, 0, NULL);

  while ((tuple = heap_getnext(scan, ForwardScanDirection)) != NULL)
  {
    volatile FormData_pg_database *dbform = (Form_pg_database)GETSTRUCT(tuple);
    TransactionId datfrozenxid = dbform->datfrozenxid;
    TransactionId datminmxid = dbform->datminmxid;

    Assert(TransactionIdIsNormal(datfrozenxid));
    Assert(MultiXactIdIsValid(datminmxid));

       
                                                                 
                                                                          
                                                                        
                                                                          
                                                                   
                                                                       
                                                          
       
    if (TransactionIdPrecedes(lastSaneFrozenXid, datfrozenxid) || MultiXactIdPrecedes(lastSaneMinMulti, datminmxid))
    {
      bogus = true;
    }

    if (TransactionIdPrecedes(nextXID, datfrozenxid))
    {
      frozenAlreadyWrapped = true;
    }
    else if (TransactionIdPrecedes(datfrozenxid, frozenXID))
    {
      frozenXID = datfrozenxid;
      oldestxid_datoid = dbform->oid;
    }

    if (MultiXactIdPrecedes(datminmxid, minMulti))
    {
      minMulti = datminmxid;
      minmulti_datoid = dbform->oid;
    }
  }

  table_endscan(scan);

  table_close(relation, AccessShareLock);

     
                                                                          
                                                                       
                                                                            
                  
     
  if (frozenAlreadyWrapped)
  {
    ereport(WARNING, (errmsg("some databases have not been vacuumed in over 2 billion transactions"), errdetail("You might have already suffered transaction-wraparound data loss.")));
    return;
  }

                                                     
  if (bogus)
  {
    return;
  }

     
                                                                          
                                                                            
                                                                             
                                                                       
                                                                   
     
  AdvanceOldestCommitTsXid(frozenXID);

     
                                                                         
     
  TruncateCLOG(frozenXID, oldestxid_datoid);
  TruncateCommitTs(frozenXID);
  TruncateMultiXact(minMulti, minmulti_datoid);

     
                                                                       
                                                                          
                                                                           
                       
     
  SetTransactionIdLimit(frozenXID, oldestxid_datoid);
  SetMultiXactIdLimit(minMulti, minmulti_datoid, false);

  LWLockRelease(WrapLimitsVacuumLock);
}

   
                                            
   
                                                                       
                                                                         
                                                                         
              
   
                                                                  
                             
   
                                                                     
                                                                    
                                                                     
                                                                         
                                                                          
   
                                                        
   
static bool
vacuum_rel(Oid relid, RangeVar *relation, VacuumParams *params)
{
  LOCKMODE lmode;
  Relation onerel;
  LockRelId onerelid;
  Oid toast_relid;
  Oid save_userid;
  int save_sec_context;
  int save_nestlevel;

  Assert(params != NULL);

                                                       
  StartTransactionCommand();

     
                                                                             
                                                         
     
  PushActiveSnapshot(GetTransactionSnapshot());

  if (!(params->options & VACOPT_FULL))
  {
       
                                                                      
                                                                         
                                                                           
                                                                   
                                                                          
                                                                         
                                                                         
                                                                          
                                                 
       
                                                                           
                                                                          
                        
       
                                                               
                                                                     
                                                                         
                                   
       
    LWLockAcquire(ProcArrayLock, LW_EXCLUSIVE);
    MyPgXact->vacuumFlags |= PROC_IN_VACUUM;
    if (params->is_wraparound)
    {
      MyPgXact->vacuumFlags |= PROC_VACUUM_FOR_WRAPAROUND;
    }
    LWLockRelease(ProcArrayLock);
  }

     
                                                                       
                                                           
     
  CHECK_FOR_INTERRUPTS();

     
                                                                           
                                                                             
                                                                            
     
  lmode = (params->options & VACOPT_FULL) ? AccessExclusiveLock : ShareUpdateExclusiveLock;

                                                            
  onerel = vacuum_open_relation(relid, relation, params->options, params->log_min_duration >= 0, lmode);

                                                       
  if (!onerel)
  {
    PopActiveSnapshot();
    CommitTransactionCommand();
    return false;
  }

     
                                                                           
                                                                         
                                                                       
                                                                             
                                                                             
           
     
  if (!vacuum_is_relation_owner(RelationGetRelid(onerel), onerel->rd_rel, params->options & VACOPT_VACUUM))
  {
    relation_close(onerel, lmode);
    PopActiveSnapshot();
    CommitTransactionCommand();
    return false;
  }

     
                                              
     
  if (onerel->rd_rel->relkind != RELKIND_RELATION && onerel->rd_rel->relkind != RELKIND_MATVIEW && onerel->rd_rel->relkind != RELKIND_TOASTVALUE && onerel->rd_rel->relkind != RELKIND_PARTITIONED_TABLE)
  {
    ereport(WARNING, (errmsg("skipping \"%s\" --- cannot vacuum non-tables or special system tables", RelationGetRelationName(onerel))));
    relation_close(onerel, lmode);
    PopActiveSnapshot();
    CommitTransactionCommand();
    return false;
  }

     
                                                                       
                                                                        
                                                                      
                                                                        
              
     
  if (RELATION_IS_OTHER_TEMP(onerel))
  {
    relation_close(onerel, lmode);
    PopActiveSnapshot();
    CommitTransactionCommand();
    return false;
  }

     
                                                                             
                                                                             
                    
     
  if (onerel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE)
  {
    relation_close(onerel, lmode);
    PopActiveSnapshot();
    CommitTransactionCommand();
                                                       
    return true;
  }

     
                                                                       
                                                                      
                                                                            
                                   
     
                                                                          
                                                                         
                   
     
  onerelid = onerel->rd_lockInfo.lockRelId;
  LockRelationIdForSession(&onerelid, lmode);

                                                               
  if (params->index_cleanup == VACOPT_TERNARY_DEFAULT)
  {
    if (onerel->rd_options == NULL || ((StdRdOptions *)onerel->rd_options)->vacuum_index_cleanup)
    {
      params->index_cleanup = VACOPT_TERNARY_ENABLED;
    }
    else
    {
      params->index_cleanup = VACOPT_TERNARY_DISABLED;
    }
  }

                                                          
  if (params->truncate == VACOPT_TERNARY_DEFAULT)
  {
    if (onerel->rd_options == NULL || ((StdRdOptions *)onerel->rd_options)->vacuum_truncate)
    {
      params->truncate = VACOPT_TERNARY_ENABLED;
    }
    else
    {
      params->truncate = VACOPT_TERNARY_DISABLED;
    }
  }

     
                                                                           
                                                                   
                                                                         
     
  if (!(params->options & VACOPT_SKIPTOAST) && !(params->options & VACOPT_FULL))
  {
    toast_relid = onerel->rd_rel->reltoastrelid;
  }
  else
  {
    toast_relid = InvalidOid;
  }

     
                                                                             
                                                                      
                                                                          
                                                  
     
  GetUserIdAndSecContext(&save_userid, &save_sec_context);
  SetUserIdAndSecContext(onerel->rd_rel->relowner, save_sec_context | SECURITY_RESTRICTED_OPERATION);
  save_nestlevel = NewGUCNestLevel();

     
                                                         
     
  if (params->options & VACOPT_FULL)
  {
    int cluster_options = 0;

                                                                     
    relation_close(onerel, NoLock);
    onerel = NULL;

    if ((params->options & VACOPT_VERBOSE) != 0)
    {
      cluster_options |= CLUOPT_VERBOSE;
    }

                                                                
    cluster_rel(relid, InvalidOid, cluster_options);
  }
  else
  {
    table_relation_vacuum(onerel, params, vac_strategy);
  }

                                                             
  AtEOXact_GUC(false, save_nestlevel);

                                           
  SetUserIdAndSecContext(save_userid, save_sec_context);

                                                            
  if (onerel)
  {
    relation_close(onerel, NoLock);
  }

     
                                                                  
     
  PopActiveSnapshot();
  CommitTransactionCommand();

     
                                                                         
                                                                         
                                                                            
                                                                       
                                              
     
  if (toast_relid != InvalidOid)
  {
    vacuum_rel(toast_relid, NULL, params);
  }

     
                                                             
     
  UnlockRelationIdForSession(&onerelid, lmode);

                                     
  return true;
}

   
                                                                        
                                                                             
                                                                     
   
                                                                            
                                                                             
                                                                          
                                                                             
                                                                               
                                
   
void
vac_open_indexes(Relation relation, LOCKMODE lockmode, int *nindexes, Relation **Irel)
{
  List *indexoidlist;
  ListCell *indexoidscan;
  int i;

  Assert(lockmode != NoLock);

  indexoidlist = RelationGetIndexList(relation);

                                              
  i = list_length(indexoidlist);

  if (i > 0)
  {
    *Irel = (Relation *)palloc(i * sizeof(Relation));
  }
  else
  {
    *Irel = NULL;
  }

                                      
  i = 0;
  foreach (indexoidscan, indexoidlist)
  {
    Oid indexoid = lfirst_oid(indexoidscan);
    Relation indrel;

    indrel = index_open(indexoid, lockmode);
    if (indrel->rd_index->indisready)
    {
      (*Irel)[i++] = indrel;
    }
    else
    {
      index_close(indrel, lockmode);
    }
  }

  *nindexes = i;

  list_free(indexoidlist);
}

   
                                                                           
                                       
   
void
vac_close_indexes(int nindexes, Relation *Irel, LOCKMODE lockmode)
{
  if (Irel == NULL)
  {
    return;
  }

  while (nindexes--)
  {
    Relation ind = Irel[nindexes];

    index_close(ind, lockmode);
  }
  pfree(Irel);
}

   
                                                                     
   
                                                                  
                                      
   
void
vacuum_delay_point(void)
{
                                   
  CHECK_FOR_INTERRUPTS();

                          
  if (VacuumCostActive && !InterruptPending && VacuumCostBalance >= VacuumCostLimit)
  {
    double msec;

    msec = VacuumCostDelay * VacuumCostBalance / VacuumCostLimit;
    if (msec > VacuumCostDelay * 4)
    {
      msec = VacuumCostDelay * 4;
    }

    pg_usleep((long)(msec * 1000));

    VacuumCostBalance = 0;

                                           
    AutoVacuumUpdateDelay();

                                                       
    CHECK_FOR_INTERRUPTS();
  }
}

   
                                          
   
                                                                            
                              
   
static VacOptTernaryValue
get_vacopt_ternary_value(DefElem *def)
{
  return defGetBoolean(def) ? VACOPT_TERNARY_ENABLED : VACOPT_TERNARY_DISABLED;
}
