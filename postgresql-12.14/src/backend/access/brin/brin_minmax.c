   
                 
                                               
   
                                                                         
                                                                        
   
                  
                                           
   
#include "postgres.h"

#include "access/genam.h"
#include "access/brin_internal.h"
#include "access/brin_tuple.h"
#include "access/stratnum.h"
#include "catalog/pg_type.h"
#include "catalog/pg_amop.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

typedef struct MinmaxOpaque
{
  Oid cached_subtype;
  FmgrInfo strategy_procinfos[BTMaxStrategyNumber];
} MinmaxOpaque;

static FmgrInfo *
minmax_get_strategy_procinfo(BrinDesc *bdesc, uint16 attno, Oid subtype, uint16 strategynum);

Datum
brin_minmax_opcinfo(PG_FUNCTION_ARGS)
{
  Oid typoid = PG_GETARG_OID(0);
  BrinOpcInfo *result;

     
                                                                         
                                                                   
     

  result = palloc0(MAXALIGN(SizeofBrinOpcInfo(2)) + sizeof(MinmaxOpaque));
  result->oi_nstored = 2;
  result->oi_opaque = (MinmaxOpaque *)MAXALIGN((char *)result + SizeofBrinOpcInfo(2));
  result->oi_typcache[0] = result->oi_typcache[1] = lookup_type_cache(typoid, 0);

  PG_RETURN_POINTER(result);
}

   
                                                                             
                                                                               
                                                                          
                                                                              
                                                
   
Datum
brin_minmax_add_value(PG_FUNCTION_ARGS)
{
  BrinDesc *bdesc = (BrinDesc *)PG_GETARG_POINTER(0);
  BrinValues *column = (BrinValues *)PG_GETARG_POINTER(1);
  Datum newval = PG_GETARG_DATUM(2);
  bool isnull = PG_GETARG_DATUM(3);
  Oid colloid = PG_GET_COLLATION();
  FmgrInfo *cmpFn;
  Datum compar;
  bool updated = false;
  Form_pg_attribute attr;
  AttrNumber attno;

     
                                                                          
                                            
     
  if (isnull)
  {
    if (column->bv_hasnulls)
    {
      PG_RETURN_BOOL(false);
    }

    column->bv_hasnulls = true;
    PG_RETURN_BOOL(true);
  }

  attno = column->bv_attno;
  attr = TupleDescAttr(bdesc->bd_tupdesc, attno - 1);

     
                                                                             
                                                            
     
  if (column->bv_allnulls)
  {
    column->bv_values[0] = datumCopy(newval, attr->attbyval, attr->attlen);
    column->bv_values[1] = datumCopy(newval, attr->attbyval, attr->attlen);
    column->bv_allnulls = false;
    PG_RETURN_BOOL(true);
  }

     
                                                                           
                                                                     
                       
     
  cmpFn = minmax_get_strategy_procinfo(bdesc, attno, attr->atttypid, BTLessStrategyNumber);
  compar = FunctionCall2Coll(cmpFn, colloid, newval, column->bv_values[0]);
  if (DatumGetBool(compar))
  {
    if (!attr->attbyval)
    {
      pfree(DatumGetPointer(column->bv_values[0]));
    }
    column->bv_values[0] = datumCopy(newval, attr->attbyval, attr->attlen);
    updated = true;
  }

     
                                                 
     
  cmpFn = minmax_get_strategy_procinfo(bdesc, attno, attr->atttypid, BTGreaterStrategyNumber);
  compar = FunctionCall2Coll(cmpFn, colloid, newval, column->bv_values[1]);
  if (DatumGetBool(compar))
  {
    if (!attr->attbyval)
    {
      pfree(DatumGetPointer(column->bv_values[1]));
    }
    column->bv_values[1] = datumCopy(newval, attr->attbyval, attr->attlen);
    updated = true;
  }

  PG_RETURN_BOOL(updated);
}

   
                                                                              
                                                                            
                                                
   
Datum
brin_minmax_consistent(PG_FUNCTION_ARGS)
{
  BrinDesc *bdesc = (BrinDesc *)PG_GETARG_POINTER(0);
  BrinValues *column = (BrinValues *)PG_GETARG_POINTER(1);
  ScanKey key = (ScanKey)PG_GETARG_POINTER(2);
  Oid colloid = PG_GET_COLLATION(), subtype;
  AttrNumber attno;
  Datum value;
  Datum matches;
  FmgrInfo *finfo;

  Assert(key->sk_attno == column->bv_attno);

                                        
  if (key->sk_flags & SK_ISNULL)
  {
    if (key->sk_flags & SK_SEARCHNULL)
    {
      if (column->bv_allnulls || column->bv_hasnulls)
      {
        PG_RETURN_BOOL(true);
      }
      PG_RETURN_BOOL(false);
    }

       
                                                                       
                   
       
    if (key->sk_flags & SK_SEARCHNOTNULL)
    {
      PG_RETURN_BOOL(!column->bv_allnulls);
    }

       
                                                                      
                                              
       
    PG_RETURN_BOOL(false);
  }

                                                                   
  if (column->bv_allnulls)
  {
    PG_RETURN_BOOL(false);
  }

  attno = key->sk_attno;
  subtype = key->sk_subtype;
  value = key->sk_argument;
  switch (key->sk_strategy)
  {
  case BTLessStrategyNumber:
  case BTLessEqualStrategyNumber:
    finfo = minmax_get_strategy_procinfo(bdesc, attno, subtype, key->sk_strategy);
    matches = FunctionCall2Coll(finfo, colloid, column->bv_values[0], value);
    break;
  case BTEqualStrategyNumber:

       
                                                                     
                                                                   
                                                    
       
    finfo = minmax_get_strategy_procinfo(bdesc, attno, subtype, BTLessEqualStrategyNumber);
    matches = FunctionCall2Coll(finfo, colloid, column->bv_values[0], value);
    if (!DatumGetBool(matches))
    {
      break;
    }
                          
    finfo = minmax_get_strategy_procinfo(bdesc, attno, subtype, BTGreaterEqualStrategyNumber);
    matches = FunctionCall2Coll(finfo, colloid, column->bv_values[1], value);
    break;
  case BTGreaterEqualStrategyNumber:
  case BTGreaterStrategyNumber:
    finfo = minmax_get_strategy_procinfo(bdesc, attno, subtype, key->sk_strategy);
    matches = FunctionCall2Coll(finfo, colloid, column->bv_values[1], value);
    break;
  default:
                          
    elog(ERROR, "invalid strategy number %d", key->sk_strategy);
    matches = 0;
    break;
  }

  PG_RETURN_DATUM(matches);
}

   
                                                                            
                                                           
   
Datum
brin_minmax_union(PG_FUNCTION_ARGS)
{
  BrinDesc *bdesc = (BrinDesc *)PG_GETARG_POINTER(0);
  BrinValues *col_a = (BrinValues *)PG_GETARG_POINTER(1);
  BrinValues *col_b = (BrinValues *)PG_GETARG_POINTER(2);
  Oid colloid = PG_GET_COLLATION();
  AttrNumber attno;
  Form_pg_attribute attr;
  FmgrInfo *finfo;
  bool needsadj;

  Assert(col_a->bv_attno == col_b->bv_attno);

                         
  if (!col_a->bv_hasnulls && col_b->bv_hasnulls)
  {
    col_a->bv_hasnulls = true;
  }

                                                               
  if (col_b->bv_allnulls)
  {
    PG_RETURN_VOID();
  }

  attno = col_a->bv_attno;
  attr = TupleDescAttr(bdesc->bd_tupdesc, attno - 1);

     
                                                                             
                                                                          
                                                                             
                             
     
  if (col_a->bv_allnulls)
  {
    col_a->bv_allnulls = false;
    col_a->bv_values[0] = datumCopy(col_b->bv_values[0], attr->attbyval, attr->attlen);
    col_a->bv_values[1] = datumCopy(col_b->bv_values[1], attr->attbyval, attr->attlen);
    PG_RETURN_VOID();
  }

                                                       
  finfo = minmax_get_strategy_procinfo(bdesc, attno, attr->atttypid, BTLessStrategyNumber);
  needsadj = FunctionCall2Coll(finfo, colloid, col_b->bv_values[0], col_a->bv_values[0]);
  if (needsadj)
  {
    if (!attr->attbyval)
    {
      pfree(DatumGetPointer(col_a->bv_values[0]));
    }
    col_a->bv_values[0] = datumCopy(col_b->bv_values[0], attr->attbyval, attr->attlen);
  }

                                                          
  finfo = minmax_get_strategy_procinfo(bdesc, attno, attr->atttypid, BTGreaterStrategyNumber);
  needsadj = FunctionCall2Coll(finfo, colloid, col_b->bv_values[1], col_a->bv_values[1]);
  if (needsadj)
  {
    if (!attr->attbyval)
    {
      pfree(DatumGetPointer(col_a->bv_values[1]));
    }
    col_a->bv_values[1] = datumCopy(col_b->bv_values[1], attr->attbyval, attr->attlen);
  }

  PG_RETURN_VOID();
}

   
                                                          
   
                                                                          
                                                            
   
static FmgrInfo *
minmax_get_strategy_procinfo(BrinDesc *bdesc, uint16 attno, Oid subtype, uint16 strategynum)
{
  MinmaxOpaque *opaque;

  Assert(strategynum >= 1 && strategynum <= BTMaxStrategyNumber);

  opaque = (MinmaxOpaque *)bdesc->bd_info[attno - 1]->oi_opaque;

     
                                                                            
                                                                    
                                        
     
  if (opaque->cached_subtype != subtype)
  {
    uint16 i;

    for (i = 1; i <= BTMaxStrategyNumber; i++)
    {
      opaque->strategy_procinfos[i - 1].fn_oid = InvalidOid;
    }
    opaque->cached_subtype = subtype;
  }

  if (opaque->strategy_procinfos[strategynum - 1].fn_oid == InvalidOid)
  {
    Form_pg_attribute attr;
    HeapTuple tuple;
    Oid opfamily, oprid;
    bool isNull;

    opfamily = bdesc->bd_index->rd_opfamily[attno - 1];
    attr = TupleDescAttr(bdesc->bd_tupdesc, attno - 1);
    tuple = SearchSysCache4(AMOPSTRATEGY, ObjectIdGetDatum(opfamily), ObjectIdGetDatum(attr->atttypid), ObjectIdGetDatum(subtype), Int16GetDatum(strategynum));

    if (!HeapTupleIsValid(tuple))
    {
      elog(ERROR, "missing operator %d(%u,%u) in opfamily %u", strategynum, attr->atttypid, subtype, opfamily);
    }

    oprid = DatumGetObjectId(SysCacheGetAttr(AMOPSTRATEGY, tuple, Anum_pg_amop_amopopr, &isNull));
    ReleaseSysCache(tuple);
    Assert(!isNull && RegProcedureIsValid(oprid));

    fmgr_info_cxt(get_opcode(oprid), &opaque->strategy_procinfos[strategynum - 1], bdesc->bd_context);
  }

  return &opaque->strategy_procinfos[strategynum - 1];
}
