                                                                            
   
                 
   
                                                                   
           
   
                                                                       
                                                                         
                                                                  
                                                                        
                                                                   
                                                                             
                                             
   
   
                                                                         
   
                                       
   
                                                                            
   

#include "postgres.h"

#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "access/tableam.h"
#include "access/xact.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_constraint.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_type.h"
#include "commands/trigger.h"
#include "executor/executor.h"
#include "executor/spi.h"
#include "lib/ilist.h"
#include "parser/parse_coerce.h"
#include "parser/parse_relation.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/fmgroids.h"
#include "utils/guc.h"
#include "utils/inval.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/rls.h"
#include "utils/ruleutils.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

   
                     
   

#define RI_MAX_NUMKEYS INDEX_MAX_KEYS

#define RI_INIT_CONSTRAINTHASHSIZE 64
#define RI_INIT_QUERYHASHSIZE (RI_INIT_CONSTRAINTHASHSIZE * 4)

#define RI_KEYS_ALL_NULL 0
#define RI_KEYS_SOME_NULL 1
#define RI_KEYS_NONE_NULL 2

                         
                                                                   
#define RI_PLAN_CHECK_LOOKUPPK 1
#define RI_PLAN_CHECK_LOOKUPPK_FROM_PK 2
#define RI_PLAN_LAST_ON_PK RI_PLAN_CHECK_LOOKUPPK_FROM_PK
                                                                    
#define RI_PLAN_CASCADE_DEL_DODELETE 3
#define RI_PLAN_CASCADE_UPD_DOUPDATE 4
#define RI_PLAN_RESTRICT_CHECKREF 5
#define RI_PLAN_SETNULL_DOUPDATE 6
#define RI_PLAN_SETDEFAULT_DOUPDATE 7

#define MAX_QUOTED_NAME_LEN (NAMEDATALEN * 2 + 3)
#define MAX_QUOTED_REL_NAME_LEN (MAX_QUOTED_NAME_LEN * 2)

#define RIAttName(rel, attnum) NameStr(*attnumAttName(rel, attnum))
#define RIAttType(rel, attnum) attnumTypeId(rel, attnum)
#define RIAttCollation(rel, attnum) attnumCollationId(rel, attnum)

#define RI_TRIGTYPE_INSERT 1
#define RI_TRIGTYPE_UPDATE 2
#define RI_TRIGTYPE_DELETE 3

   
                     
   
                                                                            
                        
   
typedef struct RI_ConstraintInfo
{
  Oid constraint_id;                                                           
  bool valid;                                                      
  uint32 oidHashValue;                                                   
  NameData conname;                                                
  Oid pk_relid;                                              
  Oid fk_relid;                                               
  char confupdtype;                                                     
  char confdeltype;                                                     
  char confmatchtype;                                             
  int nkeys;                                                   
  int16 pk_attnums[RI_MAX_NUMKEYS];                                 
  int16 fk_attnums[RI_MAX_NUMKEYS];                                  
  Oid pf_eq_oprs[RI_MAX_NUMKEYS];                                     
  Oid pp_eq_oprs[RI_MAX_NUMKEYS];                                     
  Oid ff_eq_oprs[RI_MAX_NUMKEYS];                                     
  dlist_node valid_link;                                               
} RI_ConstraintInfo;

   
               
   
                                                                  
   
typedef struct RI_QueryKey
{
  Oid constr_id;                                        
  int32 constr_queryno;                                           
} RI_QueryKey;

   
                     
   
typedef struct RI_QueryHashEntry
{
  RI_QueryKey key;
  SPIPlanPtr plan;
} RI_QueryHashEntry;

   
                 
   
                                                                  
   
typedef struct RI_CompareKey
{
  Oid eq_opr;                                     
  Oid typeid;                                   
} RI_CompareKey;

   
                       
   
typedef struct RI_CompareHashEntry
{
  RI_CompareKey key;
  bool valid;                                              
  FmgrInfo eq_opr_finfo;                                   
  FmgrInfo cast_func_finfo;                                   
} RI_CompareHashEntry;

   
              
   
static HTAB *ri_constraint_cache = NULL;
static HTAB *ri_query_cache = NULL;
static HTAB *ri_compare_cache = NULL;
static dlist_head ri_constraint_cache_valid_list;
static int ri_constraint_cache_valid_count = 0;

   
                             
   
static bool
ri_Check_Pk_Match(Relation pk_rel, Relation fk_rel, TupleTableSlot *oldslot, const RI_ConstraintInfo *riinfo);
static Datum
ri_restrict(TriggerData *trigdata, bool is_no_action);
static Datum
ri_set(TriggerData *trigdata, bool is_set_null);
static void
quoteOneName(char *buffer, const char *name);
static void
quoteRelationName(char *buffer, Relation rel);
static void
ri_GenerateQual(StringInfo buf, const char *sep, const char *leftop, Oid leftoptype, Oid opoid, const char *rightop, Oid rightoptype);
static void
ri_GenerateQualCollation(StringInfo buf, Oid collation);
static int
ri_NullCheck(TupleDesc tupdesc, TupleTableSlot *slot, const RI_ConstraintInfo *riinfo, bool rel_is_pk);
static void
ri_BuildQueryKey(RI_QueryKey *key, const RI_ConstraintInfo *riinfo, int32 constr_queryno);
static bool
ri_KeysEqual(Relation rel, TupleTableSlot *oldslot, TupleTableSlot *newslot, const RI_ConstraintInfo *riinfo, bool rel_is_pk);
static bool
ri_AttributesEqual(Oid eq_opr, Oid typeid, Datum oldvalue, Datum newvalue);

static void
ri_InitHashTables(void);
static void
InvalidateConstraintCacheCallBack(Datum arg, int cacheid, uint32 hashvalue);
static SPIPlanPtr
ri_FetchPreparedPlan(RI_QueryKey *key);
static void
ri_HashPreparedPlan(RI_QueryKey *key, SPIPlanPtr plan);
static RI_CompareHashEntry *
ri_HashCompareOp(Oid eq_opr, Oid typeid);

static void
ri_CheckTrigger(FunctionCallInfo fcinfo, const char *funcname, int tgkind);
static const RI_ConstraintInfo *
ri_FetchConstraintInfo(Trigger *trigger, Relation trig_rel, bool rel_is_pk);
static const RI_ConstraintInfo *
ri_LoadConstraintInfo(Oid constraintOid);
static SPIPlanPtr
ri_PlanCheck(const char *querystr, int nargs, Oid *argtypes, RI_QueryKey *qkey, Relation fk_rel, Relation pk_rel, bool cache_plan);
static bool
ri_PerformCheck(const RI_ConstraintInfo *riinfo, RI_QueryKey *qkey, SPIPlanPtr qplan, Relation fk_rel, Relation pk_rel, TupleTableSlot *oldslot, TupleTableSlot *newslot, bool detectNewRows, int expect_OK);
static void
ri_ExtractValues(Relation rel, TupleTableSlot *slot, const RI_ConstraintInfo *riinfo, bool rel_is_pk, Datum *vals, char *nulls);
static void
ri_ReportViolation(const RI_ConstraintInfo *riinfo, Relation pk_rel, Relation fk_rel, TupleTableSlot *violatorslot, TupleDesc tupdesc, int queryno, bool partgone) pg_attribute_noreturn();

   
                   
   
                                                                 
   
static Datum
RI_FKey_check(TriggerData *trigdata)
{
  const RI_ConstraintInfo *riinfo;
  Relation fk_rel;
  Relation pk_rel;
  TupleTableSlot *newslot;
  RI_QueryKey qkey;
  SPIPlanPtr qplan;

  riinfo = ri_FetchConstraintInfo(trigdata->tg_trigger, trigdata->tg_relation, false);

  if (TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
  {
    newslot = trigdata->tg_newslot;
  }
  else
  {
    newslot = trigdata->tg_trigslot;
  }

     
                                                                            
                                                                           
                                                                            
                                                                          
                                                                          
                                          
     
  if (!table_tuple_satisfies_snapshot(trigdata->tg_relation, newslot, SnapshotSelf))
  {
    return PointerGetDatum(NULL);
  }

     
                                                           
     
                                                                          
                                          
     
  fk_rel = trigdata->tg_relation;
  pk_rel = table_open(riinfo->pk_relid, RowShareLock);

  switch (ri_NullCheck(RelationGetDescr(fk_rel), newslot, riinfo, false))
  {
  case RI_KEYS_ALL_NULL:

       
                                                                      
                               
       
    table_close(pk_rel, RowShareLock);
    return PointerGetDatum(NULL);

  case RI_KEYS_SOME_NULL:

       
                                                                     
              
       
    switch (riinfo->confmatchtype)
    {
    case FKCONSTR_MATCH_FULL:

         
                                                                 
                                 
         
      ereport(ERROR, (errcode(ERRCODE_FOREIGN_KEY_VIOLATION), errmsg("insert or update on table \"%s\" violates foreign key constraint \"%s\"", RelationGetRelationName(fk_rel), NameStr(riinfo->conname)), errdetail("MATCH FULL does not allow mixing of null and nonnull key values."), errtableconstraint(fk_rel, NameStr(riinfo->conname))));
      table_close(pk_rel, RowShareLock);
      return PointerGetDatum(NULL);

    case FKCONSTR_MATCH_SIMPLE:

         
                                                              
                         
         
      table_close(pk_rel, RowShareLock);
      return PointerGetDatum(NULL);

#ifdef NOT_USED
    case FKCONSTR_MATCH_PARTIAL:

         
                                                               
                                                               
                                                           
                               
         
      break;
#endif
    }

  case RI_KEYS_NONE_NULL:

       
                                                                      
                 
       
    break;
  }

  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "SPI_connect failed");
  }

                                                        
  ri_BuildQueryKey(&qkey, riinfo, RI_PLAN_CHECK_LOOKUPPK);

  if ((qplan = ri_FetchPreparedPlan(&qkey)) == NULL)
  {
    StringInfoData querybuf;
    char pkrelname[MAX_QUOTED_REL_NAME_LEN];
    char attname[MAX_QUOTED_NAME_LEN];
    char paramname[16];
    const char *querysep;
    Oid queryoids[RI_MAX_NUMKEYS];
    const char *pk_only;

                  
                                 
                                                                    
                              
                                                           
                                    
                  
       
    initStringInfo(&querybuf);
    pk_only = pk_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE ? "" : "ONLY ";
    quoteRelationName(pkrelname, pk_rel);
    appendStringInfo(&querybuf, "SELECT 1 FROM %s%s x", pk_only, pkrelname);
    querysep = "WHERE";
    for (int i = 0; i < riinfo->nkeys; i++)
    {
      Oid pk_type = RIAttType(pk_rel, riinfo->pk_attnums[i]);
      Oid fk_type = RIAttType(fk_rel, riinfo->fk_attnums[i]);

      quoteOneName(attname, RIAttName(pk_rel, riinfo->pk_attnums[i]));
      sprintf(paramname, "$%d", i + 1);
      ri_GenerateQual(&querybuf, querysep, attname, pk_type, riinfo->pf_eq_oprs[i], paramname, fk_type);
      querysep = "AND";
      queryoids[i] = fk_type;
    }
    appendStringInfoString(&querybuf, " FOR KEY SHARE OF x");

                                   
    qplan = ri_PlanCheck(querybuf.data, riinfo->nkeys, queryoids, &qkey, fk_rel, pk_rel, true);
  }

     
                                                   
     
  ri_PerformCheck(riinfo, &qkey, qplan, fk_rel, pk_rel, NULL, newslot, false, SPI_OK_SELECT);

  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish failed");
  }

  table_close(pk_rel, RowShareLock);

  return PointerGetDatum(NULL);
}

   
                       
   
                                                            
   
Datum
RI_FKey_check_ins(PG_FUNCTION_ARGS)
{
                                                                            
  ri_CheckTrigger(fcinfo, "RI_FKey_check_ins", RI_TRIGTYPE_INSERT);

                                    
  return RI_FKey_check((TriggerData *)fcinfo->context);
}

   
                       
   
                                                            
   
Datum
RI_FKey_check_upd(PG_FUNCTION_ARGS)
{
                                                                            
  ri_CheckTrigger(fcinfo, "RI_FKey_check_upd", RI_TRIGTYPE_UPDATE);

                                    
  return RI_FKey_check((TriggerData *)fcinfo->context);
}

   
                     
   
                                                                          
                                                                              
                                                             
   
                                                                              
                                          
   
static bool
ri_Check_Pk_Match(Relation pk_rel, Relation fk_rel, TupleTableSlot *oldslot, const RI_ConstraintInfo *riinfo)
{
  SPIPlanPtr qplan;
  RI_QueryKey qkey;
  bool result;

                                     
  Assert(ri_NullCheck(RelationGetDescr(pk_rel), oldslot, riinfo, true) == RI_KEYS_NONE_NULL);

  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "SPI_connect failed");
  }

     
                                                                            
                   
     
  ri_BuildQueryKey(&qkey, riinfo, RI_PLAN_CHECK_LOOKUPPK_FROM_PK);

  if ((qplan = ri_FetchPreparedPlan(&qkey)) == NULL)
  {
    StringInfoData querybuf;
    char pkrelname[MAX_QUOTED_REL_NAME_LEN];
    char attname[MAX_QUOTED_NAME_LEN];
    char paramname[16];
    const char *querysep;
    const char *pk_only;
    Oid queryoids[RI_MAX_NUMKEYS];

                  
                                 
                                                                    
                              
                                                           
                                 
                  
       
    initStringInfo(&querybuf);
    pk_only = pk_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE ? "" : "ONLY ";
    quoteRelationName(pkrelname, pk_rel);
    appendStringInfo(&querybuf, "SELECT 1 FROM %s%s x", pk_only, pkrelname);
    querysep = "WHERE";
    for (int i = 0; i < riinfo->nkeys; i++)
    {
      Oid pk_type = RIAttType(pk_rel, riinfo->pk_attnums[i]);

      quoteOneName(attname, RIAttName(pk_rel, riinfo->pk_attnums[i]));
      sprintf(paramname, "$%d", i + 1);
      ri_GenerateQual(&querybuf, querysep, attname, pk_type, riinfo->pp_eq_oprs[i], paramname, pk_type);
      querysep = "AND";
      queryoids[i] = pk_type;
    }
    appendStringInfoString(&querybuf, " FOR KEY SHARE OF x");

                                   
    qplan = ri_PlanCheck(querybuf.data, riinfo->nkeys, queryoids, &qkey, fk_rel, pk_rel, true);
  }

     
                                 
     
  result = ri_PerformCheck(riinfo, &qkey, qplan, fk_rel, pk_rel, oldslot, NULL, true,                        
      SPI_OK_SELECT);

  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish failed");
  }

  return result;
}

   
                          
   
                                                              
                                                               
                         
   
Datum
RI_FKey_noaction_del(PG_FUNCTION_ARGS)
{
                                                                            
  ri_CheckTrigger(fcinfo, "RI_FKey_noaction_del", RI_TRIGTYPE_DELETE);

                                              
  return ri_restrict((TriggerData *)fcinfo->context, true);
}

   
                          
   
                                                                      
   
                                                                            
                                                                   
                                                                        
                                                                         
   
Datum
RI_FKey_restrict_del(PG_FUNCTION_ARGS)
{
                                                                            
  ri_CheckTrigger(fcinfo, "RI_FKey_restrict_del", RI_TRIGTYPE_DELETE);

                                               
  return ri_restrict((TriggerData *)fcinfo->context, false);
}

   
                          
   
                                                              
                                                               
                         
   
Datum
RI_FKey_noaction_upd(PG_FUNCTION_ARGS)
{
                                                                            
  ri_CheckTrigger(fcinfo, "RI_FKey_noaction_upd", RI_TRIGTYPE_UPDATE);

                                              
  return ri_restrict((TriggerData *)fcinfo->context, true);
}

   
                          
   
                                                              
   
                                                                            
                                                                   
                                                                        
                                                                         
   
Datum
RI_FKey_restrict_upd(PG_FUNCTION_ARGS)
{
                                                                            
  ri_CheckTrigger(fcinfo, "RI_FKey_restrict_upd", RI_TRIGTYPE_UPDATE);

                                               
  return ri_restrict((TriggerData *)fcinfo->context, false);
}

   
                 
   
                                                            
                                                
   
static Datum
ri_restrict(TriggerData *trigdata, bool is_no_action)
{
  const RI_ConstraintInfo *riinfo;
  Relation fk_rel;
  Relation pk_rel;
  TupleTableSlot *oldslot;
  RI_QueryKey qkey;
  SPIPlanPtr qplan;

  riinfo = ri_FetchConstraintInfo(trigdata->tg_trigger, trigdata->tg_relation, true);

     
                                                                             
     
                                                                          
                                          
     
  fk_rel = table_open(riinfo->fk_relid, RowShareLock);
  pk_rel = trigdata->tg_relation;
  oldslot = trigdata->tg_trigslot;

     
                                                                          
                                                                         
                                                                             
                  
     
  if (is_no_action && ri_Check_Pk_Match(pk_rel, fk_rel, oldslot, riinfo))
  {
    table_close(fk_rel, RowShareLock);
    return PointerGetDatum(NULL);
  }

  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "SPI_connect failed");
  }

     
                                                                          
                                        
     
  ri_BuildQueryKey(&qkey, riinfo, RI_PLAN_RESTRICT_CHECKREF);

  if ((qplan = ri_FetchPreparedPlan(&qkey)) == NULL)
  {
    StringInfoData querybuf;
    char fkrelname[MAX_QUOTED_REL_NAME_LEN];
    char attname[MAX_QUOTED_NAME_LEN];
    char paramname[16];
    const char *querysep;
    Oid queryoids[RI_MAX_NUMKEYS];
    const char *fk_only;

                  
                                 
                                                                    
                              
                                                           
                                    
                  
       
    initStringInfo(&querybuf);
    fk_only = fk_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE ? "" : "ONLY ";
    quoteRelationName(fkrelname, fk_rel);
    appendStringInfo(&querybuf, "SELECT 1 FROM %s%s x", fk_only, fkrelname);
    querysep = "WHERE";
    for (int i = 0; i < riinfo->nkeys; i++)
    {
      Oid pk_type = RIAttType(pk_rel, riinfo->pk_attnums[i]);
      Oid fk_type = RIAttType(fk_rel, riinfo->fk_attnums[i]);
      Oid pk_coll = RIAttCollation(pk_rel, riinfo->pk_attnums[i]);
      Oid fk_coll = RIAttCollation(fk_rel, riinfo->fk_attnums[i]);

      quoteOneName(attname, RIAttName(fk_rel, riinfo->fk_attnums[i]));
      sprintf(paramname, "$%d", i + 1);
      ri_GenerateQual(&querybuf, querysep, paramname, pk_type, riinfo->pf_eq_oprs[i], attname, fk_type);
      if (pk_coll != fk_coll && !get_collation_isdeterministic(pk_coll))
      {
        ri_GenerateQualCollation(&querybuf, pk_coll);
      }
      querysep = "AND";
      queryoids[i] = pk_type;
    }
    appendStringInfoString(&querybuf, " FOR KEY SHARE OF x");

                                   
    qplan = ri_PlanCheck(querybuf.data, riinfo->nkeys, queryoids, &qkey, fk_rel, pk_rel, true);
  }

     
                                                                  
     
  ri_PerformCheck(riinfo, &qkey, qplan, fk_rel, pk_rel, oldslot, NULL, true,                           
      SPI_OK_SELECT);

  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish failed");
  }

  table_close(fk_rel, RowShareLock);

  return PointerGetDatum(NULL);
}

   
                         
   
                                                                       
   
Datum
RI_FKey_cascade_del(PG_FUNCTION_ARGS)
{
  TriggerData *trigdata = (TriggerData *)fcinfo->context;
  const RI_ConstraintInfo *riinfo;
  Relation fk_rel;
  Relation pk_rel;
  TupleTableSlot *oldslot;
  RI_QueryKey qkey;
  SPIPlanPtr qplan;

                                                                            
  ri_CheckTrigger(fcinfo, "RI_FKey_cascade_del", RI_TRIGTYPE_DELETE);

  riinfo = ri_FetchConstraintInfo(trigdata->tg_trigger, trigdata->tg_relation, true);

     
                                                                             
     
                                                                     
                                     
     
  fk_rel = table_open(riinfo->fk_relid, RowExclusiveLock);
  pk_rel = trigdata->tg_relation;
  oldslot = trigdata->tg_trigslot;

  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "SPI_connect failed");
  }

                                                             
  ri_BuildQueryKey(&qkey, riinfo, RI_PLAN_CASCADE_DEL_DODELETE);

  if ((qplan = ri_FetchPreparedPlan(&qkey)) == NULL)
  {
    StringInfoData querybuf;
    char fkrelname[MAX_QUOTED_REL_NAME_LEN];
    char attname[MAX_QUOTED_NAME_LEN];
    char paramname[16];
    const char *querysep;
    Oid queryoids[RI_MAX_NUMKEYS];
    const char *fk_only;

                  
                                 
                                                                
                                                           
                                    
                  
       
    initStringInfo(&querybuf);
    fk_only = fk_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE ? "" : "ONLY ";
    quoteRelationName(fkrelname, fk_rel);
    appendStringInfo(&querybuf, "DELETE FROM %s%s", fk_only, fkrelname);
    querysep = "WHERE";
    for (int i = 0; i < riinfo->nkeys; i++)
    {
      Oid pk_type = RIAttType(pk_rel, riinfo->pk_attnums[i]);
      Oid fk_type = RIAttType(fk_rel, riinfo->fk_attnums[i]);
      Oid pk_coll = RIAttCollation(pk_rel, riinfo->pk_attnums[i]);
      Oid fk_coll = RIAttCollation(fk_rel, riinfo->fk_attnums[i]);

      quoteOneName(attname, RIAttName(fk_rel, riinfo->fk_attnums[i]));
      sprintf(paramname, "$%d", i + 1);
      ri_GenerateQual(&querybuf, querysep, paramname, pk_type, riinfo->pf_eq_oprs[i], attname, fk_type);
      if (pk_coll != fk_coll && !get_collation_isdeterministic(pk_coll))
      {
        ri_GenerateQualCollation(&querybuf, pk_coll);
      }
      querysep = "AND";
      queryoids[i] = pk_type;
    }

                                   
    qplan = ri_PlanCheck(querybuf.data, riinfo->nkeys, queryoids, &qkey, fk_rel, pk_rel, true);
  }

     
                                                                           
                                                      
     
  ri_PerformCheck(riinfo, &qkey, qplan, fk_rel, pk_rel, oldslot, NULL, true,                           
      SPI_OK_DELETE);

  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish failed");
  }

  table_close(fk_rel, RowExclusiveLock);

  return PointerGetDatum(NULL);
}

   
                         
   
                                                                       
   
Datum
RI_FKey_cascade_upd(PG_FUNCTION_ARGS)
{
  TriggerData *trigdata = (TriggerData *)fcinfo->context;
  const RI_ConstraintInfo *riinfo;
  Relation fk_rel;
  Relation pk_rel;
  TupleTableSlot *newslot;
  TupleTableSlot *oldslot;
  RI_QueryKey qkey;
  SPIPlanPtr qplan;

                                                                            
  ri_CheckTrigger(fcinfo, "RI_FKey_cascade_upd", RI_TRIGTYPE_UPDATE);

  riinfo = ri_FetchConstraintInfo(trigdata->tg_trigger, trigdata->tg_relation, true);

     
                                                                          
                
     
                                                                     
                                     
     
  fk_rel = table_open(riinfo->fk_relid, RowExclusiveLock);
  pk_rel = trigdata->tg_relation;
  newslot = trigdata->tg_newslot;
  oldslot = trigdata->tg_trigslot;

  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "SPI_connect failed");
  }

                                                             
  ri_BuildQueryKey(&qkey, riinfo, RI_PLAN_CASCADE_UPD_DOUPDATE);

  if ((qplan = ri_FetchPreparedPlan(&qkey)) == NULL)
  {
    StringInfoData querybuf;
    StringInfoData qualbuf;
    char fkrelname[MAX_QUOTED_REL_NAME_LEN];
    char attname[MAX_QUOTED_NAME_LEN];
    char paramname[16];
    const char *querysep;
    const char *qualsep;
    Oid queryoids[RI_MAX_NUMKEYS * 2];
    const char *fk_only;

                  
                                 
                                                       
                                     
                                                           
                                                               
                                                               
                                  
                  
       
    initStringInfo(&querybuf);
    initStringInfo(&qualbuf);
    fk_only = fk_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE ? "" : "ONLY ";
    quoteRelationName(fkrelname, fk_rel);
    appendStringInfo(&querybuf, "UPDATE %s%s SET", fk_only, fkrelname);
    querysep = "";
    qualsep = "WHERE";
    for (int i = 0, j = riinfo->nkeys; i < riinfo->nkeys; i++, j++)
    {
      Oid pk_type = RIAttType(pk_rel, riinfo->pk_attnums[i]);
      Oid fk_type = RIAttType(fk_rel, riinfo->fk_attnums[i]);
      Oid pk_coll = RIAttCollation(pk_rel, riinfo->pk_attnums[i]);
      Oid fk_coll = RIAttCollation(fk_rel, riinfo->fk_attnums[i]);

      quoteOneName(attname, RIAttName(fk_rel, riinfo->fk_attnums[i]));
      appendStringInfo(&querybuf, "%s %s = $%d", querysep, attname, i + 1);
      sprintf(paramname, "$%d", j + 1);
      ri_GenerateQual(&qualbuf, qualsep, paramname, pk_type, riinfo->pf_eq_oprs[i], attname, fk_type);
      if (pk_coll != fk_coll && !get_collation_isdeterministic(pk_coll))
      {
        ri_GenerateQualCollation(&querybuf, pk_coll);
      }
      querysep = ",";
      qualsep = "AND";
      queryoids[i] = pk_type;
      queryoids[j] = pk_type;
    }
    appendStringInfoString(&querybuf, qualbuf.data);

                                   
    qplan = ri_PlanCheck(querybuf.data, riinfo->nkeys * 2, queryoids, &qkey, fk_rel, pk_rel, true);
  }

     
                                                                   
     
  ri_PerformCheck(riinfo, &qkey, qplan, fk_rel, pk_rel, oldslot, newslot, true,                           
      SPI_OK_UPDATE);

  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish failed");
  }

  table_close(fk_rel, RowExclusiveLock);

  return PointerGetDatum(NULL);
}

   
                         
   
                                                                          
   
Datum
RI_FKey_setnull_del(PG_FUNCTION_ARGS)
{
                                                                            
  ri_CheckTrigger(fcinfo, "RI_FKey_setnull_del", RI_TRIGTYPE_DELETE);

                                   
  return ri_set((TriggerData *)fcinfo->context, true);
}

   
                         
   
                                                                   
   
Datum
RI_FKey_setnull_upd(PG_FUNCTION_ARGS)
{
                                                                            
  ri_CheckTrigger(fcinfo, "RI_FKey_setnull_upd", RI_TRIGTYPE_UPDATE);

                                   
  return ri_set((TriggerData *)fcinfo->context, true);
}

   
                            
   
                                                                       
   
Datum
RI_FKey_setdefault_del(PG_FUNCTION_ARGS)
{
                                                                            
  ri_CheckTrigger(fcinfo, "RI_FKey_setdefault_del", RI_TRIGTYPE_DELETE);

                                   
  return ri_set((TriggerData *)fcinfo->context, false);
}

   
                            
   
                                                                       
   
Datum
RI_FKey_setdefault_upd(PG_FUNCTION_ARGS)
{
                                                                            
  ri_CheckTrigger(fcinfo, "RI_FKey_setdefault_upd", RI_TRIGTYPE_UPDATE);

                                   
  return ri_set((TriggerData *)fcinfo->context, false);
}

   
            
   
                                                                            
                                    
   
static Datum
ri_set(TriggerData *trigdata, bool is_set_null)
{
  const RI_ConstraintInfo *riinfo;
  Relation fk_rel;
  Relation pk_rel;
  TupleTableSlot *oldslot;
  RI_QueryKey qkey;
  SPIPlanPtr qplan;

  riinfo = ri_FetchConstraintInfo(trigdata->tg_trigger, trigdata->tg_relation, true);

     
                                                                             
     
                                                                     
                                     
     
  fk_rel = table_open(riinfo->fk_relid, RowExclusiveLock);
  pk_rel = trigdata->tg_relation;
  oldslot = trigdata->tg_trigslot;

  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "SPI_connect failed");
  }

     
                                                                            
                                                 
     
  ri_BuildQueryKey(&qkey, riinfo, (is_set_null ? RI_PLAN_SETNULL_DOUPDATE : RI_PLAN_SETDEFAULT_DOUPDATE));

  if ((qplan = ri_FetchPreparedPlan(&qkey)) == NULL)
  {
    StringInfoData querybuf;
    StringInfoData qualbuf;
    char fkrelname[MAX_QUOTED_REL_NAME_LEN];
    char attname[MAX_QUOTED_NAME_LEN];
    char paramname[16];
    const char *querysep;
    const char *qualsep;
    Oid queryoids[RI_MAX_NUMKEYS];
    const char *fk_only;

                  
                                 
                                                                   
                                     
                                                           
                                    
                  
       
    initStringInfo(&querybuf);
    initStringInfo(&qualbuf);
    fk_only = fk_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE ? "" : "ONLY ";
    quoteRelationName(fkrelname, fk_rel);
    appendStringInfo(&querybuf, "UPDATE %s%s SET", fk_only, fkrelname);
    querysep = "";
    qualsep = "WHERE";
    for (int i = 0; i < riinfo->nkeys; i++)
    {
      Oid pk_type = RIAttType(pk_rel, riinfo->pk_attnums[i]);
      Oid fk_type = RIAttType(fk_rel, riinfo->fk_attnums[i]);
      Oid pk_coll = RIAttCollation(pk_rel, riinfo->pk_attnums[i]);
      Oid fk_coll = RIAttCollation(fk_rel, riinfo->fk_attnums[i]);

      quoteOneName(attname, RIAttName(fk_rel, riinfo->fk_attnums[i]));
      appendStringInfo(&querybuf, "%s %s = %s", querysep, attname, is_set_null ? "NULL" : "DEFAULT");
      sprintf(paramname, "$%d", i + 1);
      ri_GenerateQual(&qualbuf, qualsep, paramname, pk_type, riinfo->pf_eq_oprs[i], attname, fk_type);
      if (pk_coll != fk_coll && !get_collation_isdeterministic(pk_coll))
      {
        ri_GenerateQualCollation(&querybuf, pk_coll);
      }
      querysep = ",";
      qualsep = "AND";
      queryoids[i] = pk_type;
    }
    appendStringInfoString(&querybuf, qualbuf.data);

                                   
    qplan = ri_PlanCheck(querybuf.data, riinfo->nkeys, queryoids, &qkey, fk_rel, pk_rel, true);
  }

     
                                                                   
     
  ri_PerformCheck(riinfo, &qkey, qplan, fk_rel, pk_rel, oldslot, NULL, true,                           
      SPI_OK_UPDATE);

  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish failed");
  }

  table_close(fk_rel, RowExclusiveLock);

  if (is_set_null)
  {
    return PointerGetDatum(NULL);
  }
  else
  {
       
                                                                           
                                                                          
                                                                           
                                                                        
                                                                       
                                                                    
                                                                           
                                                                           
                                                                          
                                                                        
                                                       
       
    return ri_restrict(trigdata, true);
  }
}

   
                                   
   
                                                                                  
                                                                          
                                                                          
                                                                          
                 
   
                                                        
   
bool
RI_FKey_pk_upd_check_required(Trigger *trigger, Relation pk_rel, TupleTableSlot *oldslot, TupleTableSlot *newslot)
{
  const RI_ConstraintInfo *riinfo;

  riinfo = ri_FetchConstraintInfo(trigger, pk_rel, true);

     
                                                                             
                                       
     
  if (ri_NullCheck(RelationGetDescr(pk_rel), oldslot, riinfo, true) != RI_KEYS_NONE_NULL)
  {
    return false;
  }

                                                                   
  if (newslot && ri_KeysEqual(pk_rel, oldslot, newslot, riinfo, true))
  {
    return false;
  }

                                         
  return true;
}

   
                                   
   
                                                                         
                                                                          
                                                                          
                                                                          
                 
   
bool
RI_FKey_fk_upd_check_required(Trigger *trigger, Relation fk_rel, TupleTableSlot *oldslot, TupleTableSlot *newslot)
{
  const RI_ConstraintInfo *riinfo;
  int ri_nullcheck;
  Datum xminDatum;
  TransactionId xmin;
  bool isnull;

  riinfo = ri_FetchConstraintInfo(trigger, fk_rel, false);

  ri_nullcheck = ri_NullCheck(RelationGetDescr(fk_rel), newslot, riinfo, false);

     
                                                                             
                      
     
  if (ri_nullcheck == RI_KEYS_ALL_NULL)
  {
    return false;
  }

     
                                                                        
           
     
  else if (ri_nullcheck == RI_KEYS_SOME_NULL)
  {
    switch (riinfo->confmatchtype)
    {
    case FKCONSTR_MATCH_SIMPLE:

         
                                                                
                                            
         
      return false;

    case FKCONSTR_MATCH_PARTIAL:

         
                                          
         
      break;

    case FKCONSTR_MATCH_FULL:

         
                                                            
                                                                    
                                                              
                                                                   
                
         
      return true;
    }
  }

     
                                                                          
          
     

     
                                                                           
                                                                         
                                                                          
                                                                            
                                                                            
                        
     
  xminDatum = slot_getsysattr(oldslot, MinTransactionIdAttributeNumber, &isnull);
  Assert(!isnull);
  xmin = DatumGetTransactionId(xminDatum);
  if (TransactionIdIsCurrentTransactionId(xmin))
  {
    return true;
  }

                                                                   
  if (ri_KeysEqual(fk_rel, oldslot, newslot, riinfo, false))
  {
    return false;
  }

                                         
  return true;
}

   
                      
   
                                                                       
                                                                     
                                                           
   
                                                                        
                                                                         
                                                                      
                                                             
                                                                
   
                                                                        
                                                                           
                                                      
   
bool
RI_Initial_Check(Trigger *trigger, Relation fk_rel, Relation pk_rel)
{
  const RI_ConstraintInfo *riinfo;
  StringInfoData querybuf;
  char pkrelname[MAX_QUOTED_REL_NAME_LEN];
  char fkrelname[MAX_QUOTED_REL_NAME_LEN];
  char pkattname[MAX_QUOTED_NAME_LEN + 3];
  char fkattname[MAX_QUOTED_NAME_LEN + 3];
  RangeTblEntry *pkrte;
  RangeTblEntry *fkrte;
  const char *sep;
  const char *fk_only;
  const char *pk_only;
  int save_nestlevel;
  char workmembuf[32];
  int spi_result;
  SPIPlanPtr qplan;

  riinfo = ri_FetchConstraintInfo(trigger, fk_rel, false);

     
                                                                           
                                                                        
                                                    
     
                                                               
     
  pkrte = makeNode(RangeTblEntry);
  pkrte->rtekind = RTE_RELATION;
  pkrte->relid = RelationGetRelid(pk_rel);
  pkrte->relkind = pk_rel->rd_rel->relkind;
  pkrte->rellockmode = AccessShareLock;
  pkrte->requiredPerms = ACL_SELECT;

  fkrte = makeNode(RangeTblEntry);
  fkrte->rtekind = RTE_RELATION;
  fkrte->relid = RelationGetRelid(fk_rel);
  fkrte->relkind = fk_rel->rd_rel->relkind;
  fkrte->rellockmode = AccessShareLock;
  fkrte->requiredPerms = ACL_SELECT;

  for (int i = 0; i < riinfo->nkeys; i++)
  {
    int attno;

    attno = riinfo->pk_attnums[i] - FirstLowInvalidHeapAttributeNumber;
    pkrte->selectedCols = bms_add_member(pkrte->selectedCols, attno);

    attno = riinfo->fk_attnums[i] - FirstLowInvalidHeapAttributeNumber;
    fkrte->selectedCols = bms_add_member(fkrte->selectedCols, attno);
  }

  if (!ExecCheckRTPerms(list_make2(fkrte, pkrte), false))
  {
    return false;
  }

     
                                                                          
                                                                          
                       
     
  if (!has_bypassrls_privilege(GetUserId()) && ((pk_rel->rd_rel->relrowsecurity && !pg_class_ownercheck(pkrte->relid, GetUserId())) || (fk_rel->rd_rel->relrowsecurity && !pg_class_ownercheck(fkrte->relid, GetUserId()))))
  {
    return false;
  }

               
                                
                                              
                                          
                                             
                                     
                       
                                         
                     
                                        
     
                                                                       
                                     
               
     
  initStringInfo(&querybuf);
  appendStringInfoString(&querybuf, "SELECT ");
  sep = "";
  for (int i = 0; i < riinfo->nkeys; i++)
  {
    quoteOneName(fkattname, RIAttName(fk_rel, riinfo->fk_attnums[i]));
    appendStringInfo(&querybuf, "%sfk.%s", sep, fkattname);
    sep = ", ";
  }

  quoteRelationName(pkrelname, pk_rel);
  quoteRelationName(fkrelname, fk_rel);
  fk_only = fk_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE ? "" : "ONLY ";
  pk_only = pk_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE ? "" : "ONLY ";
  appendStringInfo(&querybuf, " FROM %s%s fk LEFT OUTER JOIN %s%s pk ON", fk_only, fkrelname, pk_only, pkrelname);

  strcpy(pkattname, "pk.");
  strcpy(fkattname, "fk.");
  sep = "(";
  for (int i = 0; i < riinfo->nkeys; i++)
  {
    Oid pk_type = RIAttType(pk_rel, riinfo->pk_attnums[i]);
    Oid fk_type = RIAttType(fk_rel, riinfo->fk_attnums[i]);
    Oid pk_coll = RIAttCollation(pk_rel, riinfo->pk_attnums[i]);
    Oid fk_coll = RIAttCollation(fk_rel, riinfo->fk_attnums[i]);

    quoteOneName(pkattname + 3, RIAttName(pk_rel, riinfo->pk_attnums[i]));
    quoteOneName(fkattname + 3, RIAttName(fk_rel, riinfo->fk_attnums[i]));
    ri_GenerateQual(&querybuf, sep, pkattname, pk_type, riinfo->pf_eq_oprs[i], fkattname, fk_type);
    if (pk_coll != fk_coll)
    {
      ri_GenerateQualCollation(&querybuf, pk_coll);
    }
    sep = "AND";
  }

     
                                                                            
              
     
  quoteOneName(pkattname, RIAttName(pk_rel, riinfo->pk_attnums[0]));
  appendStringInfo(&querybuf, ") WHERE pk.%s IS NULL AND (", pkattname);

  sep = "";
  for (int i = 0; i < riinfo->nkeys; i++)
  {
    quoteOneName(fkattname, RIAttName(fk_rel, riinfo->fk_attnums[i]));
    appendStringInfo(&querybuf, "%sfk.%s IS NOT NULL", sep, fkattname);
    switch (riinfo->confmatchtype)
    {
    case FKCONSTR_MATCH_SIMPLE:
      sep = " AND ";
      break;
    case FKCONSTR_MATCH_FULL:
      sep = " OR ";
      break;
    }
  }
  appendStringInfoChar(&querybuf, ')');

     
                                                                           
                                                                             
                                                                           
                                                                         
                                                                          
                                                             
     
                                                                            
                                                                            
                                           
     
  save_nestlevel = NewGUCNestLevel();

  snprintf(workmembuf, sizeof(workmembuf), "%d", maintenance_work_mem);
  (void)set_config_option("work_mem", workmembuf, PGC_USERSET, PGC_S_SESSION, GUC_ACTION_SAVE, true, 0, false);

  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "SPI_connect failed");
  }

     
                                                                     
                            
     
  qplan = SPI_prepare(querybuf.data, 0, NULL);

  if (qplan == NULL)
  {
    elog(ERROR, "SPI_prepare returned %s for %s", SPI_result_code_string(SPI_result), querybuf.data);
  }

     
                                                                           
                                                                             
                                                                     
                                                                            
                                                         
     
  spi_result = SPI_execute_snapshot(qplan, NULL, NULL, GetLatestSnapshot(), InvalidSnapshot, true, false, 1);

                    
  if (spi_result != SPI_OK_SELECT)
  {
    elog(ERROR, "SPI_execute_snapshot returned %s", SPI_result_code_string(spi_result));
  }

                                                     
  if (SPI_processed > 0)
  {
    TupleTableSlot *slot;
    HeapTuple tuple = SPI_tuptable->vals[0];
    TupleDesc tupdesc = SPI_tuptable->tupdesc;
    RI_ConstraintInfo fake_riinfo;

    slot = MakeSingleTupleTableSlot(tupdesc, &TTSOpsVirtual);

    heap_deform_tuple(tuple, tupdesc, slot->tts_values, slot->tts_isnull);
    ExecStoreVirtualTuple(slot);

       
                                                                         
                                                                       
                                         
       
                                                                   
                                                                           
                            
       
    memcpy(&fake_riinfo, riinfo, sizeof(RI_ConstraintInfo));
    for (int i = 0; i < fake_riinfo.nkeys; i++)
    {
      fake_riinfo.fk_attnums[i] = i + 1;
    }

       
                                                                   
                                                                        
                                         
       
    if (fake_riinfo.confmatchtype == FKCONSTR_MATCH_FULL && ri_NullCheck(tupdesc, slot, &fake_riinfo, false) != RI_KEYS_NONE_NULL)
    {
      ereport(ERROR, (errcode(ERRCODE_FOREIGN_KEY_VIOLATION), errmsg("insert or update on table \"%s\" violates foreign key constraint \"%s\"", RelationGetRelationName(fk_rel), NameStr(fake_riinfo.conname)), errdetail("MATCH FULL does not allow mixing of null and nonnull key values."), errtableconstraint(fk_rel, NameStr(fake_riinfo.conname))));
    }

       
                                                                           
                                                         
                                          
       
    ri_ReportViolation(&fake_riinfo, pk_rel, fk_rel, slot, tupdesc, RI_PLAN_CHECK_LOOKUPPK, false);

    ExecDropSingleTupleTableSlot(slot);
  }

  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish failed");
  }

     
                       
     
  AtEOXact_GUC(true, save_nestlevel);

  return true;
}

   
                              
   
                                                                       
                                                    
   
void
RI_PartitionRemove_Check(Trigger *trigger, Relation fk_rel, Relation pk_rel)
{
  const RI_ConstraintInfo *riinfo;
  StringInfoData querybuf;
  char *constraintDef;
  char pkrelname[MAX_QUOTED_REL_NAME_LEN];
  char fkrelname[MAX_QUOTED_REL_NAME_LEN];
  char pkattname[MAX_QUOTED_NAME_LEN + 3];
  char fkattname[MAX_QUOTED_NAME_LEN + 3];
  const char *sep;
  const char *fk_only;
  int save_nestlevel;
  char workmembuf[32];
  int spi_result;
  SPIPlanPtr qplan;
  int i;

  riinfo = ri_FetchConstraintInfo(trigger, fk_rel, false);

     
                                                                            
                                                                       
                                                      
     

               
                                
                                               
                          
                                               
                                           
                       
                                          
                     
                                         
     
                                                                       
                                     
               
     
  initStringInfo(&querybuf);
  appendStringInfoString(&querybuf, "SELECT ");
  sep = "";
  for (i = 0; i < riinfo->nkeys; i++)
  {
    quoteOneName(fkattname, RIAttName(fk_rel, riinfo->fk_attnums[i]));
    appendStringInfo(&querybuf, "%sfk.%s", sep, fkattname);
    sep = ", ";
  }

  quoteRelationName(pkrelname, pk_rel);
  quoteRelationName(fkrelname, fk_rel);
  fk_only = fk_rel->rd_rel->relkind == RELKIND_PARTITIONED_TABLE ? "" : "ONLY ";
  appendStringInfo(&querybuf, " FROM %s%s fk JOIN %s pk ON", fk_only, fkrelname, pkrelname);
  strcpy(pkattname, "pk.");
  strcpy(fkattname, "fk.");
  sep = "(";
  for (i = 0; i < riinfo->nkeys; i++)
  {
    Oid pk_type = RIAttType(pk_rel, riinfo->pk_attnums[i]);
    Oid fk_type = RIAttType(fk_rel, riinfo->fk_attnums[i]);
    Oid pk_coll = RIAttCollation(pk_rel, riinfo->pk_attnums[i]);
    Oid fk_coll = RIAttCollation(fk_rel, riinfo->fk_attnums[i]);

    quoteOneName(pkattname + 3, RIAttName(pk_rel, riinfo->pk_attnums[i]));
    quoteOneName(fkattname + 3, RIAttName(fk_rel, riinfo->fk_attnums[i]));
    ri_GenerateQual(&querybuf, sep, pkattname, pk_type, riinfo->pf_eq_oprs[i], fkattname, fk_type);
    if (pk_coll != fk_coll)
    {
      ri_GenerateQualCollation(&querybuf, pk_coll);
    }
    sep = "AND";
  }

     
                                                                             
                                                                       
                                                             
     
  constraintDef = pg_get_partconstrdef_string(RelationGetRelid(pk_rel), "pk");
  if (constraintDef && constraintDef[0] != '\0')
  {
    appendStringInfo(&querybuf, ") WHERE %s AND (", constraintDef);
  }
  else
  {
    appendStringInfo(&querybuf, ") WHERE (");
  }

  sep = "";
  for (i = 0; i < riinfo->nkeys; i++)
  {
    quoteOneName(fkattname, RIAttName(fk_rel, riinfo->fk_attnums[i]));
    appendStringInfo(&querybuf, "%sfk.%s IS NOT NULL", sep, fkattname);
    switch (riinfo->confmatchtype)
    {
    case FKCONSTR_MATCH_SIMPLE:
      sep = " AND ";
      break;
    case FKCONSTR_MATCH_FULL:
      sep = " OR ";
      break;
    }
  }
  appendStringInfoChar(&querybuf, ')');

     
                                                                           
                                                                             
                                                                           
                                                                         
                                                                          
                                                             
     
                                                                            
                                                                            
                                           
     
  save_nestlevel = NewGUCNestLevel();

  snprintf(workmembuf, sizeof(workmembuf), "%d", maintenance_work_mem);
  (void)set_config_option("work_mem", workmembuf, PGC_USERSET, PGC_S_SESSION, GUC_ACTION_SAVE, true, 0, false);

  if (SPI_connect() != SPI_OK_CONNECT)
  {
    elog(ERROR, "SPI_connect failed");
  }

     
                                                                     
                            
     
  qplan = SPI_prepare(querybuf.data, 0, NULL);

  if (qplan == NULL)
  {
    elog(ERROR, "SPI_prepare returned %s for %s", SPI_result_code_string(SPI_result), querybuf.data);
  }

     
                                                                           
                                                                             
                                                                     
                                                                            
                                                         
     
  spi_result = SPI_execute_snapshot(qplan, NULL, NULL, GetLatestSnapshot(), InvalidSnapshot, true, false, 1);

                    
  if (spi_result != SPI_OK_SELECT)
  {
    elog(ERROR, "SPI_execute_snapshot returned %s", SPI_result_code_string(spi_result));
  }

                                                              
  if (SPI_processed > 0)
  {
    TupleTableSlot *slot;
    HeapTuple tuple = SPI_tuptable->vals[0];
    TupleDesc tupdesc = SPI_tuptable->tupdesc;
    RI_ConstraintInfo fake_riinfo;

    slot = MakeSingleTupleTableSlot(tupdesc, &TTSOpsVirtual);

    heap_deform_tuple(tuple, tupdesc, slot->tts_values, slot->tts_isnull);
    ExecStoreVirtualTuple(slot);

       
                                                                         
                                                                          
                             
       
                                                                   
                                                                           
                            
       
    memcpy(&fake_riinfo, riinfo, sizeof(RI_ConstraintInfo));
    for (i = 0; i < fake_riinfo.nkeys; i++)
    {
      fake_riinfo.pk_attnums[i] = i + 1;
    }

    ri_ReportViolation(&fake_riinfo, pk_rel, fk_rel, slot, tupdesc, 0, true);
  }

  if (SPI_finish() != SPI_OK_FINISH)
  {
    elog(ERROR, "SPI_finish failed");
  }

     
                       
     
  AtEOXact_GUC(true, save_nestlevel);
}

              
                         
              
   

   
                                                   
   
                                                                  
   
static void
quoteOneName(char *buffer, const char *name)
{
                                                             
  *buffer++ = '"';
  while (*name)
  {
    if (*name == '"')
    {
      *buffer++ = '"';
    }
    *buffer++ = *name++;
  }
  *buffer++ = '"';
  *buffer = '\0';
}

   
                                                                      
   
                                                                      
   
static void
quoteRelationName(char *buffer, Relation rel)
{
  quoteOneName(buffer, get_namespace_name(RelationGetNamespace(rel)));
  buffer += strlen(buffer);
  *buffer++ = '.';
  quoteOneName(buffer, RelationGetRelationName(rel));
}

   
                                                                      
   
                                                                        
                                                                            
                                                                        
                                           
   
static void
ri_GenerateQual(StringInfo buf, const char *sep, const char *leftop, Oid leftoptype, Oid opoid, const char *rightop, Oid rightoptype)
{
  appendStringInfo(buf, " %s ", sep);
  generate_operator_clause(buf, leftop, leftoptype, opoid, rightop, rightoptype);
}

   
                                                                     
   
                                                                             
                                                                              
                                                                          
                                                                               
                                                                            
                                                                         
                                                                        
                                                                            
                                                                              
                                                                            
                                                                         
                                                                            
                                              
   
static void
ri_GenerateQualCollation(StringInfo buf, Oid collation)
{
  HeapTuple tp;
  Form_pg_collation colltup;
  char *collname;
  char onename[MAX_QUOTED_NAME_LEN];

                                                       
  if (!OidIsValid(collation))
  {
    return;
  }

  tp = SearchSysCache1(COLLOID, ObjectIdGetDatum(collation));
  if (!HeapTupleIsValid(tp))
  {
    elog(ERROR, "cache lookup failed for collation %u", collation);
  }
  colltup = (Form_pg_collation)GETSTRUCT(tp);
  collname = NameStr(colltup->collname);

     
                                                                           
                                
     
  quoteOneName(onename, get_namespace_name(colltup->collnamespace));
  appendStringInfo(buf, " COLLATE %s", onename);
  quoteOneName(onename, collname);
  appendStringInfo(buf, ".%s", onename);

  ReleaseSysCache(tp);
}

              
                      
   
                                                                          
   
                                                                         
                                          
                                                                  
                                                 
              
   
static void
ri_BuildQueryKey(RI_QueryKey *key, const RI_ConstraintInfo *riinfo, int32 constr_queryno)
{
     
                                                                            
                                  
     
  key->constr_id = riinfo->constraint_id;
  key->constr_queryno = constr_queryno;
}

   
                                                                 
   
static void
ri_CheckTrigger(FunctionCallInfo fcinfo, const char *funcname, int tgkind)
{
  TriggerData *trigdata = (TriggerData *)fcinfo->context;

  if (!CALLED_AS_TRIGGER(fcinfo))
  {
    ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("function \"%s\" was not called by trigger manager", funcname)));
  }

     
                        
     
  if (!TRIGGER_FIRED_AFTER(trigdata->tg_event) || !TRIGGER_FIRED_FOR_ROW(trigdata->tg_event))
  {
    ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("function \"%s\" must be fired AFTER ROW", funcname)));
  }

  switch (tgkind)
  {
  case RI_TRIGTYPE_INSERT:
    if (!TRIGGER_FIRED_BY_INSERT(trigdata->tg_event))
    {
      ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("function \"%s\" must be fired for INSERT", funcname)));
    }
    break;
  case RI_TRIGTYPE_UPDATE:
    if (!TRIGGER_FIRED_BY_UPDATE(trigdata->tg_event))
    {
      ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("function \"%s\" must be fired for UPDATE", funcname)));
    }
    break;
  case RI_TRIGTYPE_DELETE:
    if (!TRIGGER_FIRED_BY_DELETE(trigdata->tg_event))
    {
      ereport(ERROR, (errcode(ERRCODE_E_R_I_E_TRIGGER_PROTOCOL_VIOLATED), errmsg("function \"%s\" must be fired for DELETE", funcname)));
    }
    break;
  }
}

   
                                                                       
   
static const RI_ConstraintInfo *
ri_FetchConstraintInfo(Trigger *trigger, Relation trig_rel, bool rel_is_pk)
{
  Oid constraintOid = trigger->tgconstraint;
  const RI_ConstraintInfo *riinfo;

     
                                                                         
                                                                            
               
     
  if (!OidIsValid(constraintOid))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_OBJECT_DEFINITION), errmsg("no pg_constraint entry for trigger \"%s\" on table \"%s\"", trigger->tgname, RelationGetRelationName(trig_rel)), errhint("Remove this referential integrity trigger and its mates, then do ALTER TABLE ADD CONSTRAINT.")));
  }

                                                           
  riinfo = ri_LoadConstraintInfo(constraintOid);

                                                               
  if (rel_is_pk)
  {
    if (riinfo->fk_relid != trigger->tgconstrrelid || riinfo->pk_relid != RelationGetRelid(trig_rel))
    {
      elog(ERROR, "wrong pg_constraint entry for trigger \"%s\" on table \"%s\"", trigger->tgname, RelationGetRelationName(trig_rel));
    }
  }
  else
  {
    if (riinfo->fk_relid != RelationGetRelid(trig_rel) || riinfo->pk_relid != trigger->tgconstrrelid)
    {
      elog(ERROR, "wrong pg_constraint entry for trigger \"%s\" on table \"%s\"", trigger->tgname, RelationGetRelationName(trig_rel));
    }
  }

  if (riinfo->confmatchtype != FKCONSTR_MATCH_FULL && riinfo->confmatchtype != FKCONSTR_MATCH_PARTIAL && riinfo->confmatchtype != FKCONSTR_MATCH_SIMPLE)
  {
    elog(ERROR, "unrecognized confmatchtype: %d", riinfo->confmatchtype);
  }

  if (riinfo->confmatchtype == FKCONSTR_MATCH_PARTIAL)
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("MATCH PARTIAL not yet implemented")));
  }

  return riinfo;
}

   
                                                                      
   
static const RI_ConstraintInfo *
ri_LoadConstraintInfo(Oid constraintOid)
{
  RI_ConstraintInfo *riinfo;
  bool found;
  HeapTuple tup;
  Form_pg_constraint conForm;

     
                                                
     
  if (!ri_constraint_cache)
  {
    ri_InitHashTables();
  }

     
                                                                           
     
  riinfo = (RI_ConstraintInfo *)hash_search(ri_constraint_cache, (void *)&constraintOid, HASH_ENTER, &found);
  if (!found)
  {
    riinfo->valid = false;
  }
  else if (riinfo->valid)
  {
    return riinfo;
  }

     
                                                              
     
  tup = SearchSysCache1(CONSTROID, ObjectIdGetDatum(constraintOid));
  if (!HeapTupleIsValid(tup))                        
  {
    elog(ERROR, "cache lookup failed for constraint %u", constraintOid);
  }
  conForm = (Form_pg_constraint)GETSTRUCT(tup);

  if (conForm->contype != CONSTRAINT_FOREIGN)                        
  {
    elog(ERROR, "constraint %u is not a foreign key constraint", constraintOid);
  }

                        
  Assert(riinfo->constraint_id == constraintOid);
  riinfo->oidHashValue = GetSysCacheHashValue1(CONSTROID, ObjectIdGetDatum(constraintOid));
  memcpy(&riinfo->conname, &conForm->conname, sizeof(NameData));
  riinfo->pk_relid = conForm->confrelid;
  riinfo->fk_relid = conForm->conrelid;
  riinfo->confupdtype = conForm->confupdtype;
  riinfo->confdeltype = conForm->confdeltype;
  riinfo->confmatchtype = conForm->confmatchtype;

  DeconstructFkConstraintRow(tup, &riinfo->nkeys, riinfo->fk_attnums, riinfo->pk_attnums, riinfo->pf_eq_oprs, riinfo->pp_eq_oprs, riinfo->ff_eq_oprs);

  ReleaseSysCache(tup);

     
                                                                        
                                                                      
     
  dlist_push_tail(&ri_constraint_cache_valid_list, &riinfo->valid_link);
  ri_constraint_cache_valid_count++;

  riinfo->valid = true;

  return riinfo;
}

   
                                           
   
                                                                             
                                                                      
                                                                         
                                                                          
   
                                                                            
                                                                            
                                                                           
                                                                               
                                                                         
   
static void
InvalidateConstraintCacheCallBack(Datum arg, int cacheid, uint32 hashvalue)
{
  dlist_mutable_iter iter;

  Assert(ri_constraint_cache != NULL);

     
                                                                            
                                                                         
                                                                             
                                                                      
     
  if (ri_constraint_cache_valid_count > 1000)
  {
    hashvalue = 0;                                 
  }

  dlist_foreach_modify(iter, &ri_constraint_cache_valid_list)
  {
    RI_ConstraintInfo *riinfo = dlist_container(RI_ConstraintInfo, valid_link, iter.cur);

    if (hashvalue == 0 || riinfo->oidHashValue == hashvalue)
    {
      riinfo->valid = false;
                                                         
      dlist_delete(iter.cur);
      ri_constraint_cache_valid_count--;
    }
  }
}

   
                                                                   
   
                                                                    
                                           
   
static SPIPlanPtr
ri_PlanCheck(const char *querystr, int nargs, Oid *argtypes, RI_QueryKey *qkey, Relation fk_rel, Relation pk_rel, bool cache_plan)
{
  SPIPlanPtr qplan;
  Relation query_rel;
  Oid save_userid;
  int save_sec_context;

     
                                                                           
                                                                  
     
  if (qkey->constr_queryno <= RI_PLAN_LAST_ON_PK)
  {
    query_rel = pk_rel;
  }
  else
  {
    query_rel = fk_rel;
  }

                                                
  GetUserIdAndSecContext(&save_userid, &save_sec_context);
  SetUserIdAndSecContext(RelationGetForm(query_rel)->relowner, save_sec_context | SECURITY_LOCAL_USERID_CHANGE | SECURITY_NOFORCE_RLS);

                       
  qplan = SPI_prepare(querystr, nargs, argtypes);

  if (qplan == NULL)
  {
    elog(ERROR, "SPI_prepare returned %s for %s", SPI_result_code_string(SPI_result), querystr);
  }

                                        
  SetUserIdAndSecContext(save_userid, save_sec_context);

                                  
  if (cache_plan)
  {
    SPI_keepplan(qplan);
    ri_HashPreparedPlan(qkey, qplan);
  }

  return qplan;
}

   
                                                
   
static bool
ri_PerformCheck(const RI_ConstraintInfo *riinfo, RI_QueryKey *qkey, SPIPlanPtr qplan, Relation fk_rel, Relation pk_rel, TupleTableSlot *oldslot, TupleTableSlot *newslot, bool detectNewRows, int expect_OK)
{
  Relation query_rel, source_rel;
  bool source_is_pk;
  Snapshot test_snapshot;
  Snapshot crosscheck_snapshot;
  int limit;
  int spi_result;
  Oid save_userid;
  int save_sec_context;
  Datum vals[RI_MAX_NUMKEYS * 2];
  char nulls[RI_MAX_NUMKEYS * 2];

     
                                                                           
                                                                  
     
  if (qkey->constr_queryno <= RI_PLAN_LAST_ON_PK)
  {
    query_rel = pk_rel;
  }
  else
  {
    query_rel = fk_rel;
  }

     
                                                                            
                                                                            
                                                                             
                                                                           
                                                 
     
  if (qkey->constr_queryno == RI_PLAN_CHECK_LOOKUPPK)
  {
    source_rel = fk_rel;
    source_is_pk = false;
  }
  else
  {
    source_rel = pk_rel;
    source_is_pk = true;
  }

                                                          
  if (newslot)
  {
    ri_ExtractValues(source_rel, newslot, riinfo, source_is_pk, vals, nulls);
    if (oldslot)
    {
      ri_ExtractValues(source_rel, oldslot, riinfo, source_is_pk, vals + riinfo->nkeys, nulls + riinfo->nkeys);
    }
  }
  else
  {
    ri_ExtractValues(source_rel, oldslot, riinfo, source_is_pk, vals, nulls);
  }

     
                                                                       
                                                                          
                                                                             
                                                                             
                                                                             
                                                                           
                                                                           
                                                                             
                     
     
  if (IsolationUsesXactSnapshot() && detectNewRows)
  {
    CommandCounterIncrement();                                         
    test_snapshot = GetLatestSnapshot();
    crosscheck_snapshot = GetTransactionSnapshot();
  }
  else
  {
                                          
    test_snapshot = InvalidSnapshot;
    crosscheck_snapshot = InvalidSnapshot;
  }

     
                                                                      
                                                                          
                                                                            
                                   
     
  limit = (expect_OK == SPI_OK_SELECT) ? 1 : 0;

                                                
  GetUserIdAndSecContext(&save_userid, &save_sec_context);
  SetUserIdAndSecContext(RelationGetForm(query_rel)->relowner, save_sec_context | SECURITY_LOCAL_USERID_CHANGE | SECURITY_NOFORCE_RLS);

                                     
  spi_result = SPI_execute_snapshot(qplan, vals, nulls, test_snapshot, crosscheck_snapshot, false, false, limit);

                                        
  SetUserIdAndSecContext(save_userid, save_sec_context);

                    
  if (spi_result < 0)
  {
    elog(ERROR, "SPI_execute_snapshot returned %s", SPI_result_code_string(spi_result));
  }

  if (expect_OK >= 0 && spi_result != expect_OK)
  {
    ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("referential integrity query on \"%s\" from constraint \"%s\" on \"%s\" gave unexpected result", RelationGetRelationName(pk_rel), NameStr(riinfo->conname), RelationGetRelationName(fk_rel)), errhint("This is most likely due to a rule having rewritten the query.")));
  }

                                                                 
  if (qkey->constr_queryno != RI_PLAN_CHECK_LOOKUPPK_FROM_PK && expect_OK == SPI_OK_SELECT && (SPI_processed == 0) == (qkey->constr_queryno == RI_PLAN_CHECK_LOOKUPPK))
  {
    ri_ReportViolation(riinfo, pk_rel, fk_rel, newslot ? newslot : oldslot, NULL, qkey->constr_queryno, false);
  }

  return SPI_processed != 0;
}

   
                                                       
   
static void
ri_ExtractValues(Relation rel, TupleTableSlot *slot, const RI_ConstraintInfo *riinfo, bool rel_is_pk, Datum *vals, char *nulls)
{
  const int16 *attnums;
  bool isnull;

  if (rel_is_pk)
  {
    attnums = riinfo->pk_attnums;
  }
  else
  {
    attnums = riinfo->fk_attnums;
  }

  for (int i = 0; i < riinfo->nkeys; i++)
  {
    vals[i] = slot_getattr(slot, attnums[i], &isnull);
    nulls[i] = isnull ? 'n' : ' ';
  }
}

   
                           
   
                                                                  
                                                                        
                                                         
                                                                       
                                                              
   
static void
ri_ReportViolation(const RI_ConstraintInfo *riinfo, Relation pk_rel, Relation fk_rel, TupleTableSlot *violatorslot, TupleDesc tupdesc, int queryno, bool partgone)
{
  StringInfoData key_names;
  StringInfoData key_values;
  bool onfk;
  const int16 *attnums;
  Oid rel_oid;
  AclResult aclresult;
  bool has_perm = true;

     
                                                                           
                                                           
     
  onfk = (queryno == RI_PLAN_CHECK_LOOKUPPK);
  if (onfk)
  {
    attnums = riinfo->fk_attnums;
    rel_oid = fk_rel->rd_id;
    if (tupdesc == NULL)
    {
      tupdesc = fk_rel->rd_att;
    }
  }
  else
  {
    attnums = riinfo->pk_attnums;
    rel_oid = pk_rel->rd_id;
    if (tupdesc == NULL)
    {
      tupdesc = pk_rel->rd_att;
    }
  }

     
                                                                             
                                                                         
     
                                                                            
                                          
     
                                                                        
                 
     
                                                                           
                                                                   
     
  if (partgone)
  {
    has_perm = true;
  }
  else if (check_enable_rls(rel_oid, InvalidOid, true) != RLS_ENABLED)
  {
    aclresult = pg_class_aclcheck(rel_oid, GetUserId(), ACL_SELECT);
    if (aclresult != ACLCHECK_OK)
    {
                                            
      for (int idx = 0; idx < riinfo->nkeys; idx++)
      {
        aclresult = pg_attribute_aclcheck(rel_oid, attnums[idx], GetUserId(), ACL_SELECT);

                                  
        if (aclresult != ACLCHECK_OK)
        {
          has_perm = false;
          break;
        }
      }
    }
  }
  else
  {
    has_perm = false;
  }

  if (has_perm)
  {
                                                     
    initStringInfo(&key_names);
    initStringInfo(&key_values);
    for (int idx = 0; idx < riinfo->nkeys; idx++)
    {
      int fnum = attnums[idx];
      Form_pg_attribute att = TupleDescAttr(tupdesc, fnum - 1);
      char *name, *val;
      Datum datum;
      bool isnull;

      name = NameStr(att->attname);

      datum = slot_getattr(violatorslot, fnum, &isnull);
      if (!isnull)
      {
        Oid foutoid;
        bool typisvarlena;

        getTypeOutputInfo(att->atttypid, &foutoid, &typisvarlena);
        val = OidOutputFunctionCall(foutoid, datum);
      }
      else
      {
        val = "null";
      }

      if (idx > 0)
      {
        appendStringInfoString(&key_names, ", ");
        appendStringInfoString(&key_values, ", ");
      }
      appendStringInfoString(&key_names, name);
      appendStringInfoString(&key_values, val);
    }
  }

  if (partgone)
  {
    ereport(ERROR, (errcode(ERRCODE_FOREIGN_KEY_VIOLATION), errmsg("removing partition \"%s\" violates foreign key constraint \"%s\"", RelationGetRelationName(pk_rel), NameStr(riinfo->conname)), errdetail("Key (%s)=(%s) is still referenced from table \"%s\".", key_names.data, key_values.data, RelationGetRelationName(fk_rel))));
  }
  else if (onfk)
  {
    ereport(ERROR, (errcode(ERRCODE_FOREIGN_KEY_VIOLATION), errmsg("insert or update on table \"%s\" violates foreign key constraint \"%s\"", RelationGetRelationName(fk_rel), NameStr(riinfo->conname)), has_perm ? errdetail("Key (%s)=(%s) is not present in table \"%s\".", key_names.data, key_values.data, RelationGetRelationName(pk_rel)) : errdetail("Key is not present in table \"%s\".", RelationGetRelationName(pk_rel)), errtableconstraint(fk_rel, NameStr(riinfo->conname))));
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_FOREIGN_KEY_VIOLATION), errmsg("update or delete on table \"%s\" violates foreign key constraint \"%s\" on table \"%s\"", RelationGetRelationName(pk_rel), NameStr(riinfo->conname), RelationGetRelationName(fk_rel)), has_perm ? errdetail("Key (%s)=(%s) is still referenced from table \"%s\".", key_names.data, key_values.data, RelationGetRelationName(fk_rel)) : errdetail("Key is still referenced from table \"%s\".", RelationGetRelationName(fk_rel)), errtableconstraint(fk_rel, NameStr(riinfo->conname))));
  }
}

   
                  
   
                                                         
   
                                                                            
   
static int
ri_NullCheck(TupleDesc tupDesc, TupleTableSlot *slot, const RI_ConstraintInfo *riinfo, bool rel_is_pk)
{
  const int16 *attnums;
  bool allnull = true;
  bool nonenull = true;

  if (rel_is_pk)
  {
    attnums = riinfo->pk_attnums;
  }
  else
  {
    attnums = riinfo->fk_attnums;
  }

  for (int i = 0; i < riinfo->nkeys; i++)
  {
    if (slot_attisnull(slot, attnums[i]))
    {
      nonenull = false;
    }
    else
    {
      allnull = false;
    }
  }

  if (allnull)
  {
    return RI_KEYS_ALL_NULL;
  }

  if (nonenull)
  {
    return RI_KEYS_NONE_NULL;
  }

  return RI_KEYS_SOME_NULL;
}

   
                       
   
                                        
   
static void
ri_InitHashTables(void)
{
  HASHCTL ctl;

  memset(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(Oid);
  ctl.entrysize = sizeof(RI_ConstraintInfo);
  ri_constraint_cache = hash_create("RI constraint cache", RI_INIT_CONSTRAINTHASHSIZE, &ctl, HASH_ELEM | HASH_BLOBS);

                                                       
  CacheRegisterSyscacheCallback(CONSTROID, InvalidateConstraintCacheCallBack, (Datum)0);

  memset(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(RI_QueryKey);
  ctl.entrysize = sizeof(RI_QueryHashEntry);
  ri_query_cache = hash_create("RI query cache", RI_INIT_QUERYHASHSIZE, &ctl, HASH_ELEM | HASH_BLOBS);

  memset(&ctl, 0, sizeof(ctl));
  ctl.keysize = sizeof(RI_CompareKey);
  ctl.entrysize = sizeof(RI_CompareHashEntry);
  ri_compare_cache = hash_create("RI compare cache", RI_INIT_QUERYHASHSIZE, &ctl, HASH_ELEM | HASH_BLOBS);
}

   
                          
   
                                                                
                                                                    
   
static SPIPlanPtr
ri_FetchPreparedPlan(RI_QueryKey *key)
{
  RI_QueryHashEntry *entry;
  SPIPlanPtr plan;

     
                                                
     
  if (!ri_query_cache)
  {
    ri_InitHashTables();
  }

     
                        
     
  entry = (RI_QueryHashEntry *)hash_search(ri_query_cache, (void *)key, HASH_FIND, NULL);
  if (entry == NULL)
  {
    return NULL;
  }

     
                                                                           
                                                                         
                                                                          
                                                                            
                                            
     
                                                                       
                                 
     
  plan = entry->plan;
  if (plan && SPI_plan_is_valid(plan))
  {
    return plan;
  }

     
                                                                            
                                            
     
  entry->plan = NULL;
  if (plan)
  {
    SPI_freeplan(plan);
  }

  return NULL;
}

   
                         
   
                                                             
   
static void
ri_HashPreparedPlan(RI_QueryKey *key, SPIPlanPtr plan)
{
  RI_QueryHashEntry *entry;
  bool found;

     
                                                
     
  if (!ri_query_cache)
  {
    ri_InitHashTables();
  }

     
                                                                          
                                      
     
  entry = (RI_QueryHashEntry *)hash_search(ri_query_cache, (void *)key, HASH_ENTER, &found);
  Assert(!found || entry->plan == NULL);
  entry->plan = plan;
}

   
                  
   
                                                     
   
                                                                      
                                                                     
                                                                        
                                                                  
   
static bool
ri_KeysEqual(Relation rel, TupleTableSlot *oldslot, TupleTableSlot *newslot, const RI_ConstraintInfo *riinfo, bool rel_is_pk)
{
  const int16 *attnums;

  if (rel_is_pk)
  {
    attnums = riinfo->pk_attnums;
  }
  else
  {
    attnums = riinfo->fk_attnums;
  }

                                                                     
  for (int i = 0; i < riinfo->nkeys; i++)
  {
    Datum oldvalue;
    Datum newvalue;
    bool isnull;

       
                                                                        
       
    oldvalue = slot_getattr(oldslot, attnums[i], &isnull);
    if (isnull)
    {
      return false;
    }

       
                                                                        
       
    newvalue = slot_getattr(newslot, attnums[i], &isnull);
    if (isnull)
    {
      return false;
    }

    if (rel_is_pk)
    {
         
                                                               
                                                                   
                                                                    
                                                               
                                                                        
                                         
         
      Form_pg_attribute att = TupleDescAttr(oldslot->tts_tupleDescriptor, attnums[i] - 1);

      if (!datum_image_eq(oldvalue, newvalue, att->attbyval, att->attlen))
      {
        return false;
      }
    }
    else
    {
         
                                                                 
                                                                      
                                      
         
      if (!ri_AttributesEqual(riinfo->ff_eq_oprs[i], RIAttType(rel, attnums[i]), oldvalue, newvalue))
      {
        return false;
      }
    }
  }

  return true;
}

   
                        
   
                                                                     
   
                                                           
   
static bool
ri_AttributesEqual(Oid eq_opr, Oid typeid, Datum oldvalue, Datum newvalue)
{
  RI_CompareHashEntry *entry = ri_HashCompareOp(eq_opr, typeid);

                                      
  if (OidIsValid(entry->cast_func_finfo.fn_oid))
  {
    oldvalue = FunctionCall3(&entry->cast_func_finfo, oldvalue, Int32GetDatum(-1),             
        BoolGetDatum(false));                                                                             
    newvalue = FunctionCall3(&entry->cast_func_finfo, newvalue, Int32GetDatum(-1),             
        BoolGetDatum(false));                                                                             
  }

     
                                    
     
                                                                            
                                                                            
                                                                    
                                                                             
                                                                          
                                                                           
                                                                  
                                                      
     
  return DatumGetBool(FunctionCall2Coll(&entry->eq_opr_finfo, DEFAULT_COLLATION_OID, oldvalue, newvalue));
}

   
                      
   
                                                                         
           
   
static RI_CompareHashEntry *
ri_HashCompareOp(Oid eq_opr, Oid typeid)
{
  RI_CompareKey key;
  RI_CompareHashEntry *entry;
  bool found;

     
                                                
     
  if (!ri_compare_cache)
  {
    ri_InitHashTables();
  }

     
                                                                     
                                 
     
  key.eq_opr = eq_opr;
  key.typeid = typeid;
  entry = (RI_CompareHashEntry *)hash_search(ri_compare_cache, (void *)&key, HASH_ENTER, &found);
  if (!found)
  {
    entry->valid = false;
  }

     
                                                                          
                                                                           
                                          
     
  if (!entry->valid)
  {
    Oid lefttype, righttype, castfunc;
    CoercionPathType pathtype;

                                                                  
    fmgr_info_cxt(get_opcode(eq_opr), &entry->eq_opr_finfo, TopMemoryContext);

       
                                                                          
                                                              
       
                                                                       
                                                                          
                                                                         
                                    
       
                                                                         
                                                                         
       
    op_input_types(eq_opr, &lefttype, &righttype);
    Assert(lefttype == righttype);
    if (typeid == lefttype)
    {
      castfunc = InvalidOid;                    
    }
    else
    {
      pathtype = find_coercion_pathway(lefttype, typeid, COERCION_IMPLICIT, &castfunc);
      if (pathtype != COERCION_PATH_FUNC && pathtype != COERCION_PATH_RELABELTYPE)
      {
           
                                                            
                                                                  
                                                               
                                                          
           
        if (!IsBinaryCoercible(typeid, lefttype))
        {
          elog(ERROR, "no conversion function from %s to %s", format_type_be(typeid), format_type_be(lefttype));
        }
      }
    }
    if (OidIsValid(castfunc))
    {
      fmgr_info_cxt(castfunc, &entry->cast_func_finfo, TopMemoryContext);
    }
    else
    {
      entry->cast_func_finfo.fn_oid = InvalidOid;
    }
    entry->valid = true;
  }

  return entry;
}

   
                                                                        
                                                          
   
int
RI_FKey_trigger_type(Oid tgfoid)
{
  switch (tgfoid)
  {
  case F_RI_FKEY_CASCADE_DEL:
  case F_RI_FKEY_CASCADE_UPD:
  case F_RI_FKEY_RESTRICT_DEL:
  case F_RI_FKEY_RESTRICT_UPD:
  case F_RI_FKEY_SETNULL_DEL:
  case F_RI_FKEY_SETNULL_UPD:
  case F_RI_FKEY_SETDEFAULT_DEL:
  case F_RI_FKEY_SETDEFAULT_UPD:
  case F_RI_FKEY_NOACTION_DEL:
  case F_RI_FKEY_NOACTION_UPD:
    return RI_TRIGGER_PK;

  case F_RI_FKEY_CHECK_INS:
  case F_RI_FKEY_CHECK_UPD:
    return RI_TRIGGER_FK;
  }

  return RI_TRIGGER_NONE;
}
