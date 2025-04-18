                                                                            
   
         
                                              
   
                                                                         
                                                                        
   
   
                  
                                 
   
         
                                                
   
                                                                            
   
#include "postgres.h"

#include <math.h>
#include <limits.h>

#include "access/heapam.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "parser/parsetree.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/varlena.h"

#define DatumGetItemPointer(X) ((ItemPointer)DatumGetPointer(X))
#define ItemPointerGetDatum(X) PointerGetDatum(X)
#define PG_GETARG_ITEMPOINTER(n) DatumGetItemPointer(PG_GETARG_DATUM(n))
#define PG_RETURN_ITEMPOINTER(x) return ItemPointerGetDatum(x)

#define LDELIM '('
#define RDELIM ')'
#define DELIM ','
#define NTIDARGS 2

                                                                    
          
                                                                    
   
Datum
tidin(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);
  char *p, *coord[NTIDARGS];
  int i;
  ItemPointer result;
  BlockNumber blockNumber;
  OffsetNumber offsetNumber;
  char *badp;
  int hold_offset;

  for (i = 0, p = str; *p && i < NTIDARGS && *p != RDELIM; p++)
  {
    if (*p == DELIM || (*p == LDELIM && !i))
    {
      coord[i++] = p + 1;
    }
  }

  if (i < NTIDARGS)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "tid", str)));
  }

  errno = 0;
  blockNumber = strtoul(coord[0], &badp, 10);
  if (errno || *badp != DELIM)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "tid", str)));
  }

  hold_offset = strtol(coord[1], &badp, 10);
  if (errno || *badp != RDELIM || hold_offset > USHRT_MAX || hold_offset < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "tid", str)));
  }

  offsetNumber = hold_offset;

  result = (ItemPointer)palloc(sizeof(ItemPointerData));

  ItemPointerSet(result, blockNumber, offsetNumber);

  PG_RETURN_ITEMPOINTER(result);
}

                                                                    
           
                                                                    
   
Datum
tidout(PG_FUNCTION_ARGS)
{
  ItemPointer itemPtr = PG_GETARG_ITEMPOINTER(0);
  BlockNumber blockNumber;
  OffsetNumber offsetNumber;
  char buf[32];

  blockNumber = ItemPointerGetBlockNumberNoCheck(itemPtr);
  offsetNumber = ItemPointerGetOffsetNumberNoCheck(itemPtr);

                                                          
  snprintf(buf, sizeof(buf), "(%u,%u)", blockNumber, offsetNumber);

  PG_RETURN_CSTRING(pstrdup(buf));
}

   
                                                       
   
Datum
tidrecv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  ItemPointer result;
  BlockNumber blockNumber;
  OffsetNumber offsetNumber;

  blockNumber = pq_getmsgint(buf, sizeof(blockNumber));
  offsetNumber = pq_getmsgint(buf, sizeof(offsetNumber));

  result = (ItemPointer)palloc(sizeof(ItemPointerData));

  ItemPointerSet(result, blockNumber, offsetNumber);

  PG_RETURN_ITEMPOINTER(result);
}

   
                                              
   
Datum
tidsend(PG_FUNCTION_ARGS)
{
  ItemPointer itemPtr = PG_GETARG_ITEMPOINTER(0);
  StringInfoData buf;

  pq_begintypsend(&buf);
  pq_sendint32(&buf, ItemPointerGetBlockNumberNoCheck(itemPtr));
  pq_sendint16(&buf, ItemPointerGetOffsetNumberNoCheck(itemPtr));
  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

                                                                               
                                    
                                                                               

Datum
tideq(PG_FUNCTION_ARGS)
{
  ItemPointer arg1 = PG_GETARG_ITEMPOINTER(0);
  ItemPointer arg2 = PG_GETARG_ITEMPOINTER(1);

  PG_RETURN_BOOL(ItemPointerCompare(arg1, arg2) == 0);
}

Datum
tidne(PG_FUNCTION_ARGS)
{
  ItemPointer arg1 = PG_GETARG_ITEMPOINTER(0);
  ItemPointer arg2 = PG_GETARG_ITEMPOINTER(1);

  PG_RETURN_BOOL(ItemPointerCompare(arg1, arg2) != 0);
}

Datum
tidlt(PG_FUNCTION_ARGS)
{
  ItemPointer arg1 = PG_GETARG_ITEMPOINTER(0);
  ItemPointer arg2 = PG_GETARG_ITEMPOINTER(1);

  PG_RETURN_BOOL(ItemPointerCompare(arg1, arg2) < 0);
}

Datum
tidle(PG_FUNCTION_ARGS)
{
  ItemPointer arg1 = PG_GETARG_ITEMPOINTER(0);
  ItemPointer arg2 = PG_GETARG_ITEMPOINTER(1);

  PG_RETURN_BOOL(ItemPointerCompare(arg1, arg2) <= 0);
}

Datum
tidgt(PG_FUNCTION_ARGS)
{
  ItemPointer arg1 = PG_GETARG_ITEMPOINTER(0);
  ItemPointer arg2 = PG_GETARG_ITEMPOINTER(1);

  PG_RETURN_BOOL(ItemPointerCompare(arg1, arg2) > 0);
}

Datum
tidge(PG_FUNCTION_ARGS)
{
  ItemPointer arg1 = PG_GETARG_ITEMPOINTER(0);
  ItemPointer arg2 = PG_GETARG_ITEMPOINTER(1);

  PG_RETURN_BOOL(ItemPointerCompare(arg1, arg2) >= 0);
}

Datum
bttidcmp(PG_FUNCTION_ARGS)
{
  ItemPointer arg1 = PG_GETARG_ITEMPOINTER(0);
  ItemPointer arg2 = PG_GETARG_ITEMPOINTER(1);

  PG_RETURN_INT32(ItemPointerCompare(arg1, arg2));
}

Datum
tidlarger(PG_FUNCTION_ARGS)
{
  ItemPointer arg1 = PG_GETARG_ITEMPOINTER(0);
  ItemPointer arg2 = PG_GETARG_ITEMPOINTER(1);

  PG_RETURN_ITEMPOINTER(ItemPointerCompare(arg1, arg2) >= 0 ? arg1 : arg2);
}

Datum
tidsmaller(PG_FUNCTION_ARGS)
{
  ItemPointer arg1 = PG_GETARG_ITEMPOINTER(0);
  ItemPointer arg2 = PG_GETARG_ITEMPOINTER(1);

  PG_RETURN_ITEMPOINTER(ItemPointerCompare(arg1, arg2) <= 0 ? arg1 : arg2);
}

Datum
hashtid(PG_FUNCTION_ARGS)
{
  ItemPointer key = PG_GETARG_ITEMPOINTER(0);

     
                                                                      
                                                                         
                                                                      
                                                                
     
  return hash_any((unsigned char *)key, sizeof(BlockIdData) + sizeof(OffsetNumber));
}

Datum
hashtidextended(PG_FUNCTION_ARGS)
{
  ItemPointer key = PG_GETARG_ITEMPOINTER(0);
  uint64 seed = PG_GETARG_INT64(1);

                
  return hash_any_extended((unsigned char *)key, sizeof(BlockIdData) + sizeof(OffsetNumber), seed);
}

   
                                                     
   
                                                                
   

static ItemPointerData Current_last_tid = {{0, 0}, 0};

void
setLastTid(const ItemPointer tid)
{
  Current_last_tid = *tid;
}

   
                          
                                                   
                                               
   
static Datum
currtid_for_view(Relation viewrel, ItemPointer tid)
{
  TupleDesc att = RelationGetDescr(viewrel);
  RuleLock *rulelock;
  RewriteRule *rewrite;
  int i, natts = att->natts, tididx = -1;

  for (i = 0; i < natts; i++)
  {
    Form_pg_attribute attr = TupleDescAttr(att, i);

    if (strcmp(NameStr(attr->attname), "ctid") == 0)
    {
      if (attr->atttypid != TIDOID)
      {
        elog(ERROR, "ctid isn't of type TID");
      }
      tididx = i;
      break;
    }
  }
  if (tididx < 0)
  {
    elog(ERROR, "currtid cannot handle views with no CTID");
  }
  rulelock = viewrel->rd_rules;
  if (!rulelock)
  {
    elog(ERROR, "the view has no rules");
  }
  for (i = 0; i < rulelock->numLocks; i++)
  {
    rewrite = rulelock->rules[i];
    if (rewrite->event == CMD_SELECT)
    {
      Query *query;
      TargetEntry *tle;

      if (list_length(rewrite->actions) != 1)
      {
        elog(ERROR, "only one select rule is allowed in views");
      }
      query = (Query *)linitial(rewrite->actions);
      tle = get_tle_by_resno(query->targetList, tididx + 1);
      if (tle && tle->expr && IsA(tle->expr, Var))
      {
        Var *var = (Var *)tle->expr;
        RangeTblEntry *rte;

        if (!IS_SPECIAL_VARNO(var->varno) && var->varattno == SelfItemPointerAttributeNumber)
        {
          rte = rt_fetch(var->varno, query->rtable);
          if (rte)
          {
            Datum result;

            result = DirectFunctionCall2(currtid_byreloid, ObjectIdGetDatum(rte->relid), PointerGetDatum(tid));
            table_close(viewrel, AccessShareLock);
            return result;
          }
        }
      }
      break;
    }
  }
  elog(ERROR, "currtid cannot handle this view");
  return (Datum)0;
}

Datum
currtid_byreloid(PG_FUNCTION_ARGS)
{
  Oid reloid = PG_GETARG_OID(0);
  ItemPointer tid = PG_GETARG_ITEMPOINTER(1);
  ItemPointer result;
  Relation rel;
  AclResult aclresult;
  Snapshot snapshot;
  TableScanDesc scan;

  result = (ItemPointer)palloc(sizeof(ItemPointerData));
  if (!reloid)
  {
    *result = Current_last_tid;
    PG_RETURN_ITEMPOINTER(result);
  }

  rel = table_open(reloid, AccessShareLock);

  aclresult = pg_class_aclcheck(RelationGetRelid(rel), GetUserId(), ACL_SELECT);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, get_relkind_objtype(rel->rd_rel->relkind), RelationGetRelationName(rel));
  }

  if (rel->rd_rel->relkind == RELKIND_VIEW)
  {
    return currtid_for_view(rel, tid);
  }

  if (!RELKIND_HAS_STORAGE(rel->rd_rel->relkind))
  {
    elog(ERROR, "cannot look at latest visible tid for relation \"%s.%s\"", get_namespace_name(RelationGetNamespace(rel)), RelationGetRelationName(rel));
  }

  ItemPointerCopy(tid, result);

  snapshot = RegisterSnapshot(GetLatestSnapshot());
  scan = table_beginscan_tid(rel, snapshot);
  table_tuple_get_latest_tid(scan, result);
  table_endscan(scan);
  UnregisterSnapshot(snapshot);

  table_close(rel, AccessShareLock);

  PG_RETURN_ITEMPOINTER(result);
}

Datum
currtid_byrelname(PG_FUNCTION_ARGS)
{
  text *relname = PG_GETARG_TEXT_PP(0);
  ItemPointer tid = PG_GETARG_ITEMPOINTER(1);
  ItemPointer result;
  RangeVar *relrv;
  Relation rel;
  AclResult aclresult;
  Snapshot snapshot;
  TableScanDesc scan;

  relrv = makeRangeVarFromNameList(textToQualifiedNameList(relname));
  rel = table_openrv(relrv, AccessShareLock);

  aclresult = pg_class_aclcheck(RelationGetRelid(rel), GetUserId(), ACL_SELECT);
  if (aclresult != ACLCHECK_OK)
  {
    aclcheck_error(aclresult, get_relkind_objtype(rel->rd_rel->relkind), RelationGetRelationName(rel));
  }

  if (rel->rd_rel->relkind == RELKIND_VIEW)
  {
    return currtid_for_view(rel, tid);
  }

  if (!RELKIND_HAS_STORAGE(rel->rd_rel->relkind))
  {
    elog(ERROR, "cannot look at latest visible tid for relation \"%s.%s\"", get_namespace_name(RelationGetNamespace(rel)), RelationGetRelationName(rel));
  }

  result = (ItemPointer)palloc(sizeof(ItemPointerData));
  ItemPointerCopy(tid, result);

  snapshot = RegisterSnapshot(GetLatestSnapshot());
  scan = table_beginscan_tid(rel, snapshot);
  table_tuple_get_latest_tid(scan, result);
  table_endscan(scan);
  UnregisterSnapshot(snapshot);

  table_close(rel, AccessShareLock);

  PG_RETURN_ITEMPOINTER(result);
}
