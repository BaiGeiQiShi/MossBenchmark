                                                                            
   
                 
                                               
   
   
                                                                         
                                                                        
   
                  
                                          
   
                                                                            
   

#include "postgres.h"

#include "access/nbtree.h"
#include "catalog/pg_am.h"
#include "fmgr.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/sortsupport.h"

                                                                              
typedef struct
{
  FmgrInfo flinfo;                                                          
  FunctionCallInfoBaseData fcinfo;                                  
} SortShimExtra;

#define SizeForSortShimExtra(nargs) (offsetof(SortShimExtra, fcinfo) + SizeForFunctionCallInfo(nargs))

   
                                                     
   
                                                                         
                                                                            
                                     
   
static int
comparison_shim(Datum x, Datum y, SortSupport ssup)
{
  SortShimExtra *extra = (SortShimExtra *)ssup->ssup_extra;
  Datum result;

  extra->fcinfo.args[0].value = x;
  extra->fcinfo.args[1].value = y;

                                                           
  extra->fcinfo.isnull = false;

  result = FunctionCallInvoke(&extra->fcinfo);

                                                                        
  if (extra->fcinfo.isnull)
  {
    elog(ERROR, "function %u returned NULL", extra->flinfo.fn_oid);
  }

  return result;
}

   
                                                                        
                                                     
   
void
PrepareSortSupportComparisonShim(Oid cmpFunc, SortSupport ssup)
{
  SortShimExtra *extra;

  extra = (SortShimExtra *)MemoryContextAlloc(ssup->ssup_cxt, SizeForSortShimExtra(2));

                                      
  fmgr_info_cxt(cmpFunc, &extra->flinfo, ssup->ssup_cxt);

                                                              
  InitFunctionCallInfoData(extra->fcinfo, &extra->flinfo, 2, ssup->ssup_collation, NULL, NULL);
  extra->fcinfo.args[0].isnull = false;
  extra->fcinfo.args[1].isnull = false;

  ssup->ssup_extra = extra;
  ssup->comparator = comparison_shim;
}

   
                                                                          
                                                                          
                                   
   
static void
FinishSortSupportFunction(Oid opfamily, Oid opcintype, SortSupport ssup)
{
  Oid sortSupportFunction;

                                        
  sortSupportFunction = get_opfamily_proc(opfamily, opcintype, opcintype, BTSORTSUPPORT_PROC);
  if (OidIsValid(sortSupportFunction))
  {
       
                                                                           
                                                                
       
    OidFunctionCall1(sortSupportFunction, PointerGetDatum(ssup));
  }

  if (ssup->comparator == NULL)
  {
    Oid sortFunction;

    sortFunction = get_opfamily_proc(opfamily, opcintype, opcintype, BTORDER_PROC);

    if (!OidIsValid(sortFunction))
    {
      elog(ERROR, "missing support function %d(%u,%u) in opfamily %u", BTORDER_PROC, opcintype, opcintype, opfamily);
    }

                                                                 
    PrepareSortSupportComparisonShim(sortFunction, ssup);
  }
}

   
                                                                               
   
                                                                             
                                                                             
                                                               
   
void
PrepareSortSupportFromOrderingOp(Oid orderingOp, SortSupport ssup)
{
  Oid opfamily;
  Oid opcintype;
  int16 strategy;

  Assert(ssup->comparator == NULL);

                                    
  if (!get_ordering_op_properties(orderingOp, &opfamily, &opcintype, &strategy))
  {
    elog(ERROR, "operator %u is not a valid ordering operator", orderingOp);
  }
  ssup->ssup_reverse = (strategy == BTGreaterStrategyNumber);

  FinishSortSupportFunction(opfamily, opcintype, ssup);
}

   
                                                                         
   
                                                                             
                                                                               
                                                                              
                                
   
void
PrepareSortSupportFromIndexRel(Relation indexRel, int16 strategy, SortSupport ssup)
{
  Oid opfamily = indexRel->rd_opfamily[ssup->ssup_attno - 1];
  Oid opcintype = indexRel->rd_opcintype[ssup->ssup_attno - 1];

  Assert(ssup->comparator == NULL);

  if (indexRel->rd_rel->relam != BTREE_AM_OID)
  {
    elog(ERROR, "unexpected non-btree AM: %u", indexRel->rd_rel->relam);
  }
  if (strategy != BTGreaterStrategyNumber && strategy != BTLessStrategyNumber)
  {
    elog(ERROR, "unexpected sort support strategy: %d", strategy);
  }
  ssup->ssup_reverse = (strategy == BTGreaterStrategyNumber);

  FinishSortSupportFunction(opfamily, opcintype, ssup);
}
