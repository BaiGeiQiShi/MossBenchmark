   
                    
                                                   
   
                                                                             
                                                                              
                 
   
                                                                         
                                                                              
                                                                             
                                                                            
                                                                           
                                                                               
                                                                          
                                                                          
           
   
                                                                         
                                                                        
   
                  
                                              
   
#include "postgres.h"

#include "access/brin_internal.h"
#include "access/brin_tuple.h"
#include "access/genam.h"
#include "access/skey.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

   
                                          
   
                                                                       
                    
   
#define INCLUSION_MAX_PROCNUMS 4                                    
#define PROCNUM_MERGE 11                       
#define PROCNUM_MERGEABLE 12                   
#define PROCNUM_CONTAINS 13                    
#define PROCNUM_EMPTY 14                       

   
                                                                        
                                                   
   
#define PROCNUM_BASE 11

    
                                                            
   
                   
                                               
                         
                                                           
                                                 
                            
                                                   
                       
   
#define INCLUSION_UNION 0
#define INCLUSION_UNMERGEABLE 1
#define INCLUSION_CONTAINS_EMPTY 2

typedef struct InclusionOpaque
{
  FmgrInfo extra_procinfos[INCLUSION_MAX_PROCNUMS];
  bool extra_proc_missing[INCLUSION_MAX_PROCNUMS];
  Oid cached_subtype;
  FmgrInfo strategy_procinfos[RTMaxStrategyNumber];
} InclusionOpaque;

static FmgrInfo *
inclusion_get_procinfo(BrinDesc *bdesc, uint16 attno, uint16 procnum);
static FmgrInfo *
inclusion_get_strategy_procinfo(BrinDesc *bdesc, uint16 attno, Oid subtype, uint16 strategynum);

   
                                   
   
Datum
brin_inclusion_opcinfo(PG_FUNCTION_ARGS)
{
  Oid typoid = PG_GETARG_OID(0);
  BrinOpcInfo *result;
  TypeCacheEntry *bool_typcache = lookup_type_cache(BOOLOID, 0);

     
                                                                        
                                                                      
                                                                           
                                                                   
                                                                         
                                                                            
                          
     
  result = palloc0(MAXALIGN(SizeofBrinOpcInfo(3)) + sizeof(InclusionOpaque));
  result->oi_nstored = 3;
  result->oi_opaque = (InclusionOpaque *)MAXALIGN((char *)result + SizeofBrinOpcInfo(3));

                 
  result->oi_typcache[INCLUSION_UNION] = lookup_type_cache(typoid, 0);

                                                
  result->oi_typcache[INCLUSION_UNMERGEABLE] = bool_typcache;

                                  
  result->oi_typcache[INCLUSION_CONTAINS_EMPTY] = bool_typcache;

  PG_RETURN_POINTER(result);
}

   
                                     
   
                                                                             
                                                                               
                                                                           
                                                                            
                                         
   
Datum
brin_inclusion_add_value(PG_FUNCTION_ARGS)
{
  BrinDesc *bdesc = (BrinDesc *)PG_GETARG_POINTER(0);
  BrinValues *column = (BrinValues *)PG_GETARG_POINTER(1);
  Datum newval = PG_GETARG_DATUM(2);
  bool isnull = PG_GETARG_BOOL(3);
  Oid colloid = PG_GET_COLLATION();
  FmgrInfo *finfo;
  Datum result;
  bool new = false;
  AttrNumber attno;
  Form_pg_attribute attr;

     
                                                                          
                                            
     
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
    column->bv_values[INCLUSION_UNION] = datumCopy(newval, attr->attbyval, attr->attlen);
    column->bv_values[INCLUSION_UNMERGEABLE] = BoolGetDatum(false);
    column->bv_values[INCLUSION_CONTAINS_EMPTY] = BoolGetDatum(false);
    column->bv_allnulls = false;
    new = true;
  }

     
                                                                    
                                    
     
  if (DatumGetBool(column->bv_values[INCLUSION_UNMERGEABLE]))
  {
    PG_RETURN_BOOL(false);
  }

     
                                                                          
                                                                     
                                                                
     
  finfo = inclusion_get_procinfo(bdesc, attno, PROCNUM_EMPTY);
  if (finfo != NULL && DatumGetBool(FunctionCall1Coll(finfo, colloid, newval)))
  {
    if (!DatumGetBool(column->bv_values[INCLUSION_CONTAINS_EMPTY]))
    {
      column->bv_values[INCLUSION_CONTAINS_EMPTY] = BoolGetDatum(true);
      PG_RETURN_BOOL(true);
    }

    PG_RETURN_BOOL(false);
  }

  if (new)
  {
    PG_RETURN_BOOL(true);
  }

                                                    
  finfo = inclusion_get_procinfo(bdesc, attno, PROCNUM_CONTAINS);
  if (finfo != NULL && DatumGetBool(FunctionCall2Coll(finfo, colloid, column->bv_values[INCLUSION_UNION], newval)))
  {
    PG_RETURN_BOOL(false);
  }

     
                                                                          
                                                                         
     
                                                                         
                                                                        
                                                                      
     
  finfo = inclusion_get_procinfo(bdesc, attno, PROCNUM_MERGEABLE);
  if (finfo != NULL && !DatumGetBool(FunctionCall2Coll(finfo, colloid, column->bv_values[INCLUSION_UNION], newval)))
  {
    column->bv_values[INCLUSION_UNMERGEABLE] = BoolGetDatum(true);
    PG_RETURN_BOOL(true);
  }

                                                           
  finfo = inclusion_get_procinfo(bdesc, attno, PROCNUM_MERGE);
  Assert(finfo != NULL);
  result = FunctionCall2Coll(finfo, colloid, column->bv_values[INCLUSION_UNION], newval);
  if (!attr->attbyval && DatumGetPointer(result) != DatumGetPointer(column->bv_values[INCLUSION_UNION]))
  {
    pfree(DatumGetPointer(column->bv_values[INCLUSION_UNION]));

    if (result == newval)
    {
      result = datumCopy(result, attr->attbyval, attr->attlen);
    }
  }
  column->bv_values[INCLUSION_UNION] = result;

  PG_RETURN_BOOL(true);
}

   
                                      
   
                                       
   
Datum
brin_inclusion_consistent(PG_FUNCTION_ARGS)
{
  BrinDesc *bdesc = (BrinDesc *)PG_GETARG_POINTER(0);
  BrinValues *column = (BrinValues *)PG_GETARG_POINTER(1);
  ScanKey key = (ScanKey)PG_GETARG_POINTER(2);
  Oid colloid = PG_GET_COLLATION(), subtype;
  Datum unionval;
  AttrNumber attno;
  Datum query;
  FmgrInfo *finfo;
  Datum result;

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

                                                                             
  if (DatumGetBool(column->bv_values[INCLUSION_UNMERGEABLE]))
  {
    PG_RETURN_BOOL(true);
  }

  attno = key->sk_attno;
  subtype = key->sk_subtype;
  query = key->sk_argument;
  unionval = column->bv_values[INCLUSION_UNION];
  switch (key->sk_strategy)
  {
       
                            
       
                                                                     
                                                                   
                                                                      
                                                                     
                                   
       
                                                                       
                                            
       

  case RTLeftStrategyNumber:
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTOverRightStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    PG_RETURN_BOOL(!DatumGetBool(result));

  case RTOverLeftStrategyNumber:
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTRightStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    PG_RETURN_BOOL(!DatumGetBool(result));

  case RTOverRightStrategyNumber:
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTLeftStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    PG_RETURN_BOOL(!DatumGetBool(result));

  case RTRightStrategyNumber:
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTOverLeftStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    PG_RETURN_BOOL(!DatumGetBool(result));

  case RTBelowStrategyNumber:
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTOverAboveStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    PG_RETURN_BOOL(!DatumGetBool(result));

  case RTOverBelowStrategyNumber:
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTAboveStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    PG_RETURN_BOOL(!DatumGetBool(result));

  case RTOverAboveStrategyNumber:
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTBelowStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    PG_RETURN_BOOL(!DatumGetBool(result));

  case RTAboveStrategyNumber:
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTOverBelowStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    PG_RETURN_BOOL(!DatumGetBool(result));

       
                                       
       
                                                                      
                                                                    
                   
       

  case RTOverlapStrategyNumber:
  case RTContainsStrategyNumber:
  case RTOldContainsStrategyNumber:
  case RTContainsElemStrategyNumber:
  case RTSubStrategyNumber:
  case RTSubEqualStrategyNumber:
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, key->sk_strategy);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    PG_RETURN_DATUM(result);

       
                               
       
                                                                      
                                                                     
                                                              
       
                                                                     
                                                 
       

  case RTContainedByStrategyNumber:
  case RTOldContainedByStrategyNumber:
  case RTSuperStrategyNumber:
  case RTSuperEqualStrategyNumber:
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTOverlapStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    if (DatumGetBool(result))
    {
      PG_RETURN_BOOL(true);
    }

    PG_RETURN_DATUM(column->bv_values[INCLUSION_CONTAINS_EMPTY]);

       
                         
       
                                                                    
                                      
       
                                                                     
                                
       

  case RTAdjacentStrategyNumber:
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTOverlapStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    if (DatumGetBool(result))
    {
      PG_RETURN_BOOL(true);
    }

    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTAdjacentStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    PG_RETURN_DATUM(result);

       
                                   
       
                                                                     
                                                                       
                                                                 
                                                                 
                                                               
                           
       
                                                                    
                                                                       
                 
       
                                                                     
                                                                      
                                                                      
                                                                     
                                                                     
               
       

  case RTLessStrategyNumber:
  case RTLessEqualStrategyNumber:
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTRightStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    if (!DatumGetBool(result))
    {
      PG_RETURN_BOOL(true);
    }

    PG_RETURN_DATUM(column->bv_values[INCLUSION_CONTAINS_EMPTY]);

  case RTSameStrategyNumber:
  case RTEqualStrategyNumber:
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTContainsStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    if (DatumGetBool(result))
    {
      PG_RETURN_BOOL(true);
    }

    PG_RETURN_DATUM(column->bv_values[INCLUSION_CONTAINS_EMPTY]);

  case RTGreaterEqualStrategyNumber:
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTLeftStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    if (!DatumGetBool(result))
    {
      PG_RETURN_BOOL(true);
    }

    PG_RETURN_DATUM(column->bv_values[INCLUSION_CONTAINS_EMPTY]);

  case RTGreaterStrategyNumber:
                                             
    finfo = inclusion_get_strategy_procinfo(bdesc, attno, subtype, RTLeftStrategyNumber);
    result = FunctionCall2Coll(finfo, colloid, unionval, query);
    PG_RETURN_BOOL(!DatumGetBool(result));

  default:
                          
    elog(ERROR, "invalid strategy number %d", key->sk_strategy);
    PG_RETURN_BOOL(false);
  }
}

   
                                 
   
                                                                            
                                                           
   
Datum
brin_inclusion_union(PG_FUNCTION_ARGS)
{
  BrinDesc *bdesc = (BrinDesc *)PG_GETARG_POINTER(0);
  BrinValues *col_a = (BrinValues *)PG_GETARG_POINTER(1);
  BrinValues *col_b = (BrinValues *)PG_GETARG_POINTER(2);
  Oid colloid = PG_GET_COLLATION();
  AttrNumber attno;
  Form_pg_attribute attr;
  FmgrInfo *finfo;
  Datum result;

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
    col_a->bv_values[INCLUSION_UNION] = datumCopy(col_b->bv_values[INCLUSION_UNION], attr->attbyval, attr->attlen);
    col_a->bv_values[INCLUSION_UNMERGEABLE] = col_b->bv_values[INCLUSION_UNMERGEABLE];
    col_a->bv_values[INCLUSION_CONTAINS_EMPTY] = col_b->bv_values[INCLUSION_CONTAINS_EMPTY];
    PG_RETURN_VOID();
  }

                                                                  
  if (!DatumGetBool(col_a->bv_values[INCLUSION_CONTAINS_EMPTY]) && DatumGetBool(col_b->bv_values[INCLUSION_CONTAINS_EMPTY]))
  {
    col_a->bv_values[INCLUSION_CONTAINS_EMPTY] = BoolGetDatum(true);
  }

                                                            
  if (DatumGetBool(col_a->bv_values[INCLUSION_UNMERGEABLE]))
  {
    PG_RETURN_VOID();
  }

                                                                        
  if (DatumGetBool(col_b->bv_values[INCLUSION_UNMERGEABLE]))
  {
    col_a->bv_values[INCLUSION_UNMERGEABLE] = BoolGetDatum(true);
    PG_RETURN_VOID();
  }

                                                                   
  finfo = inclusion_get_procinfo(bdesc, attno, PROCNUM_MERGEABLE);
  if (finfo != NULL && !DatumGetBool(FunctionCall2Coll(finfo, colloid, col_a->bv_values[INCLUSION_UNION], col_b->bv_values[INCLUSION_UNION])))
  {
    col_a->bv_values[INCLUSION_UNMERGEABLE] = BoolGetDatum(true);
    PG_RETURN_VOID();
  }

                              
  finfo = inclusion_get_procinfo(bdesc, attno, PROCNUM_MERGE);
  Assert(finfo != NULL);
  result = FunctionCall2Coll(finfo, colloid, col_a->bv_values[INCLUSION_UNION], col_b->bv_values[INCLUSION_UNION]);
  if (!attr->attbyval && DatumGetPointer(result) != DatumGetPointer(col_a->bv_values[INCLUSION_UNION]))
  {
    pfree(DatumGetPointer(col_a->bv_values[INCLUSION_UNION]));

    if (result == col_b->bv_values[INCLUSION_UNION])
    {
      result = datumCopy(result, attr->attbyval, attr->attlen);
    }
  }
  col_a->bv_values[INCLUSION_UNION] = result;

  PG_RETURN_VOID();
}

   
                                                        
   
                                                                           
                                
   
static FmgrInfo *
inclusion_get_procinfo(BrinDesc *bdesc, uint16 attno, uint16 procnum)
{
  InclusionOpaque *opaque;
  uint16 basenum = procnum - PROCNUM_BASE;

     
                                                                       
              
     
  opaque = (InclusionOpaque *)bdesc->bd_info[attno - 1]->oi_opaque;

     
                                                                           
                      
     
  if (opaque->extra_proc_missing[basenum])
  {
    return NULL;
  }

  if (opaque->extra_procinfos[basenum].fn_oid == InvalidOid)
  {
    if (RegProcedureIsValid(index_getprocid(bdesc->bd_index, attno, procnum)))
    {
      fmgr_info_copy(&opaque->extra_procinfos[basenum], index_getprocinfo(bdesc->bd_index, attno, procnum), bdesc->bd_context);
    }
    else
    {
      opaque->extra_proc_missing[basenum] = true;
      return NULL;
    }
  }

  return &opaque->extra_procinfos[basenum];
}

   
                                                        
   
                                                                         
                                                                             
                                                                            
                                                                          
                                              
   
                                                                            
                                                                              
                                                                          
                                                                         
                                                                                
                                                                          
                                            
   
                                                                            
                                     
   
static FmgrInfo *
inclusion_get_strategy_procinfo(BrinDesc *bdesc, uint16 attno, Oid subtype, uint16 strategynum)
{
  InclusionOpaque *opaque;

  Assert(strategynum >= 1 && strategynum <= RTMaxStrategyNumber);

  opaque = (InclusionOpaque *)bdesc->bd_info[attno - 1]->oi_opaque;

     
                                                                            
                                                                     
                                        
     
  if (opaque->cached_subtype != subtype)
  {
    uint16 i;

    for (i = 1; i <= RTMaxStrategyNumber; i++)
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
