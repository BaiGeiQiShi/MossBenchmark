                                                                            
   
                
                                                                      
   
                                                       
   
                           
                             
                                                                         
                                                                         
                    
   
                                                                         
                                                                         
                                                                           
                                                                             
                                                                           
                                    
   
   
                                                                         
                                                                        
   
   
                  
                                        
   
                                                                            
   
#include "postgres.h"

#include "access/tupmacs.h"
#include "lib/stringinfo.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/date.h"
#include "utils/hashutils.h"
#include "utils/int8.h"
#include "utils/lsyscache.h"
#include "utils/rangetypes.h"
#include "utils/timestamp.h"

#define RANGE_EMPTY_LITERAL "empty"

                                                             
typedef struct RangeIOData
{
  TypeCacheEntry *typcache;                                  
  Oid typiofunc;                                             
  Oid typioparam;                                             
  FmgrInfo proc;                                             
} RangeIOData;

static RangeIOData *
get_range_io_data(FunctionCallInfo fcinfo, Oid rngtypid, IOFuncSelector func);
static char
range_parse_flags(const char *flags_str);
static void
range_parse(const char *input_str, char *flags, char **lbound_str, char **ubound_str);
static const char *
range_parse_bound(const char *string, const char *ptr, char **bound_str, bool *infinite);
static char *
range_deparse(char flags, const char *lbound_str, const char *ubound_str);
static char *
range_bound_escape(const char *value);
static Size
datum_compute_size(Size sz, Datum datum, bool typbyval, char typalign, int16 typlen, char typstorage);
static Pointer
datum_write(Pointer ptr, Datum datum, bool typbyval, char typalign, int16 typlen, char typstorage);

   
                                                             
                 
                                                             
   

Datum
range_in(PG_FUNCTION_ARGS)
{
  char *input_str = PG_GETARG_CSTRING(0);
  Oid rngtypoid = PG_GETARG_OID(1);
  Oid typmod = PG_GETARG_INT32(2);
  RangeType *range;
  RangeIOData *cache;
  char flags;
  char *lbound_str;
  char *ubound_str;
  RangeBound lower;
  RangeBound upper;

  check_stack_depth();                                            

  cache = get_range_io_data(fcinfo, rngtypoid, IOFunc_input);

             
  range_parse(input_str, &flags, &lbound_str, &ubound_str);

                                          
  if (RANGE_HAS_LBOUND(flags))
  {
    lower.val = InputFunctionCall(&cache->proc, lbound_str, cache->typioparam, typmod);
  }
  if (RANGE_HAS_UBOUND(flags))
  {
    upper.val = InputFunctionCall(&cache->proc, ubound_str, cache->typioparam, typmod);
  }

  lower.infinite = (flags & RANGE_LB_INF) != 0;
  lower.inclusive = (flags & RANGE_LB_INC) != 0;
  lower.lower = true;
  upper.infinite = (flags & RANGE_UB_INF) != 0;
  upper.inclusive = (flags & RANGE_UB_INC) != 0;
  upper.lower = false;

                                  
  range = make_range(cache->typcache, &lower, &upper, flags & RANGE_EMPTY);

  PG_RETURN_RANGE_P(range);
}

Datum
range_out(PG_FUNCTION_ARGS)
{
  RangeType *range = PG_GETARG_RANGE_P(0);
  char *output_str;
  RangeIOData *cache;
  char flags;
  char *lbound_str = NULL;
  char *ubound_str = NULL;
  RangeBound lower;
  RangeBound upper;
  bool empty;

  check_stack_depth();                                            

  cache = get_range_io_data(fcinfo, RangeTypeGetOid(range), IOFunc_output);

                   
  range_deserialize(cache->typcache, range, &lower, &upper, &empty);
  flags = range_get_flags(range);

                                           
  if (RANGE_HAS_LBOUND(flags))
  {
    lbound_str = OutputFunctionCall(&cache->proc, lower.val);
  }
  if (RANGE_HAS_UBOUND(flags))
  {
    ubound_str = OutputFunctionCall(&cache->proc, upper.val);
  }

                               
  output_str = range_deparse(flags, lbound_str, ubound_str);

  PG_RETURN_CSTRING(output_str);
}

   
                                                                            
                                                                               
                                                                             
                                                             
   

Datum
range_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);
  Oid rngtypoid = PG_GETARG_OID(1);
  int32 typmod = PG_GETARG_INT32(2);
  RangeType *range;
  RangeIOData *cache;
  char flags;
  RangeBound lower;
  RangeBound upper;

  check_stack_depth();                                            

  cache = get_range_io_data(fcinfo, rngtypoid, IOFunc_receive);

                            
  flags = (unsigned char)pq_getmsgbyte(buf);

     
                                                                            
                                                                           
                                                             
     
  flags &= (RANGE_EMPTY | RANGE_LB_INC | RANGE_LB_INF | RANGE_UB_INC | RANGE_UB_INF);

                              
  if (RANGE_HAS_LBOUND(flags))
  {
    uint32 bound_len = pq_getmsgint(buf, 4);
    const char *bound_data = pq_getmsgbytes(buf, bound_len);
    StringInfoData bound_buf;

    initStringInfo(&bound_buf);
    appendBinaryStringInfo(&bound_buf, bound_data, bound_len);

    lower.val = ReceiveFunctionCall(&cache->proc, &bound_buf, cache->typioparam, typmod);
    pfree(bound_buf.data);
  }
  else
  {
    lower.val = (Datum)0;
  }

  if (RANGE_HAS_UBOUND(flags))
  {
    uint32 bound_len = pq_getmsgint(buf, 4);
    const char *bound_data = pq_getmsgbytes(buf, bound_len);
    StringInfoData bound_buf;

    initStringInfo(&bound_buf);
    appendBinaryStringInfo(&bound_buf, bound_data, bound_len);

    upper.val = ReceiveFunctionCall(&cache->proc, &bound_buf, cache->typioparam, typmod);
    pfree(bound_buf.data);
  }
  else
  {
    upper.val = (Datum)0;
  }

  pq_getmsgend(buf);

                                                     
  lower.infinite = (flags & RANGE_LB_INF) != 0;
  lower.inclusive = (flags & RANGE_LB_INC) != 0;
  lower.lower = true;
  upper.infinite = (flags & RANGE_UB_INF) != 0;
  upper.inclusive = (flags & RANGE_UB_INC) != 0;
  upper.lower = false;

                                  
  range = make_range(cache->typcache, &lower, &upper, flags & RANGE_EMPTY);

  PG_RETURN_RANGE_P(range);
}

Datum
range_send(PG_FUNCTION_ARGS)
{
  RangeType *range = PG_GETARG_RANGE_P(0);
  StringInfo buf = makeStringInfo();
  RangeIOData *cache;
  char flags;
  RangeBound lower;
  RangeBound upper;
  bool empty;

  check_stack_depth();                                            

  cache = get_range_io_data(fcinfo, RangeTypeGetOid(range), IOFunc_send);

                   
  range_deserialize(cache->typcache, range, &lower, &upper, &empty);
  flags = range_get_flags(range);

                        
  pq_begintypsend(buf);

  pq_sendbyte(buf, flags);

  if (RANGE_HAS_LBOUND(flags))
  {
    Datum bound = PointerGetDatum(SendFunctionCall(&cache->proc, lower.val));
    uint32 bound_len = VARSIZE(bound) - VARHDRSZ;
    char *bound_data = VARDATA(bound);

    pq_sendint32(buf, bound_len);
    pq_sendbytes(buf, bound_data, bound_len);
  }

  if (RANGE_HAS_UBOUND(flags))
  {
    Datum bound = PointerGetDatum(SendFunctionCall(&cache->proc, upper.val));
    uint32 bound_len = VARSIZE(bound) - VARHDRSZ;
    char *bound_data = VARDATA(bound);

    pq_sendint32(buf, bound_len);
    pq_sendbytes(buf, bound_data, bound_len);
  }

  PG_RETURN_BYTEA_P(pq_endtypsend(buf));
}

   
                                                                       
   
                                                                        
                                                                         
                                  
   
static RangeIOData *
get_range_io_data(FunctionCallInfo fcinfo, Oid rngtypid, IOFuncSelector func)
{
  RangeIOData *cache = (RangeIOData *)fcinfo->flinfo->fn_extra;

  if (cache == NULL || cache->typcache->type_id != rngtypid)
  {
    int16 typlen;
    bool typbyval;
    char typalign;
    char typdelim;

    cache = (RangeIOData *)MemoryContextAlloc(fcinfo->flinfo->fn_mcxt, sizeof(RangeIOData));
    cache->typcache = lookup_type_cache(rngtypid, TYPECACHE_RANGE_INFO);
    if (cache->typcache->rngelemtype == NULL)
    {
      elog(ERROR, "type %u is not a range type", rngtypid);
    }

                                                                    
    get_type_io_data(cache->typcache->rngelemtype->type_id, func, &typlen, &typbyval, &typalign, &typdelim, &cache->typioparam, &cache->typiofunc);

    if (!OidIsValid(cache->typiofunc))
    {
                                                      
      if (func == IOFunc_receive)
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("no binary input function available for type %s", format_type_be(cache->typcache->rngelemtype->type_id))));
      }
      else
      {
        ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("no binary output function available for type %s", format_type_be(cache->typcache->rngelemtype->type_id))));
      }
    }
    fmgr_info_cxt(cache->typiofunc, &cache->proc, fcinfo->flinfo->fn_mcxt);

    fcinfo->flinfo->fn_extra = (void *)cache;
  }

  return cache;
}

   
                                                             
                     
                                                             
   

                                                            
Datum
range_constructor2(PG_FUNCTION_ARGS)
{
  Datum arg1 = PG_GETARG_DATUM(0);
  Datum arg2 = PG_GETARG_DATUM(1);
  Oid rngtypid = get_fn_expr_rettype(fcinfo->flinfo);
  RangeType *range;
  TypeCacheEntry *typcache;
  RangeBound lower;
  RangeBound upper;

  typcache = range_get_typcache(fcinfo, rngtypid);

  lower.val = PG_ARGISNULL(0) ? (Datum)0 : arg1;
  lower.infinite = PG_ARGISNULL(0);
  lower.inclusive = true;
  lower.lower = true;

  upper.val = PG_ARGISNULL(1) ? (Datum)0 : arg2;
  upper.infinite = PG_ARGISNULL(1);
  upper.inclusive = false;
  upper.lower = false;

  range = make_range(typcache, &lower, &upper, false);

  PG_RETURN_RANGE_P(range);
}

                                                        
Datum
range_constructor3(PG_FUNCTION_ARGS)
{
  Datum arg1 = PG_GETARG_DATUM(0);
  Datum arg2 = PG_GETARG_DATUM(1);
  Oid rngtypid = get_fn_expr_rettype(fcinfo->flinfo);
  RangeType *range;
  TypeCacheEntry *typcache;
  RangeBound lower;
  RangeBound upper;
  char flags;

  typcache = range_get_typcache(fcinfo, rngtypid);

  if (PG_ARGISNULL(2))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION), errmsg("range constructor flags argument must not be null")));
  }

  flags = range_parse_flags(text_to_cstring(PG_GETARG_TEXT_PP(2)));

  lower.val = PG_ARGISNULL(0) ? (Datum)0 : arg1;
  lower.infinite = PG_ARGISNULL(0);
  lower.inclusive = (flags & RANGE_LB_INC) != 0;
  lower.lower = true;

  upper.val = PG_ARGISNULL(1) ? (Datum)0 : arg2;
  upper.infinite = PG_ARGISNULL(1);
  upper.inclusive = (flags & RANGE_UB_INC) != 0;
  upper.lower = false;

  range = make_range(typcache, &lower, &upper, false);

  PG_RETURN_RANGE_P(range);
}

                                

                               
Datum
range_lower(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  TypeCacheEntry *typcache;
  RangeBound lower;
  RangeBound upper;
  bool empty;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  range_deserialize(typcache, r1, &lower, &upper, &empty);

                                                    
  if (empty || lower.infinite)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_DATUM(lower.val);
}

                               
Datum
range_upper(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  TypeCacheEntry *typcache;
  RangeBound lower;
  RangeBound upper;
  bool empty;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  range_deserialize(typcache, r1, &lower, &upper, &empty);

                                                    
  if (empty || upper.infinite)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_DATUM(upper.val);
}

                             

                     
Datum
range_empty(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  char flags = range_get_flags(r1);

  PG_RETURN_BOOL(flags & RANGE_EMPTY);
}

                               
Datum
range_lower_inc(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  char flags = range_get_flags(r1);

  PG_RETURN_BOOL(flags & RANGE_LB_INC);
}

                               
Datum
range_upper_inc(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  char flags = range_get_flags(r1);

  PG_RETURN_BOOL(flags & RANGE_UB_INC);
}

                              
Datum
range_lower_inf(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  char flags = range_get_flags(r1);

  PG_RETURN_BOOL(flags & RANGE_LB_INF);
}

                              
Datum
range_upper_inf(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  char flags = range_get_flags(r1);

  PG_RETURN_BOOL(flags & RANGE_UB_INF);
}

                                      

               
Datum
range_contains_elem(PG_FUNCTION_ARGS)
{
  RangeType *r = PG_GETARG_RANGE_P(0);
  Datum val = PG_GETARG_DATUM(1);
  TypeCacheEntry *typcache;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r));

  PG_RETURN_BOOL(range_contains_elem_internal(typcache, r, val));
}

                   
Datum
elem_contained_by_range(PG_FUNCTION_ARGS)
{
  Datum val = PG_GETARG_DATUM(0);
  RangeType *r = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r));

  PG_RETURN_BOOL(range_contains_elem_internal(typcache, r, val));
}

                                    

                                 
bool
range_eq_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{
  RangeBound lower1, lower2;
  RangeBound upper1, upper2;
  bool empty1, empty2;

                                                                      
  if (RangeTypeGetOid(r1) != RangeTypeGetOid(r2))
  {
    elog(ERROR, "range types do not match");
  }

  range_deserialize(typcache, r1, &lower1, &upper1, &empty1);
  range_deserialize(typcache, r2, &lower2, &upper2, &empty2);

  if (empty1 && empty2)
  {
    return true;
  }
  if (empty1 != empty2)
  {
    return false;
  }

  if (range_cmp_bounds(typcache, &lower1, &lower2) != 0)
  {
    return false;
  }

  if (range_cmp_bounds(typcache, &upper1, &upper2) != 0)
  {
    return false;
  }

  return true;
}

              
Datum
range_eq(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  PG_RETURN_BOOL(range_eq_internal(typcache, r1, r2));
}

                                   
bool
range_ne_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{
  return (!range_eq_internal(typcache, r1, r2));
}

                
Datum
range_ne(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  PG_RETURN_BOOL(range_ne_internal(typcache, r1, r2));
}

               
Datum
range_contains(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  PG_RETURN_BOOL(range_contains_internal(typcache, r1, r2));
}

                   
Datum
range_contained_by(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  PG_RETURN_BOOL(range_contained_by_internal(typcache, r1, r2));
}

                                          
bool
range_before_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{
  RangeBound lower1, lower2;
  RangeBound upper1, upper2;
  bool empty1, empty2;

                                                                      
  if (RangeTypeGetOid(r1) != RangeTypeGetOid(r2))
  {
    elog(ERROR, "range types do not match");
  }

  range_deserialize(typcache, r1, &lower1, &upper1, &empty1);
  range_deserialize(typcache, r2, &lower2, &upper2, &empty2);

                                                                  
  if (empty1 || empty2)
  {
    return false;
  }

  return (range_cmp_bounds(typcache, &upper1, &lower2) < 0);
}

                       
Datum
range_before(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  PG_RETURN_BOOL(range_before_internal(typcache, r1, r2));
}

                                           
bool
range_after_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{
  RangeBound lower1, lower2;
  RangeBound upper1, upper2;
  bool empty1, empty2;

                                                                      
  if (RangeTypeGetOid(r1) != RangeTypeGetOid(r2))
  {
    elog(ERROR, "range types do not match");
  }

  range_deserialize(typcache, r1, &lower1, &upper1, &empty1);
  range_deserialize(typcache, r2, &lower2, &upper2, &empty2);

                                                                  
  if (empty1 || empty2)
  {
    return false;
  }

  return (range_cmp_bounds(typcache, &lower1, &upper2) > 0);
}

                        
Datum
range_after(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  PG_RETURN_BOOL(range_after_internal(typcache, r1, r2));
}

   
                                                                               
                                                                            
                                                                              
                                                                               
                                                                
   
                                                                                
                                                                        
                                                                              
         
   
                                                                        
                                                                        
                    
   
                                                                
   
bool
bounds_adjacent(TypeCacheEntry *typcache, RangeBound boundA, RangeBound boundB)
{
  int cmp;

  Assert(!boundA.lower && boundB.lower);

  cmp = range_cmp_bound_values(typcache, &boundA, &boundB);
  if (cmp < 0)
  {
    RangeType *r;

       
                                                                  
       

                                                                         
    if (!OidIsValid(typcache->rng_canonical_finfo.fn_oid))
    {
      return false;
    }

       
                                                                         
                          
       

                                  
    boundA.inclusive = !boundA.inclusive;
    boundB.inclusive = !boundB.inclusive;
                                                            
    boundA.lower = true;
    boundB.lower = false;
    r = make_range(typcache, &boundA, &boundB, false);
    return RangeIsEmpty(r);
  }
  else if (cmp == 0)
  {
    return boundA.inclusive != boundB.inclusive;
  }
  else
  {
    return false;                     
  }
}

                                                           
bool
range_adjacent_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{
  RangeBound lower1, lower2;
  RangeBound upper1, upper2;
  bool empty1, empty2;

                                                                      
  if (RangeTypeGetOid(r1) != RangeTypeGetOid(r2))
  {
    elog(ERROR, "range types do not match");
  }

  range_deserialize(typcache, r1, &lower1, &upper1, &empty1);
  range_deserialize(typcache, r2, &lower2, &upper2, &empty2);

                                                         
  if (empty1 || empty2)
  {
    return false;
  }

     
                                                                            
                                                
     
  return (bounds_adjacent(typcache, upper1, lower2) || bounds_adjacent(typcache, upper2, lower1));
}

                                        
Datum
range_adjacent(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  PG_RETURN_BOOL(range_adjacent_internal(typcache, r1, r2));
}

                                  
bool
range_overlaps_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{
  RangeBound lower1, lower2;
  RangeBound upper1, upper2;
  bool empty1, empty2;

                                                                      
  if (RangeTypeGetOid(r1) != RangeTypeGetOid(r2))
  {
    elog(ERROR, "range types do not match");
  }

  range_deserialize(typcache, r1, &lower1, &upper1, &empty1);
  range_deserialize(typcache, r2, &lower2, &upper2, &empty2);

                                                       
  if (empty1 || empty2)
  {
    return false;
  }

  if (range_cmp_bounds(typcache, &lower1, &lower2) >= 0 && range_cmp_bounds(typcache, &lower1, &upper2) <= 0)
  {
    return true;
  }

  if (range_cmp_bounds(typcache, &lower2, &lower1) >= 0 && range_cmp_bounds(typcache, &lower2, &upper1) <= 0)
  {
    return true;
  }

  return false;
}

               
Datum
range_overlaps(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  PG_RETURN_BOOL(range_overlaps_internal(typcache, r1, r2));
}

                                                     
bool
range_overleft_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{
  RangeBound lower1, lower2;
  RangeBound upper1, upper2;
  bool empty1, empty2;

                                                                      
  if (RangeTypeGetOid(r1) != RangeTypeGetOid(r2))
  {
    elog(ERROR, "range types do not match");
  }

  range_deserialize(typcache, r1, &lower1, &upper1, &empty1);
  range_deserialize(typcache, r2, &lower2, &upper2, &empty2);

                                                                  
  if (empty1 || empty2)
  {
    return false;
  }

  if (range_cmp_bounds(typcache, &upper1, &upper2) <= 0)
  {
    return true;
  }

  return false;
}

                                  
Datum
range_overleft(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  PG_RETURN_BOOL(range_overleft_internal(typcache, r1, r2));
}

                                                    
bool
range_overright_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{
  RangeBound lower1, lower2;
  RangeBound upper1, upper2;
  bool empty1, empty2;

                                                                      
  if (RangeTypeGetOid(r1) != RangeTypeGetOid(r2))
  {
    elog(ERROR, "range types do not match");
  }

  range_deserialize(typcache, r1, &lower1, &upper1, &empty1);
  range_deserialize(typcache, r2, &lower2, &upper2, &empty2);

                                                                  
  if (empty1 || empty2)
  {
    return false;
  }

  if (range_cmp_bounds(typcache, &lower1, &lower2) >= 0)
  {
    return true;
  }

  return false;
}

                                 
Datum
range_overright(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  PG_RETURN_BOOL(range_overright_internal(typcache, r1, r2));
}

                                     

                    
Datum
range_minus(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;
  RangeBound lower1, lower2;
  RangeBound upper1, upper2;
  bool empty1, empty2;
  int cmp_l1l2, cmp_l1u2, cmp_u1l2, cmp_u1u2;

                                                                      
  if (RangeTypeGetOid(r1) != RangeTypeGetOid(r2))
  {
    elog(ERROR, "range types do not match");
  }

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  range_deserialize(typcache, r1, &lower1, &upper1, &empty1);
  range_deserialize(typcache, r2, &lower2, &upper2, &empty2);

                                                    
  if (empty1 || empty2)
  {
    PG_RETURN_RANGE_P(r1);
  }

  cmp_l1l2 = range_cmp_bounds(typcache, &lower1, &lower2);
  cmp_l1u2 = range_cmp_bounds(typcache, &lower1, &upper2);
  cmp_u1l2 = range_cmp_bounds(typcache, &upper1, &lower2);
  cmp_u1u2 = range_cmp_bounds(typcache, &upper1, &upper2);

  if (cmp_l1l2 < 0 && cmp_u1u2 > 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION), errmsg("result of range difference would not be contiguous")));
  }

  if (cmp_l1u2 > 0 || cmp_u1l2 < 0)
  {
    PG_RETURN_RANGE_P(r1);
  }

  if (cmp_l1l2 >= 0 && cmp_u1u2 <= 0)
  {
    PG_RETURN_RANGE_P(make_empty_range(typcache));
  }

  if (cmp_l1l2 <= 0 && cmp_u1l2 >= 0 && cmp_u1u2 <= 0)
  {
    lower2.inclusive = !lower2.inclusive;
    lower2.lower = false;                                     
    PG_RETURN_RANGE_P(make_range(typcache, &lower1, &lower2, false));
  }

  if (cmp_l1l2 >= 0 && cmp_u1u2 >= 0 && cmp_l1u2 <= 0)
  {
    upper2.inclusive = !upper2.inclusive;
    upper2.lower = true;                                     
    PG_RETURN_RANGE_P(make_range(typcache, &upper2, &upper1, false));
  }

  elog(ERROR, "unexpected case in range_minus");
  PG_RETURN_NULL();
}

   
                                                                           
                                    
   
static RangeType *
range_union_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2, bool strict)
{
  RangeBound lower1, lower2;
  RangeBound upper1, upper2;
  bool empty1, empty2;
  RangeBound *result_lower;
  RangeBound *result_upper;

                                                                      
  if (RangeTypeGetOid(r1) != RangeTypeGetOid(r2))
  {
    elog(ERROR, "range types do not match");
  }

  range_deserialize(typcache, r1, &lower1, &upper1, &empty1);
  range_deserialize(typcache, r2, &lower2, &upper2, &empty2);

                                                           
  if (empty1)
  {
    return r2;
  }
  if (empty2)
  {
    return r1;
  }

  if (strict && !DatumGetBool(range_overlaps_internal(typcache, r1, r2)) && !DatumGetBool(range_adjacent_internal(typcache, r1, r2)))
  {
    ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION), errmsg("result of range union would not be contiguous")));
  }

  if (range_cmp_bounds(typcache, &lower1, &lower2) < 0)
  {
    result_lower = &lower1;
  }
  else
  {
    result_lower = &lower2;
  }

  if (range_cmp_bounds(typcache, &upper1, &upper2) > 0)
  {
    result_upper = &upper1;
  }
  else
  {
    result_upper = &upper2;
  }

  return make_range(typcache, result_lower, result_upper, false);
}

Datum
range_union(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  PG_RETURN_RANGE_P(range_union_internal(typcache, r1, r2, true));
}

   
                                                                               
                 
   
Datum
range_merge(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  PG_RETURN_RANGE_P(range_union_internal(typcache, r1, r2, false));
}

                      
Datum
range_intersect(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;
  RangeBound lower1, lower2;
  RangeBound upper1, upper2;
  bool empty1, empty2;
  RangeBound *result_lower;
  RangeBound *result_upper;

                                                                      
  if (RangeTypeGetOid(r1) != RangeTypeGetOid(r2))
  {
    elog(ERROR, "range types do not match");
  }

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  range_deserialize(typcache, r1, &lower1, &upper1, &empty1);
  range_deserialize(typcache, r2, &lower2, &upper2, &empty2);

  if (empty1 || empty2 || !DatumGetBool(range_overlaps(fcinfo)))
  {
    PG_RETURN_RANGE_P(make_empty_range(typcache));
  }

  if (range_cmp_bounds(typcache, &lower1, &lower2) >= 0)
  {
    result_lower = &lower1;
  }
  else
  {
    result_lower = &lower2;
  }

  if (range_cmp_bounds(typcache, &upper1, &upper2) <= 0)
  {
    result_upper = &upper1;
  }
  else
  {
    result_upper = &upper2;
  }

  PG_RETURN_RANGE_P(make_range(typcache, result_lower, result_upper, false));
}

                   

                      
Datum
range_cmp(PG_FUNCTION_ARGS)
{
  RangeType *r1 = PG_GETARG_RANGE_P(0);
  RangeType *r2 = PG_GETARG_RANGE_P(1);
  TypeCacheEntry *typcache;
  RangeBound lower1, lower2;
  RangeBound upper1, upper2;
  bool empty1, empty2;
  int cmp;

  check_stack_depth();                                            

                                                                      
  if (RangeTypeGetOid(r1) != RangeTypeGetOid(r2))
  {
    elog(ERROR, "range types do not match");
  }

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r1));

  range_deserialize(typcache, r1, &lower1, &upper1, &empty1);
  range_deserialize(typcache, r2, &lower2, &upper2, &empty2);

                                                         
  if (empty1 && empty2)
  {
    cmp = 0;
  }
  else if (empty1)
  {
    cmp = -1;
  }
  else if (empty2)
  {
    cmp = 1;
  }
  else
  {
    cmp = range_cmp_bounds(typcache, &lower1, &lower2);
    if (cmp == 0)
    {
      cmp = range_cmp_bounds(typcache, &upper1, &upper2);
    }
  }

  PG_FREE_IF_COPY(r1, 0);
  PG_FREE_IF_COPY(r2, 1);

  PG_RETURN_INT32(cmp);
}

                                                       
Datum
range_lt(PG_FUNCTION_ARGS)
{
  int cmp = range_cmp(fcinfo);

  PG_RETURN_BOOL(cmp < 0);
}

Datum
range_le(PG_FUNCTION_ARGS)
{
  int cmp = range_cmp(fcinfo);

  PG_RETURN_BOOL(cmp <= 0);
}

Datum
range_ge(PG_FUNCTION_ARGS)
{
  int cmp = range_cmp(fcinfo);

  PG_RETURN_BOOL(cmp >= 0);
}

Datum
range_gt(PG_FUNCTION_ARGS)
{
  int cmp = range_cmp(fcinfo);

  PG_RETURN_BOOL(cmp > 0);
}

                  

                        
Datum
hash_range(PG_FUNCTION_ARGS)
{
  RangeType *r = PG_GETARG_RANGE_P(0);
  uint32 result;
  TypeCacheEntry *typcache;
  TypeCacheEntry *scache;
  RangeBound lower;
  RangeBound upper;
  bool empty;
  char flags;
  uint32 lower_hash;
  uint32 upper_hash;

  check_stack_depth();                                            

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r));

                   
  range_deserialize(typcache, r, &lower, &upper, &empty);
  flags = range_get_flags(r);

     
                                                                    
     
  scache = typcache->rngelemtype;
  if (!OidIsValid(scache->hash_proc_finfo.fn_oid))
  {
    scache = lookup_type_cache(scache->type_id, TYPECACHE_HASH_PROC_FINFO);
    if (!OidIsValid(scache->hash_proc_finfo.fn_oid))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not identify a hash function for type %s", format_type_be(scache->type_id))));
    }
  }

     
                                            
     
  if (RANGE_HAS_LBOUND(flags))
  {
    lower_hash = DatumGetUInt32(FunctionCall1Coll(&scache->hash_proc_finfo, typcache->rng_collation, lower.val));
  }
  else
  {
    lower_hash = 0;
  }

  if (RANGE_HAS_UBOUND(flags))
  {
    upper_hash = DatumGetUInt32(FunctionCall1Coll(&scache->hash_proc_finfo, typcache->rng_collation, upper.val));
  }
  else
  {
    upper_hash = 0;
  }

                                        
  result = hash_uint32((uint32)flags);
  result ^= lower_hash;
  result = (result << 1) | (result >> 31);
  result ^= upper_hash;

  PG_RETURN_INT32(result);
}

   
                                                                           
                                     
   
Datum
hash_range_extended(PG_FUNCTION_ARGS)
{
  RangeType *r = PG_GETARG_RANGE_P(0);
  Datum seed = PG_GETARG_DATUM(1);
  uint64 result;
  TypeCacheEntry *typcache;
  TypeCacheEntry *scache;
  RangeBound lower;
  RangeBound upper;
  bool empty;
  char flags;
  uint64 lower_hash;
  uint64 upper_hash;

  check_stack_depth();

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r));

  range_deserialize(typcache, r, &lower, &upper, &empty);
  flags = range_get_flags(r);

  scache = typcache->rngelemtype;
  if (!OidIsValid(scache->hash_extended_proc_finfo.fn_oid))
  {
    scache = lookup_type_cache(scache->type_id, TYPECACHE_HASH_EXTENDED_PROC_FINFO);
    if (!OidIsValid(scache->hash_extended_proc_finfo.fn_oid))
    {
      ereport(ERROR, (errcode(ERRCODE_UNDEFINED_FUNCTION), errmsg("could not identify a hash function for type %s", format_type_be(scache->type_id))));
    }
  }

  if (RANGE_HAS_LBOUND(flags))
  {
    lower_hash = DatumGetUInt64(FunctionCall2Coll(&scache->hash_extended_proc_finfo, typcache->rng_collation, lower.val, seed));
  }
  else
  {
    lower_hash = 0;
  }

  if (RANGE_HAS_UBOUND(flags))
  {
    upper_hash = DatumGetUInt64(FunctionCall2Coll(&scache->hash_extended_proc_finfo, typcache->rng_collation, upper.val, seed));
  }
  else
  {
    upper_hash = 0;
  }

                                        
  result = DatumGetUInt64(hash_uint32_extended((uint32)flags, DatumGetInt64(seed)));
  result ^= lower_hash;
  result = ROTATE_HIGH_AND_LOW_32BITS(result);
  result ^= upper_hash;

  PG_RETURN_UINT64(result);
}

   
                                                             
                       
   
                                                 
                                                             
   

Datum
int4range_canonical(PG_FUNCTION_ARGS)
{
  RangeType *r = PG_GETARG_RANGE_P(0);
  TypeCacheEntry *typcache;
  RangeBound lower;
  RangeBound upper;
  bool empty;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r));

  range_deserialize(typcache, r, &lower, &upper, &empty);

  if (empty)
  {
    PG_RETURN_RANGE_P(r);
  }

  if (!lower.infinite && !lower.inclusive)
  {
    lower.val = DirectFunctionCall2(int4pl, lower.val, Int32GetDatum(1));
    lower.inclusive = true;
  }

  if (!upper.infinite && upper.inclusive)
  {
    upper.val = DirectFunctionCall2(int4pl, upper.val, Int32GetDatum(1));
    upper.inclusive = false;
  }

  PG_RETURN_RANGE_P(range_serialize(typcache, &lower, &upper, false));
}

Datum
int8range_canonical(PG_FUNCTION_ARGS)
{
  RangeType *r = PG_GETARG_RANGE_P(0);
  TypeCacheEntry *typcache;
  RangeBound lower;
  RangeBound upper;
  bool empty;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r));

  range_deserialize(typcache, r, &lower, &upper, &empty);

  if (empty)
  {
    PG_RETURN_RANGE_P(r);
  }

  if (!lower.infinite && !lower.inclusive)
  {
    lower.val = DirectFunctionCall2(int8pl, lower.val, Int64GetDatum(1));
    lower.inclusive = true;
  }

  if (!upper.infinite && upper.inclusive)
  {
    upper.val = DirectFunctionCall2(int8pl, upper.val, Int64GetDatum(1));
    upper.inclusive = false;
  }

  PG_RETURN_RANGE_P(range_serialize(typcache, &lower, &upper, false));
}

Datum
daterange_canonical(PG_FUNCTION_ARGS)
{
  RangeType *r = PG_GETARG_RANGE_P(0);
  TypeCacheEntry *typcache;
  RangeBound lower;
  RangeBound upper;
  bool empty;

  typcache = range_get_typcache(fcinfo, RangeTypeGetOid(r));

  range_deserialize(typcache, r, &lower, &upper, &empty);

  if (empty)
  {
    PG_RETURN_RANGE_P(r);
  }

  if (!lower.infinite && !DATE_NOT_FINITE(DatumGetDateADT(lower.val)) && !lower.inclusive)
  {
    lower.val = DirectFunctionCall2(date_pli, lower.val, Int32GetDatum(1));
    lower.inclusive = true;
  }

  if (!upper.infinite && !DATE_NOT_FINITE(DatumGetDateADT(upper.val)) && upper.inclusive)
  {
    upper.val = DirectFunctionCall2(date_pli, upper.val, Int32GetDatum(1));
    upper.inclusive = false;
  }

  PG_RETURN_RANGE_P(range_serialize(typcache, &lower, &upper, false));
}

   
                                                             
                          
   
                                                
   
                                                                             
                                                               
                                                
                                                             
   

Datum
int4range_subdiff(PG_FUNCTION_ARGS)
{
  int32 v1 = PG_GETARG_INT32(0);
  int32 v2 = PG_GETARG_INT32(1);

  PG_RETURN_FLOAT8((float8)v1 - (float8)v2);
}

Datum
int8range_subdiff(PG_FUNCTION_ARGS)
{
  int64 v1 = PG_GETARG_INT64(0);
  int64 v2 = PG_GETARG_INT64(1);

  PG_RETURN_FLOAT8((float8)v1 - (float8)v2);
}

Datum
numrange_subdiff(PG_FUNCTION_ARGS)
{
  Datum v1 = PG_GETARG_DATUM(0);
  Datum v2 = PG_GETARG_DATUM(1);
  Datum numresult;
  float8 floatresult;

  numresult = DirectFunctionCall2(numeric_sub, v1, v2);

  floatresult = DatumGetFloat8(DirectFunctionCall1(numeric_float8, numresult));

  PG_RETURN_FLOAT8(floatresult);
}

Datum
daterange_subdiff(PG_FUNCTION_ARGS)
{
  int32 v1 = PG_GETARG_INT32(0);
  int32 v2 = PG_GETARG_INT32(1);

  PG_RETURN_FLOAT8((float8)v1 - (float8)v2);
}

Datum
tsrange_subdiff(PG_FUNCTION_ARGS)
{
  Timestamp v1 = PG_GETARG_TIMESTAMP(0);
  Timestamp v2 = PG_GETARG_TIMESTAMP(1);
  float8 result;

  result = ((float8)v1 - (float8)v2) / USECS_PER_SEC;
  PG_RETURN_FLOAT8(result);
}

Datum
tstzrange_subdiff(PG_FUNCTION_ARGS)
{
  Timestamp v1 = PG_GETARG_TIMESTAMP(0);
  Timestamp v2 = PG_GETARG_TIMESTAMP(1);
  float8 result;

  result = ((float8)v1 - (float8)v2) / USECS_PER_SEC;
  PG_RETURN_FLOAT8(result);
}

   
                                                             
                     
   
                                                          
                                               
                                                             
   

   
                                                                 
   
                                                                         
                                                                        
                                                                       
                                  
   
TypeCacheEntry *
range_get_typcache(FunctionCallInfo fcinfo, Oid rngtypid)
{
  TypeCacheEntry *typcache = (TypeCacheEntry *)fcinfo->flinfo->fn_extra;

  if (typcache == NULL || typcache->type_id != rngtypid)
  {
    typcache = lookup_type_cache(rngtypid, TYPECACHE_RANGE_INFO);
    if (typcache->rngelemtype == NULL)
    {
      elog(ERROR, "type %u is not a range type", rngtypid);
    }
    fcinfo->flinfo->fn_extra = (void *)typcache;
  }

  return typcache;
}

   
                                                                       
   
                                                                            
                                                                          
                                                                        
   
RangeType *
range_serialize(TypeCacheEntry *typcache, RangeBound *lower, RangeBound *upper, bool empty)
{
  RangeType *range;
  int cmp;
  Size msize;
  Pointer ptr;
  int16 typlen;
  bool typbyval;
  char typalign;
  char typstorage;
  char flags = 0;

     
                                                                         
                                                                           
     
  Assert(lower->lower);
  Assert(!upper->lower);

  if (empty)
  {
    flags |= RANGE_EMPTY;
  }
  else
  {
    cmp = range_cmp_bound_values(typcache, lower, upper);

                                                                      
    if (cmp > 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION), errmsg("range lower bound must be less than or equal to range upper bound")));
    }

                                                                     
    if (cmp == 0 && !(lower->inclusive && upper->inclusive))
    {
      flags |= RANGE_EMPTY;
    }
    else
    {
                                                   
      if (lower->infinite)
      {
        flags |= RANGE_LB_INF;
      }
      else if (lower->inclusive)
      {
        flags |= RANGE_LB_INC;
      }
      if (upper->infinite)
      {
        flags |= RANGE_UB_INF;
      }
      else if (upper->inclusive)
      {
        flags |= RANGE_UB_INC;
      }
    }
  }

                                                    
  typlen = typcache->rngelemtype->typlen;
  typbyval = typcache->rngelemtype->typbyval;
  typalign = typcache->rngelemtype->typalign;
  typstorage = typcache->rngelemtype->typstorage;

                                                           
  msize = sizeof(RangeType);
  Assert(msize == MAXALIGN(msize));

                              
  if (RANGE_HAS_LBOUND(flags))
  {
       
                                                                           
                                                                     
                                                                          
                                                                        
                                                                       
                                                                         
                                                   
       
    if (typlen == -1)
    {
      lower->val = PointerGetDatum(PG_DETOAST_DATUM_PACKED(lower->val));
    }

    msize = datum_compute_size(msize, lower->val, typbyval, typalign, typlen, typstorage);
  }

  if (RANGE_HAS_UBOUND(flags))
  {
                                                      
    if (typlen == -1)
    {
      upper->val = PointerGetDatum(PG_DETOAST_DATUM_PACKED(upper->val));
    }

    msize = datum_compute_size(msize, upper->val, typbyval, typalign, typlen, typstorage);
  }

                               
  msize += sizeof(char);

                                                                
  range = (RangeType *)palloc0(msize);
  SET_VARSIZE(range, msize);

                             
  range->rangetypid = typcache->type_id;

  ptr = (char *)(range + 1);

  if (RANGE_HAS_LBOUND(flags))
  {
    Assert(lower->lower);
    ptr = datum_write(ptr, lower->val, typbyval, typalign, typlen, typstorage);
  }

  if (RANGE_HAS_UBOUND(flags))
  {
    Assert(!upper->lower);
    ptr = datum_write(ptr, upper->val, typbyval, typalign, typlen, typstorage);
  }

  *((char *)ptr) = flags;

  return range;
}

   
                                                
   
                                                                        
                         
   
                                                                         
                                                                    
   
void
range_deserialize(TypeCacheEntry *typcache, RangeType *range, RangeBound *lower, RangeBound *upper, bool *empty)
{
  char flags;
  int16 typlen;
  bool typbyval;
  char typalign;
  Pointer ptr;
  Datum lbound;
  Datum ubound;

                                                     
  Assert(RangeTypeGetOid(range) == typcache->type_id);

                                                  
  flags = *((char *)range + VARSIZE(range) - 1);

                                                    
  typlen = typcache->rngelemtype->typlen;
  typbyval = typcache->rngelemtype->typbyval;
  typalign = typcache->rngelemtype->typalign;

                                                        
  ptr = (Pointer)(range + 1);

                                 
  if (RANGE_HAS_LBOUND(flags))
  {
                                                    
    lbound = fetch_att(ptr, typbyval, typlen);
    ptr = (Pointer)att_addlength_pointer(ptr, typlen, ptr);
  }
  else
  {
    lbound = (Datum)0;
  }

                                 
  if (RANGE_HAS_UBOUND(flags))
  {
    ptr = (Pointer)att_align_pointer(ptr, typalign, typlen, ptr);
    ubound = fetch_att(ptr, typbyval, typlen);
                                           
  }
  else
  {
    ubound = (Datum)0;
  }

                    

  *empty = (flags & RANGE_EMPTY) != 0;

  lower->val = lbound;
  lower->infinite = (flags & RANGE_LB_INF) != 0;
  lower->inclusive = (flags & RANGE_LB_INC) != 0;
  lower->lower = true;

  upper->val = ubound;
  upper->infinite = (flags & RANGE_UB_INF) != 0;
  upper->inclusive = (flags & RANGE_UB_INC) != 0;
  upper->lower = false;
}

   
                                                               
   
                                                                        
                                          
   
char
range_get_flags(RangeType *range)
{
                                                  
  return *((char *)range + VARSIZE(range) - 1);
}

   
                                                                          
   
                                                                           
                                                                            
               
   
void
range_set_contain_empty(RangeType *range)
{
  char *flagsp;

                                      
  flagsp = (char *)range + VARSIZE(range) - 1;

  *flagsp |= RANGE_CONTAIN_EMPTY;
}

   
                                                                     
                                        
   
RangeType *
make_range(TypeCacheEntry *typcache, RangeBound *lower, RangeBound *upper, bool empty)
{
  RangeType *range;

  range = range_serialize(typcache, lower, upper, empty);

                                                     
  if (OidIsValid(typcache->rng_canonical_finfo.fn_oid) && !RangeIsEmpty(range))
  {
    range = DatumGetRangeTypeP(FunctionCall1(&typcache->rng_canonical_finfo, RangeTypePGetDatum(range)));
  }

  return range;
}

   
                                                                          
                                                          
   
                                                                            
                               
   
                                                                             
                                                                         
   
                                                                             
                                                                             
                                                                          
                                    
   
                                                                               
                                                                            
                                  
   
                                                                         
                                                                             
                                                          
   
int
range_cmp_bounds(TypeCacheEntry *typcache, RangeBound *b1, RangeBound *b2)
{
  int32 result;

     
                                                                          
                          
     
  if (b1->infinite && b2->infinite)
  {
       
                                                                        
                  
       
    if (b1->lower == b2->lower)
    {
      return 0;
    }
    else
    {
      return b1->lower ? -1 : 1;
    }
  }
  else if (b1->infinite)
  {
    return b1->lower ? -1 : 1;
  }
  else if (b2->infinite)
  {
    return b2->lower ? 1 : -1;
  }

     
                                                             
     
  result = DatumGetInt32(FunctionCall2Coll(&typcache->rng_cmp_proc_finfo, typcache->rng_collation, b1->val, b2->val));

     
                                                                         
                                                                            
                                 
     
  if (result == 0)
  {
    if (!b1->inclusive && !b2->inclusive)
    {
                              
      if (b1->lower == b2->lower)
      {
        return 0;
      }
      else
      {
        return b1->lower ? 1 : -1;
      }
    }
    else if (!b1->inclusive)
    {
      return b1->lower ? 1 : -1;
    }
    else if (!b2->inclusive)
    {
      return b2->lower ? -1 : 1;
    }
    else
    {
         
                                                                       
                                                                         
                   
         
      return 0;
    }
  }

  return result;
}

   
                                                                             
                                                             
   
                                                                            
                                                                          
                                                                           
                              
   
int
range_cmp_bound_values(TypeCacheEntry *typcache, RangeBound *b1, RangeBound *b2)
{
     
                                                                          
                          
     
  if (b1->infinite && b2->infinite)
  {
       
                                                                        
                  
       
    if (b1->lower == b2->lower)
    {
      return 0;
    }
    else
    {
      return b1->lower ? -1 : 1;
    }
  }
  else if (b1->infinite)
  {
    return b1->lower ? -1 : 1;
  }
  else if (b2->infinite)
  {
    return b2->lower ? 1 : -1;
  }

     
                                                             
     
  return DatumGetInt32(FunctionCall2Coll(&typcache->rng_cmp_proc_finfo, typcache->rng_collation, b1->val, b2->val));
}

   
                                                                           
   
RangeType *
make_empty_range(TypeCacheEntry *typcache)
{
  RangeBound lower;
  RangeBound upper;

  lower.val = (Datum)0;
  lower.infinite = false;
  lower.inclusive = false;
  lower.lower = true;

  upper.val = (Datum)0;
  upper.infinite = false;
  upper.inclusive = false;
  upper.lower = false;

  return make_range(typcache, &lower, &upper, true);
}

   
                                                             
                    
                                                             
   

   
                                                                              
                          
   
static char
range_parse_flags(const char *flags_str)
{
  char flags = 0;

  if (flags_str[0] == '\0' || flags_str[1] == '\0' || flags_str[2] != '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("invalid range bound flags"), errhint("Valid values are \"[]\", \"[)\", \"(]\", and \"()\".")));
  }

  switch (flags_str[0])
  {
  case '[':
    flags |= RANGE_LB_INC;
    break;
  case '(':
    break;
  default:
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("invalid range bound flags"), errhint("Valid values are \"[]\", \"[)\", \"(]\", and \"()\".")));
  }

  switch (flags_str[1])
  {
  case ']':
    flags |= RANGE_UB_INC;
    break;
  case ')':
    break;
  default:
    ereport(ERROR, (errcode(ERRCODE_SYNTAX_ERROR), errmsg("invalid range bound flags"), errhint("Valid values are \"[]\", \"[)\", \"(]\", and \"()\".")));
  }

  return flags;
}

   
                      
   
                     
                                     
                      
                                  
                                                                      
                                                                      
   
                                                           
                        
                      
                                               
                          
                          
   
                                                                                
                                                                           
   
                                                                               
                                                                         
           
   
                                                                         
                                                                                
                                                                                
   
static void
range_parse(const char *string, char *flags, char **lbound_str, char **ubound_str)
{
  const char *ptr = string;
  bool infinite;

  *flags = 0;

                          
  while (*ptr != '\0' && isspace((unsigned char)*ptr))
  {
    ptr++;
  }

                             
  if (pg_strncasecmp(ptr, RANGE_EMPTY_LITERAL, strlen(RANGE_EMPTY_LITERAL)) == 0)
  {
    *flags = RANGE_EMPTY;
    *lbound_str = NULL;
    *ubound_str = NULL;

    ptr += strlen(RANGE_EMPTY_LITERAL);

                                       
    while (*ptr != '\0' && isspace((unsigned char)*ptr))
    {
      ptr++;
    }

                                         
    if (*ptr != '\0')
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed range literal: \"%s\"", string), errdetail("Junk after \"empty\" key word.")));
    }

    return;
  }

  if (*ptr == '[')
  {
    *flags |= RANGE_LB_INC;
    ptr++;
  }
  else if (*ptr == '(')
  {
    ptr++;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed range literal: \"%s\"", string), errdetail("Missing left parenthesis or bracket.")));
  }

  ptr = range_parse_bound(string, ptr, lbound_str, &infinite);
  if (infinite)
  {
    *flags |= RANGE_LB_INF;
  }

  if (*ptr == ',')
  {
    ptr++;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed range literal: \"%s\"", string), errdetail("Missing comma after lower bound.")));
  }

  ptr = range_parse_bound(string, ptr, ubound_str, &infinite);
  if (infinite)
  {
    *flags |= RANGE_UB_INF;
  }

  if (*ptr == ']')
  {
    *flags |= RANGE_UB_INC;
    ptr++;
  }
  else if (*ptr == ')')
  {
    ptr++;
  }
  else                      
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed range literal: \"%s\"", string), errdetail("Too many commas.")));
  }

                          
  while (*ptr != '\0' && isspace((unsigned char)*ptr))
  {
    ptr++;
  }

  if (*ptr != '\0')
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed range literal: \"%s\"", string), errdetail("Junk after right parenthesis or bracket.")));
  }
}

   
                                                                
   
                                                                     
   
                     
                                                             
                                     
                      
                                                               
                                               
   
                                                                     
   
static const char *
range_parse_bound(const char *string, const char *ptr, char **bound_str, bool *infinite)
{
  StringInfoData buf;

                                                         
  if (*ptr == ',' || *ptr == ')' || *ptr == ']')
  {
    *bound_str = NULL;
    *infinite = true;
  }
  else
  {
                                       
    bool inquote = false;

    initStringInfo(&buf);
    while (inquote || !(*ptr == ',' || *ptr == ')' || *ptr == ']'))
    {
      char ch = *ptr++;

      if (ch == '\0')
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed range literal: \"%s\"", string), errdetail("Unexpected end of input.")));
      }
      if (ch == '\\')
      {
        if (*ptr == '\0')
        {
          ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("malformed range literal: \"%s\"", string), errdetail("Unexpected end of input.")));
        }
        appendStringInfoChar(&buf, *ptr++);
      }
      else if (ch == '"')
      {
        if (!inquote)
        {
          inquote = true;
        }
        else if (*ptr == '"')
        {
                                                   
          appendStringInfoChar(&buf, *ptr++);
        }
        else
        {
          inquote = false;
        }
      }
      else
      {
        appendStringInfoChar(&buf, ch);
      }
    }

    *bound_str = buf.data;
    *infinite = false;
  }

  return ptr;
}

   
                                                   
   
                                                                            
                                                             
   
                               
   
static char *
range_deparse(char flags, const char *lbound_str, const char *ubound_str)
{
  StringInfoData buf;

  if (flags & RANGE_EMPTY)
  {
    return pstrdup(RANGE_EMPTY_LITERAL);
  }

  initStringInfo(&buf);

  appendStringInfoChar(&buf, (flags & RANGE_LB_INC) ? '[' : '(');

  if (RANGE_HAS_LBOUND(flags))
  {
    appendStringInfoString(&buf, range_bound_escape(lbound_str));
  }

  appendStringInfoChar(&buf, ',');

  if (RANGE_HAS_UBOUND(flags))
  {
    appendStringInfoString(&buf, range_bound_escape(ubound_str));
  }

  appendStringInfoChar(&buf, (flags & RANGE_UB_INC) ? ']' : ')');

  return buf.data;
}

   
                                                           
   
                               
   
static char *
range_bound_escape(const char *value)
{
  bool nq;
  const char *ptr;
  StringInfoData buf;

  initStringInfo(&buf);

                                                           
  nq = (value[0] == '\0');                                    
  for (ptr = value; *ptr; ptr++)
  {
    char ch = *ptr;

    if (ch == '"' || ch == '\\' || ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == ',' || isspace((unsigned char)ch))
    {
      nq = true;
      break;
    }
  }

                           
  if (nq)
  {
    appendStringInfoChar(&buf, '"');
  }
  for (ptr = value; *ptr; ptr++)
  {
    char ch = *ptr;

    if (ch == '"' || ch == '\\')
    {
      appendStringInfoChar(&buf, ch);
    }
    appendStringInfoChar(&buf, ch);
  }
  if (nq)
  {
    appendStringInfoChar(&buf, '"');
  }

  return buf.data;
}

   
                                            
   
                                                                               
                                 
   
bool
range_contains_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{
  RangeBound lower1;
  RangeBound upper1;
  bool empty1;
  RangeBound lower2;
  RangeBound upper2;
  bool empty2;

                                                                      
  if (RangeTypeGetOid(r1) != RangeTypeGetOid(r2))
  {
    elog(ERROR, "range types do not match");
  }

  range_deserialize(typcache, r1, &lower1, &upper1, &empty1);
  range_deserialize(typcache, r2, &lower2, &upper2, &empty2);

                                                    
  if (empty2)
  {
    return true;
  }
  else if (empty1)
  {
    return false;
  }

                                                               
  if (range_cmp_bounds(typcache, &lower1, &lower2) > 0)
  {
    return false;
  }
  if (range_cmp_bounds(typcache, &upper1, &upper2) < 0)
  {
    return false;
  }

  return true;
}

bool
range_contained_by_internal(TypeCacheEntry *typcache, RangeType *r1, RangeType *r2)
{
  return range_contains_internal(typcache, r2, r1);
}

   
                                                           
   
bool
range_contains_elem_internal(TypeCacheEntry *typcache, RangeType *r, Datum val)
{
  RangeBound lower;
  RangeBound upper;
  bool empty;
  int32 cmp;

  range_deserialize(typcache, r, &lower, &upper, &empty);

  if (empty)
  {
    return false;
  }

  if (!lower.infinite)
  {
    cmp = DatumGetInt32(FunctionCall2Coll(&typcache->rng_cmp_proc_finfo, typcache->rng_collation, lower.val, val));
    if (cmp > 0)
    {
      return false;
    }
    if (cmp == 0 && !lower.inclusive)
    {
      return false;
    }
  }

  if (!upper.infinite)
  {
    cmp = DatumGetInt32(FunctionCall2Coll(&typcache->rng_cmp_proc_finfo, typcache->rng_collation, upper.val, val));
    if (cmp < 0)
    {
      return false;
    }
    if (cmp == 0 && !upper.inclusive)
    {
      return false;
    }
  }

  return true;
}

   
                                                                       
                                                                     
                                                                          
                                                                        
                                        
   

                                                                        
#define TYPE_IS_PACKABLE(typlen, typstorage) ((typlen) == -1 && (typstorage) != 'p')

   
                                                                         
                                
   
static Size
datum_compute_size(Size data_length, Datum val, bool typbyval, char typalign, int16 typlen, char typstorage)
{
  if (TYPE_IS_PACKABLE(typlen, typstorage) && VARATT_CAN_MAKE_SHORT(DatumGetPointer(val)))
  {
       
                                                                          
                                            
       
    data_length += VARATT_CONVERTED_SHORT_SIZE(DatumGetPointer(val));
  }
  else
  {
    data_length = att_align_datum(data_length, typalign, typlen, val);
    data_length = att_addlength_datum(data_length, typlen, val);
  }

  return data_length;
}

   
                                                                      
                                                                         
   
static Pointer
datum_write(Pointer ptr, Datum datum, bool typbyval, char typalign, int16 typlen, char typstorage)
{
  Size data_length;

  if (typbyval)
  {
                       
    ptr = (char *)att_align_nominal(ptr, typalign);
    store_att_byval(ptr, datum, typlen);
    data_length = typlen;
  }
  else if (typlen == -1)
  {
                 
    Pointer val = DatumGetPointer(datum);

    if (VARATT_IS_EXTERNAL(val))
    {
         
                                                                         
                                                         
         
      elog(ERROR, "cannot store a toast pointer inside a range");
      data_length = 0;                          
    }
    else if (VARATT_IS_SHORT(val))
    {
                                           
      data_length = VARSIZE_SHORT(val);
      memcpy(ptr, val, data_length);
    }
    else if (TYPE_IS_PACKABLE(typlen, typstorage) && VARATT_CAN_MAKE_SHORT(val))
    {
                                                    
      data_length = VARATT_CONVERTED_SHORT_SIZE(val);
      SET_VARSIZE_SHORT(ptr, data_length);
      memcpy(ptr + 1, VARDATA(val), data_length - 1);
    }
    else
    {
                                      
      ptr = (char *)att_align_nominal(ptr, typalign);
      data_length = VARSIZE(val);
      memcpy(ptr, val, data_length);
    }
  }
  else if (typlen == -2)
  {
                                           
    Assert(typalign == 'c');
    data_length = strlen(DatumGetCString(datum)) + 1;
    memcpy(ptr, DatumGetPointer(datum), data_length);
  }
  else
  {
                                        
    ptr = (char *)att_align_nominal(ptr, typalign);
    Assert(typlen > 0);
    data_length = typlen;
    memcpy(ptr, DatumGetPointer(datum), data_length);
  }

  ptr += data_length;

  return ptr;
}
