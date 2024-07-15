                                                                            
   
             
                                                                 
   
                                                                     
   
                                                                         
                                                                         
                                                                 
                                                                         
                  
   
                                                                
   
                  
                                     
   
                                                                            
   

#include "postgres.h"

#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <math.h>

#include "catalog/pg_type.h"
#include "common/int.h"
#include "funcapi.h"
#include "lib/hyperloglog.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/float.h"
#include "utils/guc.h"
#include "utils/hashutils.h"
#include "utils/int8.h"
#include "utils/numeric.h"
#include "utils/sortsupport.h"

              
                                                                   
                                                                             
              
                      
   

              
                    
   
                                                                         
                                                                           
                                                                            
                                                                             
                                                                            
                                                                         
                                                                              
                                                                              
                                
   
                                                                               
                                                                                
                                                                            
                                                                         
                                                 
              
   

#if 0
#define NBASE 10
#define HALF_NBASE 5
#define DEC_DIGITS 1                                           
#define MUL_GUARD_DIGITS 4                                         
#define DIV_GUARD_DIGITS 8

typedef signed char NumericDigit;
#endif

#if 0
#define NBASE 100
#define HALF_NBASE 50
#define DEC_DIGITS 2                                           
#define MUL_GUARD_DIGITS 3                                         
#define DIV_GUARD_DIGITS 6

typedef signed char NumericDigit;
#endif

#if 1
#define NBASE 10000
#define HALF_NBASE 5000
#define DEC_DIGITS 4                                           
#define MUL_GUARD_DIGITS 2                                         
#define DIV_GUARD_DIGITS 4

typedef int16 NumericDigit;
#endif

   
                                       
   
                                                                       
                                                                          
                                                                       
                                                                             
                                                                            
                                                                           
                                                                              
                                                                       
                                                                             
                                                                           
               
   
                                                                        
                                                                        
                                                                        
                                                            
   
                                                                       
                                                                         
                                  
   
                                                                        
                                                                            
                                                                        
                                                                         
   

struct NumericShort
{
  uint16 n_header;                                                               
  NumericDigit n_data[FLEXIBLE_ARRAY_MEMBER];             
};

struct NumericLong
{
  uint16 n_sign_dscale;                                                 
  int16 n_weight;                                                      
  NumericDigit n_data[FLEXIBLE_ARRAY_MEMBER];             
};

union NumericChoice
{
  uint16 n_header;                              
  struct NumericLong n_long;                                  
  struct NumericShort n_short;                                 
};

struct NumericData
{
  int32 vl_len_;                                                           
  union NumericChoice choice;                       
};

   
                                
   

#define NUMERIC_SIGN_MASK 0xC000
#define NUMERIC_POS 0x0000
#define NUMERIC_NEG 0x4000
#define NUMERIC_SHORT 0x8000
#define NUMERIC_NAN 0xC000

#define NUMERIC_FLAGBITS(n) ((n)->choice.n_header & NUMERIC_SIGN_MASK)
#define NUMERIC_IS_NAN(n) (NUMERIC_FLAGBITS(n) == NUMERIC_NAN)
#define NUMERIC_IS_SHORT(n) (NUMERIC_FLAGBITS(n) == NUMERIC_SHORT)

#define NUMERIC_HDRSZ (VARHDRSZ + sizeof(uint16) + sizeof(int16))
#define NUMERIC_HDRSZ_SHORT (VARHDRSZ + sizeof(uint16))

   
                                                                                
                                                                               
                                                                
   
#define NUMERIC_HEADER_IS_SHORT(n) (((n)->choice.n_header & 0x8000) != 0)
#define NUMERIC_HEADER_SIZE(n) (VARHDRSZ + sizeof(uint16) + (NUMERIC_HEADER_IS_SHORT(n) ? 0 : sizeof(int16)))

   
                             
   

#define NUMERIC_SHORT_SIGN_MASK 0x2000
#define NUMERIC_SHORT_DSCALE_MASK 0x1F80
#define NUMERIC_SHORT_DSCALE_SHIFT 7
#define NUMERIC_SHORT_DSCALE_MAX (NUMERIC_SHORT_DSCALE_MASK >> NUMERIC_SHORT_DSCALE_SHIFT)
#define NUMERIC_SHORT_WEIGHT_SIGN_MASK 0x0040
#define NUMERIC_SHORT_WEIGHT_MASK 0x003F
#define NUMERIC_SHORT_WEIGHT_MAX NUMERIC_SHORT_WEIGHT_MASK
#define NUMERIC_SHORT_WEIGHT_MIN (-(NUMERIC_SHORT_WEIGHT_MASK + 1))

   
                                        
   

#define NUMERIC_DSCALE_MASK 0x3FFF
#define NUMERIC_DSCALE_MAX NUMERIC_DSCALE_MASK

#define NUMERIC_SIGN(n) (NUMERIC_IS_SHORT(n) ? (((n)->choice.n_short.n_header & NUMERIC_SHORT_SIGN_MASK) ? NUMERIC_NEG : NUMERIC_POS) : NUMERIC_FLAGBITS(n))
#define NUMERIC_DSCALE(n) (NUMERIC_HEADER_IS_SHORT((n)) ? ((n)->choice.n_short.n_header & NUMERIC_SHORT_DSCALE_MASK) >> NUMERIC_SHORT_DSCALE_SHIFT : ((n)->choice.n_long.n_sign_dscale & NUMERIC_DSCALE_MASK))
#define NUMERIC_WEIGHT(n) (NUMERIC_HEADER_IS_SHORT((n)) ? (((n)->choice.n_short.n_header & NUMERIC_SHORT_WEIGHT_SIGN_MASK ? ~NUMERIC_SHORT_WEIGHT_MASK : 0) | ((n)->choice.n_short.n_header & NUMERIC_SHORT_WEIGHT_MASK)) : ((n)->choice.n_long.n_weight))

              
                                                                         
                                                                         
            
   
                                                                            
                                
   
                                                                             
                                                                         
                                                                        
   
                                                                         
                                                                        
                                                                         
                                                                             
                                                                             
                                                                             
                                                                            
                                    
   
                                                                           
                                                            
   
                                                                          
                                                                          
                                                                              
                                                                            
                                                                              
                                                                               
                                                                           
                                                                          
   
                                                                       
                                                                               
                                         
                                                                           
                                   
   
                                                                           
                                                                           
                                                                            
                                                                          
   
                                                                             
                                                                           
                                                                            
              
   
typedef struct NumericVar
{
  int ndigits;                                                   
  int weight;                                      
  int sign;                                                           
  int dscale;                              
  NumericDigit *buf;                                              
  NumericDigit *digits;                        
} NumericVar;

              
                            
              
   
typedef struct
{
  NumericVar current;
  NumericVar stop;
  NumericVar step;
} generate_series_numeric_fctx;

              
                 
              
   
typedef struct
{
  void *buf;                                        
  int64 input_count;                                     
  bool estimating;                                       

  hyperLogLogState abbr_card;                            
} NumericSortSupport;

              
                         
   
                                                                             
                                                                             
                                                                           
                                                                            
                                                                         
                                                                           
   
                                                                            
                                                                              
                                                                             
                                                                          
                                                                           
                                                                 
   
                                                                        
                                                                             
                                                                            
                                                                             
                                                                           
                                                                         
                                        
   
                                                                      
   
                                         
              
   
typedef struct NumericSumAccum
{
  int ndigits;
  int weight;
  int dscale;
  int num_uncarried;
  bool have_carry_space;
  int32 *pos_digits;
  int32 *neg_digits;
} NumericSumAccum;

   
                                                                      
                                                                     
                                                                       
                                                                        
   
#define NUMERIC_ABBREV_BITS (SIZEOF_DATUM * BITS_PER_BYTE)
#if SIZEOF_DATUM == 8
#define NumericAbbrevGetDatum(X) ((Datum)(X))
#define DatumGetNumericAbbrev(X) ((int64)(X))
#define NUMERIC_ABBREV_NAN NumericAbbrevGetDatum(PG_INT64_MIN)
#else
#define NumericAbbrevGetDatum(X) ((Datum)(X))
#define DatumGetNumericAbbrev(X) ((int32)(X))
#define NUMERIC_ABBREV_NAN NumericAbbrevGetDatum(PG_INT32_MIN)
#endif

              
                                 
              
   
static const NumericDigit const_zero_data[1] = {0};
static const NumericVar const_zero = {0, 0, NUMERIC_POS, 0, NULL, (NumericDigit *)const_zero_data};

static const NumericDigit const_one_data[1] = {1};
static const NumericVar const_one = {1, 0, NUMERIC_POS, 0, NULL, (NumericDigit *)const_one_data};

static const NumericDigit const_two_data[1] = {2};
static const NumericVar const_two = {1, 0, NUMERIC_POS, 0, NULL, (NumericDigit *)const_two_data};

#if DEC_DIGITS == 4
static const NumericDigit const_zero_point_five_data[1] = {5000};
#elif DEC_DIGITS == 2
static const NumericDigit const_zero_point_five_data[1] = {50};
#elif DEC_DIGITS == 1
static const NumericDigit const_zero_point_five_data[1] = {5};
#endif
static const NumericVar const_zero_point_five = {1, -1, NUMERIC_POS, 1, NULL, (NumericDigit *)const_zero_point_five_data};

#if DEC_DIGITS == 4
static const NumericDigit const_zero_point_nine_data[1] = {9000};
#elif DEC_DIGITS == 2
static const NumericDigit const_zero_point_nine_data[1] = {90};
#elif DEC_DIGITS == 1
static const NumericDigit const_zero_point_nine_data[1] = {9};
#endif
static const NumericVar const_zero_point_nine = {1, -1, NUMERIC_POS, 1, NULL, (NumericDigit *)const_zero_point_nine_data};

#if DEC_DIGITS == 4
static const NumericDigit const_one_point_one_data[2] = {1, 1000};
#elif DEC_DIGITS == 2
static const NumericDigit const_one_point_one_data[2] = {1, 10};
#elif DEC_DIGITS == 1
static const NumericDigit const_one_point_one_data[2] = {1, 1};
#endif
static const NumericVar const_one_point_one = {2, 0, NUMERIC_POS, 1, NULL, (NumericDigit *)const_one_point_one_data};

static const NumericVar const_nan = {0, 0, NUMERIC_NAN, 0, NULL, NULL};

#if DEC_DIGITS == 4
static const int round_powers[4] = {0, 1000, 100, 10};
#endif

              
                   
              
   

#ifdef NUMERIC_DEBUG
static void
dump_numeric(const char *str, Numeric num);
static void
dump_var(const char *str, NumericVar *var);
#else
#define dump_numeric(s, n)
#define dump_var(s, v)
#endif

#define digitbuf_alloc(ndigits) ((NumericDigit *)palloc((ndigits) * sizeof(NumericDigit)))
#define digitbuf_free(buf)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if ((buf) != NULL)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
      pfree(buf);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  } while (0)

#define init_var(v) MemSetAligned(v, 0, sizeof(NumericVar))

#define NUMERIC_DIGITS(num) (NUMERIC_HEADER_IS_SHORT(num) ? (num)->choice.n_short.n_data : (num)->choice.n_long.n_data)
#define NUMERIC_NDIGITS(num) ((VARSIZE(num) - NUMERIC_HEADER_SIZE(num)) / sizeof(NumericDigit))
#define NUMERIC_CAN_BE_SHORT(scale, weight) ((scale) <= NUMERIC_SHORT_DSCALE_MAX && (weight) <= NUMERIC_SHORT_WEIGHT_MAX && (weight) >= NUMERIC_SHORT_WEIGHT_MIN)

static void
alloc_var(NumericVar *var, int ndigits);
static void
free_var(NumericVar *var);
static void
zero_var(NumericVar *var);

static const char *
set_var_from_str(const char *str, const char *cp, NumericVar *dest);
static void
set_var_from_num(Numeric value, NumericVar *dest);
static void
init_var_from_num(Numeric num, NumericVar *dest);
static void
set_var_from_var(const NumericVar *value, NumericVar *dest);
static char *
get_str_from_var(const NumericVar *var);
static char *
get_str_from_var_sci(const NumericVar *var, int rscale);

static Numeric
make_result(const NumericVar *var);
static Numeric
make_result_opt_error(const NumericVar *var, bool *error);

static void
apply_typmod(NumericVar *var, int32 typmod);

static bool
numericvar_to_int32(const NumericVar *var, int32 *result);
static bool
numericvar_to_int64(const NumericVar *var, int64 *result);
static void
int64_to_numericvar(int64 val, NumericVar *var);
#ifdef HAVE_INT128
static bool
numericvar_to_int128(const NumericVar *var, int128 *result);
static void
int128_to_numericvar(int128 val, NumericVar *var);
#endif
static double
numeric_to_double_no_overflow(Numeric num);
static double
numericvar_to_double_no_overflow(const NumericVar *var);

static Datum
numeric_abbrev_convert(Datum original_datum, SortSupport ssup);
static bool
numeric_abbrev_abort(int memtupcount, SortSupport ssup);
static int
numeric_fast_cmp(Datum x, Datum y, SortSupport ssup);
static int
numeric_cmp_abbrev(Datum x, Datum y, SortSupport ssup);

static Datum
numeric_abbrev_convert_var(const NumericVar *var, NumericSortSupport *nss);

static int
cmp_numerics(Numeric num1, Numeric num2);
static int
cmp_var(const NumericVar *var1, const NumericVar *var2);
static int
cmp_var_common(const NumericDigit *var1digits, int var1ndigits, int var1weight, int var1sign, const NumericDigit *var2digits, int var2ndigits, int var2weight, int var2sign);
static void
add_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result);
static void
sub_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result);
static void
mul_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result, int rscale);
static void
div_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result, int rscale, bool round);
static void
div_var_fast(const NumericVar *var1, const NumericVar *var2, NumericVar *result, int rscale, bool round);
static int
select_div_scale(const NumericVar *var1, const NumericVar *var2);
static void
mod_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result);
static void
ceil_var(const NumericVar *var, NumericVar *result);
static void
floor_var(const NumericVar *var, NumericVar *result);

static void
sqrt_var(const NumericVar *arg, NumericVar *result, int rscale);
static void
exp_var(const NumericVar *arg, NumericVar *result, int rscale);
static int
estimate_ln_dweight(const NumericVar *var);
static void
ln_var(const NumericVar *arg, NumericVar *result, int rscale);
static void
log_var(const NumericVar *base, const NumericVar *num, NumericVar *result);
static void
power_var(const NumericVar *base, const NumericVar *exp, NumericVar *result);
static void
power_var_int(const NumericVar *base, int exp, NumericVar *result, int rscale);
static void
power_ten_int(int exp, NumericVar *result);

static int
cmp_abs(const NumericVar *var1, const NumericVar *var2);
static int
cmp_abs_common(const NumericDigit *var1digits, int var1ndigits, int var1weight, const NumericDigit *var2digits, int var2ndigits, int var2weight);
static void
add_abs(const NumericVar *var1, const NumericVar *var2, NumericVar *result);
static void
sub_abs(const NumericVar *var1, const NumericVar *var2, NumericVar *result);
static void
round_var(NumericVar *var, int rscale);
static void
trunc_var(NumericVar *var, int rscale);
static void
strip_var(NumericVar *var);
static void
compute_bucket(Numeric operand, Numeric bound1, Numeric bound2, const NumericVar *count_var, NumericVar *result_var);

static void
accum_sum_add(NumericSumAccum *accum, const NumericVar *var1);
static void
accum_sum_rescale(NumericSumAccum *accum, const NumericVar *val);
static void
accum_sum_carry(NumericSumAccum *accum);
static void
accum_sum_reset(NumericSumAccum *accum);
static void
accum_sum_final(NumericSumAccum *accum, NumericVar *result);
static void
accum_sum_copy(NumericSumAccum *dst, NumericSumAccum *src);
static void
accum_sum_combine(NumericSumAccum *accum, NumericSumAccum *accum2);

                                                                          
   
                                          
   
                                                                          
   

   
                  
   
                                        
   
Datum
numeric_in(PG_FUNCTION_ARGS)
{
  char *str = PG_GETARG_CSTRING(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 typmod = PG_GETARG_INT32(2);
  Numeric res;
  const char *cp;

                           
  cp = str;
  while (*cp)
  {
    if (!isspace((unsigned char)*cp))
    {
      break;
    }
    cp++;
  }

     
                   
     
  if (pg_strncasecmp(cp, "NaN", 3) == 0)
  {
    res = make_result(&const_nan);

                                           
    cp += 3;
    while (*cp)
    {
      if (!isspace((unsigned char)*cp))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "numeric", str)));
      }
      cp++;
    }
  }
  else
  {
       
                                                              
       
    NumericVar value;

    init_var(&value);

    cp = set_var_from_str(str, cp, &value);

       
                                                                      
                                                                      
                                                                        
                                                                
       
    while (*cp)
    {
      if (!isspace((unsigned char)*cp))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "numeric", str)));
      }
      cp++;
    }

    apply_typmod(&value, typmod);

    res = make_result(&value);
    free_var(&value);
  }

  PG_RETURN_NUMERIC(res);
}

   
                   
   
                                         
   
Datum
numeric_out(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  NumericVar x;
  char *str;

     
                
     
  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_CSTRING(pstrdup("NaN"));
  }

     
                                            
     
  init_var_from_num(num, &x);

  str = get_str_from_var(&x);

  PG_RETURN_CSTRING(str);
}

   
                      
   
                           
   
bool
numeric_is_nan(Numeric num)
{
  return NUMERIC_IS_NAN(num);
}

   
                            
   
                                                                            
   
int32
numeric_maximum_size(int32 typmod)
{
  int precision;
  int numeric_digits;

  if (typmod < (int32)(VARHDRSZ))
  {
    return -1;
  }

                                                                  
  precision = ((typmod - VARHDRSZ) >> 16) & 0xffff;

     
                                                                             
                                                                           
                                                                           
                                                                            
                                                                          
                                                                     
                                                                    
                   
     
  numeric_digits = (precision + 2 * (DEC_DIGITS - 1)) / DEC_DIGITS;

     
                                                                         
                                                                           
                                                                           
                                                                             
                     
     
  return NUMERIC_HDRSZ + (numeric_digits * sizeof(NumericDigit));
}

   
                       
   
                                                                 
   
char *
numeric_out_sci(Numeric num, int scale)
{
  NumericVar x;
  char *str;

     
                
     
  if (NUMERIC_IS_NAN(num))
  {
    return pstrdup("NaN");
  }

  init_var_from_num(num, &x);

  str = get_str_from_var_sci(&x, scale);

  return str;
}

   
                         
   
                                                                             
                                                                         
                                                                          
                  
   
char *
numeric_normalize(Numeric num)
{
  NumericVar x;
  char *str;
  int last;

     
                
     
  if (NUMERIC_IS_NAN(num))
  {
    return pstrdup("NaN");
  }

  init_var_from_num(num, &x);

  str = get_str_from_var(&x);

                                                                         
  if (strchr(str, '.') != NULL)
  {
       
                                                                          
                                               
       
    last = strlen(str) - 1;
    while (str[last] == '0')
    {
      last--;
    }

                                                                        
    if (str[last] == '.')
    {
      last--;
    }

                                            
    str[last + 1] = '\0';
  }

  return str;
}

   
                                                                
   
                                             
                                                 
   
Datum
numeric_recv(PG_FUNCTION_ARGS)
{
  StringInfo buf = (StringInfo)PG_GETARG_POINTER(0);

#ifdef NOT_USED
  Oid typelem = PG_GETARG_OID(1);
#endif
  int32 typmod = PG_GETARG_INT32(2);
  NumericVar value;
  Numeric res;
  int len, i;

  init_var(&value);

  len = (uint16)pq_getmsgint(buf, sizeof(uint16));

  alloc_var(&value, len);

  value.weight = (int16)pq_getmsgint(buf, sizeof(int16));
                                             

  value.sign = (uint16)pq_getmsgint(buf, sizeof(uint16));
  if (!(value.sign == NUMERIC_POS || value.sign == NUMERIC_NEG || value.sign == NUMERIC_NAN))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("invalid sign in external \"numeric\" value")));
  }

  value.dscale = (uint16)pq_getmsgint(buf, sizeof(uint16));
  if ((value.dscale & NUMERIC_DSCALE_MASK) != value.dscale)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("invalid scale in external \"numeric\" value")));
  }

  for (i = 0; i < len; i++)
  {
    NumericDigit d = pq_getmsgint(buf, sizeof(NumericDigit));

    if (d < 0 || d >= NBASE)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_BINARY_REPRESENTATION), errmsg("invalid digit in external \"numeric\" value")));
    }
    value.digits[i] = d;
  }

     
                                                                            
                                                                           
                                                                          
                                  
     
  trunc_var(&value, value.dscale);

  apply_typmod(&value, typmod);

  res = make_result(&value);
  free_var(&value);

  PG_RETURN_NUMERIC(res);
}

   
                                                       
   
Datum
numeric_send(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  NumericVar x;
  StringInfoData buf;
  int i;

  init_var_from_num(num, &x);

  pq_begintypsend(&buf);

  pq_sendint16(&buf, x.ndigits);
  pq_sendint16(&buf, x.weight);
  pq_sendint16(&buf, x.sign);
  pq_sendint16(&buf, x.dscale);
  for (i = 0; i < x.ndigits; i++)
  {
    pq_sendint16(&buf, x.digits[i]);
  }

  PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
}

   
                     
   
                                                                        
   
                                                                         
                                                                              
                                                                              
                                                                              
   
Datum
numeric_support(PG_FUNCTION_ARGS)
{
  Node *rawreq = (Node *)PG_GETARG_POINTER(0);
  Node *ret = NULL;

  if (IsA(rawreq, SupportRequestSimplify))
  {
    SupportRequestSimplify *req = (SupportRequestSimplify *)rawreq;
    FuncExpr *expr = req->fcall;
    Node *typmod;

    Assert(list_length(expr->args) >= 2);

    typmod = (Node *)lsecond(expr->args);

    if (IsA(typmod, Const) && !((Const *)typmod)->constisnull)
    {
      Node *source = (Node *)linitial(expr->args);
      int32 old_typmod = exprTypmod(source);
      int32 new_typmod = DatumGetInt32(((Const *)typmod)->constvalue);
      int32 old_scale = (old_typmod - VARHDRSZ) & 0xffff;
      int32 new_scale = (new_typmod - VARHDRSZ) & 0xffff;
      int32 old_precision = (old_typmod - VARHDRSZ) >> 16 & 0xffff;
      int32 new_precision = (new_typmod - VARHDRSZ) >> 16 & 0xffff;

         
                                                                     
                                                                     
                                                                     
                                                                     
                         
         
      if (new_typmod < (int32)VARHDRSZ || (old_typmod >= (int32)VARHDRSZ && new_scale == old_scale && new_precision >= old_precision))
      {
        ret = relabel_to_typmod(source, new_typmod);
      }
    }
  }

  PG_RETURN_POINTER(ret);
}

   
               
   
                                                                     
                                                                      
                                                           
   
Datum
numeric(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  int32 typmod = PG_GETARG_INT32(1);
  Numeric new;
  int32 tmp_typmod;
  int precision;
  int scale;
  int ddigits;
  int maxdigits;
  NumericVar var;

     
                
     
  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

     
                                                                           
                 
     
  if (typmod < (int32)(VARHDRSZ))
  {
    new = (Numeric)palloc(VARSIZE(num));
    memcpy(new, num, VARSIZE(num));
    PG_RETURN_NUMERIC(new);
  }

     
                                                         
     
  tmp_typmod = typmod - VARHDRSZ;
  precision = (tmp_typmod >> 16) & 0xffff;
  scale = tmp_typmod & 0xffff;
  maxdigits = precision - scale;

     
                                                                         
                                                                           
                                                                        
                                                                   
                
     
  ddigits = (NUMERIC_WEIGHT(num) + 1) * DEC_DIGITS;
  if (ddigits <= maxdigits && scale >= NUMERIC_DSCALE(num) && (NUMERIC_CAN_BE_SHORT(scale, NUMERIC_WEIGHT(num)) || !NUMERIC_IS_SHORT(num)))
  {
    new = (Numeric)palloc(VARSIZE(num));
    memcpy(new, num, VARSIZE(num));
    if (NUMERIC_IS_SHORT(num))
    {
      new->choice.n_short.n_header = (num->choice.n_short.n_header & ~NUMERIC_SHORT_DSCALE_MASK) | (scale << NUMERIC_SHORT_DSCALE_SHIFT);
    }
    else
    {
      new->choice.n_long.n_sign_dscale = NUMERIC_SIGN(new) | ((uint16)scale & NUMERIC_DSCALE_MASK);
    }
    PG_RETURN_NUMERIC(new);
  }

     
                                                                     
                                            
     
  init_var(&var);

  set_var_from_num(num, &var);
  apply_typmod(&var, typmod);
  new = make_result(&var);

  free_var(&var);

  PG_RETURN_NUMERIC(new);
}

Datum
numerictypmodin(PG_FUNCTION_ARGS)
{
  ArrayType *ta = PG_GETARG_ARRAYTYPE_P(0);
  int32 *tl;
  int n;
  int32 typmod;

  tl = ArrayGetIntegerTypmods(ta, &n);

  if (n == 2)
  {
    if (tl[0] < 1 || tl[0] > NUMERIC_MAX_PRECISION)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("NUMERIC precision %d must be between 1 and %d", tl[0], NUMERIC_MAX_PRECISION)));
    }
    if (tl[1] < 0 || tl[1] > tl[0])
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("NUMERIC scale %d must be between 0 and precision %d", tl[1], tl[0])));
    }
    typmod = ((tl[0] << 16) | tl[1]) + VARHDRSZ;
  }
  else if (n == 1)
  {
    if (tl[0] < 1 || tl[0] > NUMERIC_MAX_PRECISION)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("NUMERIC precision %d must be between 1 and %d", tl[0], NUMERIC_MAX_PRECISION)));
    }
                                
    typmod = (tl[0] << 16) + VARHDRSZ;
  }
  else
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("invalid NUMERIC type modifier")));
    typmod = 0;                          
  }

  PG_RETURN_INT32(typmod);
}

Datum
numerictypmodout(PG_FUNCTION_ARGS)
{
  int32 typmod = PG_GETARG_INT32(0);
  char *res = (char *)palloc(64);

  if (typmod >= 0)
  {
    snprintf(res, 64, "(%d,%d)", ((typmod - VARHDRSZ) >> 16) & 0xffff, (typmod - VARHDRSZ) & 0xffff);
  }
  else
  {
    *res = '\0';
  }

  PG_RETURN_CSTRING(res);
}

                                                                          
   
                                            
   
                                                                          
   

Datum
numeric_abs(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  Numeric res;

     
                
     
  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

     
                                                      
     
  res = (Numeric)palloc(VARSIZE(num));
  memcpy(res, num, VARSIZE(num));

  if (NUMERIC_IS_SHORT(num))
  {
    res->choice.n_short.n_header = num->choice.n_short.n_header & ~NUMERIC_SHORT_SIGN_MASK;
  }
  else
  {
    res->choice.n_long.n_sign_dscale = NUMERIC_POS | NUMERIC_DSCALE(num);
  }

  PG_RETURN_NUMERIC(res);
}

Datum
numeric_uminus(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  Numeric res;

     
                
     
  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

     
                                                      
     
  res = (Numeric)palloc(VARSIZE(num));
  memcpy(res, num, VARSIZE(num));

     
                                                                            
                                                                             
                        
     
  if (NUMERIC_NDIGITS(num) != 0)
  {
                             
    if (NUMERIC_IS_SHORT(num))
    {
      res->choice.n_short.n_header = num->choice.n_short.n_header ^ NUMERIC_SHORT_SIGN_MASK;
    }
    else if (NUMERIC_SIGN(num) == NUMERIC_POS)
    {
      res->choice.n_long.n_sign_dscale = NUMERIC_NEG | NUMERIC_DSCALE(num);
    }
    else
    {
      res->choice.n_long.n_sign_dscale = NUMERIC_POS | NUMERIC_DSCALE(num);
    }
  }

  PG_RETURN_NUMERIC(res);
}

Datum
numeric_uplus(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  Numeric res;

  res = (Numeric)palloc(VARSIZE(num));
  memcpy(res, num, VARSIZE(num));

  PG_RETURN_NUMERIC(res);
}

   
                    
   
                                                                         
                                                     
   
Datum
numeric_sign(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  Numeric res;
  NumericVar result;

     
                
     
  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

  init_var(&result);

     
                                                                            
                                                                         
     
  if (NUMERIC_NDIGITS(num) == 0)
  {
    set_var_from_var(&const_zero, &result);
  }
  else
  {
       
                                                                           
                
       
    set_var_from_var(&const_one, &result);
    result.sign = NUMERIC_SIGN(num);
  }

  res = make_result(&result);
  free_var(&result);

  PG_RETURN_NUMERIC(res);
}

   
                     
   
                                                                 
                                                                   
                                                  
   
Datum
numeric_round(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  int32 scale = PG_GETARG_INT32(1);
  Numeric res;
  NumericVar arg;

     
                
     
  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

     
                                                                      
     
  scale = Max(scale, -NUMERIC_MAX_RESULT_SCALE);
  scale = Min(scale, NUMERIC_MAX_RESULT_SCALE);

     
                                                                   
     
  init_var(&arg);
  set_var_from_num(num, &arg);

  round_var(&arg, scale);

                                             
  if (scale < 0)
  {
    arg.dscale = 0;
  }

     
                               
     
  res = make_result(&arg);

  free_var(&arg);
  PG_RETURN_NUMERIC(res);
}

   
                     
   
                                                                    
                                                                       
                                                    
   
Datum
numeric_trunc(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  int32 scale = PG_GETARG_INT32(1);
  Numeric res;
  NumericVar arg;

     
                
     
  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

     
                                                                      
     
  scale = Max(scale, -NUMERIC_MAX_RESULT_SCALE);
  scale = Min(scale, NUMERIC_MAX_RESULT_SCALE);

     
                                                                      
     
  init_var(&arg);
  set_var_from_num(num, &arg);

  trunc_var(&arg, scale);

                                             
  if (scale < 0)
  {
    arg.dscale = 0;
  }

     
                                 
     
  res = make_result(&arg);

  free_var(&arg);
  PG_RETURN_NUMERIC(res);
}

   
                    
   
                                                                     
   
Datum
numeric_ceil(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  Numeric res;
  NumericVar result;

  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

  init_var_from_num(num, &result);
  ceil_var(&result, &result);

  res = make_result(&result);
  free_var(&result);

  PG_RETURN_NUMERIC(res);
}

   
                     
   
                                                                 
   
Datum
numeric_floor(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  Numeric res;
  NumericVar result;

  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

  init_var_from_num(num, &result);
  floor_var(&result, &result);

  res = make_result(&result);
  free_var(&result);

  PG_RETURN_NUMERIC(res);
}

   
                               
   
                               
   
Datum
generate_series_numeric(PG_FUNCTION_ARGS)
{
  return generate_series_step_numeric(fcinfo);
}

Datum
generate_series_step_numeric(PG_FUNCTION_ARGS)
{
  generate_series_numeric_fctx *fctx;
  FuncCallContext *funcctx;
  MemoryContext oldcontext;

  if (SRF_IS_FIRSTCALL())
  {
    Numeric start_num = PG_GETARG_NUMERIC(0);
    Numeric stop_num = PG_GETARG_NUMERIC(1);
    NumericVar steploc = const_one;

                                             
    if (NUMERIC_IS_NAN(start_num))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("start value cannot be NaN")));
    }

    if (NUMERIC_IS_NAN(stop_num))
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("stop value cannot be NaN")));
    }

                                                    
    if (PG_NARGS() == 3)
    {
      Numeric step_num = PG_GETARG_NUMERIC(2);

      if (NUMERIC_IS_NAN(step_num))
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("step size cannot be NaN")));
      }

      init_var_from_num(step_num, &steploc);

      if (cmp_var(&steploc, &const_zero) == 0)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("step size cannot equal zero")));
      }
    }

                                                              
    funcctx = SRF_FIRSTCALL_INIT();

       
                                                                         
       
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

                                          
    fctx = (generate_series_numeric_fctx *)palloc(sizeof(generate_series_numeric_fctx));

       
                                                                       
                                                                     
                                                                        
                                     
       
    init_var(&fctx->current);
    init_var(&fctx->stop);
    init_var(&fctx->step);

    set_var_from_num(start_num, &fctx->current);
    set_var_from_num(stop_num, &fctx->stop);
    set_var_from_var(&steploc, &fctx->step);

    funcctx->user_fctx = fctx;
    MemoryContextSwitchTo(oldcontext);
  }

                                                
  funcctx = SRF_PERCALL_SETUP();

     
                                                                     
                
     
  fctx = funcctx->user_fctx;

  if ((fctx->step.sign == NUMERIC_POS && cmp_var(&fctx->current, &fctx->stop) <= 0) || (fctx->step.sign == NUMERIC_NEG && cmp_var(&fctx->current, &fctx->stop) >= 0))
  {
    Numeric result = make_result(&fctx->current);

                                                                        
    oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

                                                             
    add_var(&fctx->current, &fctx->step, &fctx->current);
    MemoryContextSwitchTo(oldcontext);

                                            
    SRF_RETURN_NEXT(funcctx, NumericGetDatum(result));
  }
  else
  {
                                       
    SRF_RETURN_DONE(funcctx);
  }
}

   
                                                                 
                                                       
   
                                                               
                                                                     
                                                                      
                                                                     
                                                                   
                                                                    
                                                                
                                                                    
   
Datum
width_bucket_numeric(PG_FUNCTION_ARGS)
{
  Numeric operand = PG_GETARG_NUMERIC(0);
  Numeric bound1 = PG_GETARG_NUMERIC(1);
  Numeric bound2 = PG_GETARG_NUMERIC(2);
  int32 count = PG_GETARG_INT32(3);
  NumericVar count_var;
  NumericVar result_var;
  int32 result;

  if (count <= 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_WIDTH_BUCKET_FUNCTION), errmsg("count must be greater than zero")));
  }

  if (NUMERIC_IS_NAN(operand) || NUMERIC_IS_NAN(bound1) || NUMERIC_IS_NAN(bound2))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_WIDTH_BUCKET_FUNCTION), errmsg("operand, lower bound, and upper bound cannot be NaN")));
  }

  init_var(&result_var);
  init_var(&count_var);

                                                           
  int64_to_numericvar((int64)count, &count_var);

  switch (cmp_numerics(bound1, bound2))
  {
  case 0:
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_WIDTH_BUCKET_FUNCTION), errmsg("lower bound cannot equal upper bound")));
    break;

                         
  case -1:
    if (cmp_numerics(operand, bound1) < 0)
    {
      set_var_from_var(&const_zero, &result_var);
    }
    else if (cmp_numerics(operand, bound2) >= 0)
    {
      add_var(&count_var, &const_one, &result_var);
    }
    else
    {
      compute_bucket(operand, bound1, bound2, &count_var, &result_var);
    }
    break;

                         
  case 1:
    if (cmp_numerics(operand, bound1) > 0)
    {
      set_var_from_var(&const_zero, &result_var);
    }
    else if (cmp_numerics(operand, bound2) <= 0)
    {
      add_var(&count_var, &const_one, &result_var);
    }
    else
    {
      compute_bucket(operand, bound1, bound2, &count_var, &result_var);
    }
    break;
  }

                                                                    
  if (!numericvar_to_int32(&result_var, &result))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
  }

  free_var(&count_var);
  free_var(&result_var);

  PG_RETURN_INT32(result);
}

   
                                                                       
                                                                    
                                               
   
static void
compute_bucket(Numeric operand, Numeric bound1, Numeric bound2, const NumericVar *count_var, NumericVar *result_var)
{
  NumericVar bound1_var;
  NumericVar bound2_var;
  NumericVar operand_var;

  init_var_from_num(bound1, &bound1_var);
  init_var_from_num(bound2, &bound2_var);
  init_var_from_num(operand, &operand_var);

  if (cmp_var(&bound1_var, &bound2_var) < 0)
  {
    sub_var(&operand_var, &bound1_var, &operand_var);
    sub_var(&bound2_var, &bound1_var, &bound2_var);
    div_var(&operand_var, &bound2_var, result_var, select_div_scale(&operand_var, &bound2_var), true);
  }
  else
  {
    sub_var(&bound1_var, &operand_var, &operand_var);
    sub_var(&bound1_var, &bound2_var, &bound1_var);
    div_var(&operand_var, &bound1_var, result_var, select_div_scale(&operand_var, &bound1_var), true);
  }

  mul_var(result_var, count_var, result_var, result_var->dscale + count_var->dscale);
  add_var(result_var, &const_one, result_var);
  floor_var(result_var, result_var);

  free_var(&bound1_var);
  free_var(&bound2_var);
  free_var(&operand_var);
}

                                                                          
   
                        
   
                                                                          
                                                                           
                          
   
                 
   
                                                                                
                                                                               
                                                                           
                                                                              
                                 
   
                                                                           
                                                                                
                                                                                
                                                                                
                                                                            
                                                                  
   
                                                                              
                                                                            
                                                                            
                                                                           
                                                                              
                                                                               
                                                                                
                                                                             
                                                                              
                                                                                
                                                             
   
                                                                          
   

   
                                  
   
Datum
numeric_sortsupport(PG_FUNCTION_ARGS)
{
  SortSupport ssup = (SortSupport)PG_GETARG_POINTER(0);

  ssup->comparator = numeric_fast_cmp;

  if (ssup->abbreviate)
  {
    NumericSortSupport *nss;
    MemoryContext oldcontext = MemoryContextSwitchTo(ssup->ssup_cxt);

    nss = palloc(sizeof(NumericSortSupport));

       
                                                                           
                          
       
    nss->buf = palloc(VARATT_SHORT_MAX + VARHDRSZ + 1);

    nss->input_count = 0;
    nss->estimating = true;
    initHyperLogLog(&nss->abbr_card, 10);

    ssup->ssup_extra = nss;

    ssup->abbrev_full_comparator = ssup->comparator;
    ssup->comparator = numeric_cmp_abbrev;
    ssup->abbrev_converter = numeric_abbrev_convert;
    ssup->abbrev_abort = numeric_abbrev_abort;

    MemoryContextSwitchTo(oldcontext);
  }

  PG_RETURN_VOID();
}

   
                                                            
                           
   
static Datum
numeric_abbrev_convert(Datum original_datum, SortSupport ssup)
{
  NumericSortSupport *nss = ssup->ssup_extra;
  void *original_varatt = PG_DETOAST_DATUM_PACKED(original_datum);
  Numeric value;
  Datum result;

  nss->input_count += 1;

     
                                                                           
                                                                        
     
  if (VARATT_IS_SHORT(original_varatt))
  {
    void *buf = nss->buf;
    Size sz = VARSIZE_SHORT(original_varatt) - VARHDRSZ_SHORT;

    Assert(sz <= VARATT_SHORT_MAX - VARHDRSZ_SHORT);

    SET_VARSIZE(buf, VARHDRSZ + sz);
    memcpy(VARDATA(buf), VARDATA_SHORT(original_varatt), sz);

    value = (Numeric)buf;
  }
  else
  {
    value = (Numeric)original_varatt;
  }

  if (NUMERIC_IS_NAN(value))
  {
    result = NUMERIC_ABBREV_NAN;
  }
  else
  {
    NumericVar var;

    init_var_from_num(value, &var);

    result = numeric_abbrev_convert_var(&var, nss);
  }

                                                         
  if ((Pointer)original_varatt != DatumGetPointer(original_datum))
  {
    pfree(original_varatt);
  }

  return result;
}

   
                                           
   
                                                                                
                                                                               
                                                                               
                                                 
   
static bool
numeric_abbrev_abort(int memtupcount, SortSupport ssup)
{
  NumericSortSupport *nss = ssup->ssup_extra;
  double abbr_card;

  if (memtupcount < 10000 || nss->input_count < 10000 || !nss->estimating)
  {
    return false;
  }

  abbr_card = estimateHyperLogLog(&nss->abbr_card);

     
                                                                         
                                                                           
                                                                         
                             
     
  if (abbr_card > 100000.0)
  {
#ifdef TRACE_SORT
    if (trace_sort)
    {
      elog(LOG,
          "numeric_abbrev: estimation ends at cardinality %f"
          " after " INT64_FORMAT " values (%d rows)",
          abbr_card, nss->input_count, memtupcount);
    }
#endif
    nss->estimating = false;
    return false;
  }

     
                                                                        
                                                                    
                                                                            
                                                                      
                                                                        
                                                                          
                                                             
     
  if (abbr_card < nss->input_count / 10000.0 + 0.5)
  {
#ifdef TRACE_SORT
    if (trace_sort)
    {
      elog(LOG,
          "numeric_abbrev: aborting abbreviation at cardinality %f"
          " below threshold %f after " INT64_FORMAT " values (%d rows)",
          abbr_card, nss->input_count / 10000.0 + 0.5, nss->input_count, memtupcount);
    }
#endif
    return true;
  }

#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG,
        "numeric_abbrev: cardinality %f"
        " after " INT64_FORMAT " values (%d rows)",
        abbr_card, nss->input_count, memtupcount);
  }
#endif

  return false;
}

   
                                                                              
                                                                               
                                                                             
                  
   
                                                                               
                                                                             
                        
   
static int
numeric_fast_cmp(Datum x, Datum y, SortSupport ssup)
{
  Numeric nx = DatumGetNumeric(x);
  Numeric ny = DatumGetNumeric(y);
  int result;

  result = cmp_numerics(nx, ny);

  if ((Pointer)nx != DatumGetPointer(x))
  {
    pfree(nx);
  }
  if ((Pointer)ny != DatumGetPointer(y))
  {
    pfree(ny);
  }

  return result;
}

   
                                                                               
                                                                         
                                 
   
static int
numeric_cmp_abbrev(Datum x, Datum y, SortSupport ssup)
{
     
                                                                             
                                                            
     
  if (DatumGetNumericAbbrev(x) < DatumGetNumericAbbrev(y))
  {
    return 1;
  }
  if (DatumGetNumericAbbrev(x) > DatumGetNumericAbbrev(y))
  {
    return -1;
  }
  return 0;
}

   
                                                                
   
                                       
   
                                                
   
                                                                            
                                                                               
                                                                             
                                                                              
                                                                          
                                                                               
                                                                               
                                                                            
                                                                     
                                                                              
                                                                               
                                                                     
   
                                                          
   
                                       
   
                                                    
   
                                                                             
                                                                             
                                                                               
                                                                      
                                                                       
                                                                              
                                                                           
                                                                              
                                                                                
                     
   
                                                                                
                                               
   
                                                                              
                                                                               
                                                                               
          
   

#if NUMERIC_ABBREV_BITS == 64

static Datum
numeric_abbrev_convert_var(const NumericVar *var, NumericSortSupport *nss)
{
  int ndigits = var->ndigits;
  int weight = var->weight;
  int64 result;

  if (ndigits == 0 || weight < -44)
  {
    result = 0;
  }
  else if (weight > 83)
  {
    result = PG_INT64_MAX;
  }
  else
  {
    result = ((int64)(weight + 44) << 56);

    switch (ndigits)
    {
    default:
      result |= ((int64)var->digits[3]);
                       
    case 3:
      result |= ((int64)var->digits[2]) << 14;
                       
    case 2:
      result |= ((int64)var->digits[1]) << 28;
                       
    case 1:
      result |= ((int64)var->digits[0]) << 42;
      break;
    }
  }

                                                      
  if (var->sign == NUMERIC_POS)
  {
    result = -result;
  }

  if (nss->estimating)
  {
    uint32 tmp = ((uint32)result ^ (uint32)((uint64)result >> 32));

    addHyperLogLog(&nss->abbr_card, DatumGetUInt32(hash_uint32(tmp)));
  }

  return NumericAbbrevGetDatum(result);
}

#endif                                

#if NUMERIC_ABBREV_BITS == 32

static Datum
numeric_abbrev_convert_var(const NumericVar *var, NumericSortSupport *nss)
{
  int ndigits = var->ndigits;
  int weight = var->weight;
  int32 result;

  if (ndigits == 0 || weight < -11)
  {
    result = 0;
  }
  else if (weight > 20)
  {
    result = PG_INT32_MAX;
  }
  else
  {
    NumericDigit nxt1 = (ndigits > 1) ? var->digits[1] : 0;

    weight = (weight + 11) * 4;

    result = var->digits[0];

       
                                                                       
                                                                 
       

    if (result > 999)
    {
                                             
      result = (result * 1000) + (nxt1 / 10);
      weight += 3;
    }
    else if (result > 99)
    {
                                             
      result = (result * 10000) + nxt1;
      weight += 2;
    }
    else if (result > 9)
    {
      NumericDigit nxt2 = (ndigits > 2) ? var->digits[2] : 0;

                                             
      result = (result * 100000) + (nxt1 * 10) + (nxt2 / 1000);
      weight += 1;
    }
    else
    {
      NumericDigit nxt2 = (ndigits > 2) ? var->digits[2] : 0;

                                            
      result = (result * 1000000) + (nxt1 * 100) + (nxt2 / 100);
    }

    result = result | (weight << 24);
  }

                                                      
  if (var->sign == NUMERIC_POS)
  {
    result = -result;
  }

  if (nss->estimating)
  {
    uint32 tmp = (uint32)result;

    addHyperLogLog(&nss->abbr_card, DatumGetUInt32(hash_uint32(tmp)));
  }

  return NumericAbbrevGetDatum(result);
}

#endif                                

   
                                                  
   

Datum
numeric_cmp(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  int result;

  result = cmp_numerics(num1, num2);

  PG_FREE_IF_COPY(num1, 0);
  PG_FREE_IF_COPY(num2, 1);

  PG_RETURN_INT32(result);
}

Datum
numeric_eq(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  bool result;

  result = cmp_numerics(num1, num2) == 0;

  PG_FREE_IF_COPY(num1, 0);
  PG_FREE_IF_COPY(num2, 1);

  PG_RETURN_BOOL(result);
}

Datum
numeric_ne(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  bool result;

  result = cmp_numerics(num1, num2) != 0;

  PG_FREE_IF_COPY(num1, 0);
  PG_FREE_IF_COPY(num2, 1);

  PG_RETURN_BOOL(result);
}

Datum
numeric_gt(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  bool result;

  result = cmp_numerics(num1, num2) > 0;

  PG_FREE_IF_COPY(num1, 0);
  PG_FREE_IF_COPY(num2, 1);

  PG_RETURN_BOOL(result);
}

Datum
numeric_ge(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  bool result;

  result = cmp_numerics(num1, num2) >= 0;

  PG_FREE_IF_COPY(num1, 0);
  PG_FREE_IF_COPY(num2, 1);

  PG_RETURN_BOOL(result);
}

Datum
numeric_lt(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  bool result;

  result = cmp_numerics(num1, num2) < 0;

  PG_FREE_IF_COPY(num1, 0);
  PG_FREE_IF_COPY(num2, 1);

  PG_RETURN_BOOL(result);
}

Datum
numeric_le(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  bool result;

  result = cmp_numerics(num1, num2) <= 0;

  PG_FREE_IF_COPY(num1, 0);
  PG_FREE_IF_COPY(num2, 1);

  PG_RETURN_BOOL(result);
}

static int
cmp_numerics(Numeric num1, Numeric num2)
{
  int result;

     
                                                                           
                                                                          
            
     
  if (NUMERIC_IS_NAN(num1))
  {
    if (NUMERIC_IS_NAN(num2))
    {
      result = 0;                
    }
    else
    {
      result = 1;                    
    }
  }
  else if (NUMERIC_IS_NAN(num2))
  {
    result = -1;                    
  }
  else
  {
    result = cmp_var_common(NUMERIC_DIGITS(num1), NUMERIC_NDIGITS(num1), NUMERIC_WEIGHT(num1), NUMERIC_SIGN(num1), NUMERIC_DIGITS(num2), NUMERIC_NDIGITS(num2), NUMERIC_WEIGHT(num2), NUMERIC_SIGN(num2));
  }

  return result;
}

   
                                          
   
Datum
in_range_numeric_numeric(PG_FUNCTION_ARGS)
{
  Numeric val = PG_GETARG_NUMERIC(0);
  Numeric base = PG_GETARG_NUMERIC(1);
  Numeric offset = PG_GETARG_NUMERIC(2);
  bool sub = PG_GETARG_BOOL(3);
  bool less = PG_GETARG_BOOL(4);
  bool result;

     
                                                                      
                                                              
     
  if (NUMERIC_IS_NAN(offset) || NUMERIC_SIGN(offset) == NUMERIC_NEG)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_PRECEDING_OR_FOLLOWING_SIZE), errmsg("invalid preceding or following size in window function")));
  }

     
                                                                           
                                                                          
                     
     
  if (NUMERIC_IS_NAN(val))
  {
    if (NUMERIC_IS_NAN(base))
    {
      result = true;                
    }
    else
    {
      result = !less;                    
    }
  }
  else if (NUMERIC_IS_NAN(base))
  {
    result = less;                    
  }
  else
  {
       
                                                                   
                                                                       
                                                         
       
    NumericVar valv;
    NumericVar basev;
    NumericVar offsetv;
    NumericVar sum;

    init_var_from_num(val, &valv);
    init_var_from_num(base, &basev);
    init_var_from_num(offset, &offsetv);
    init_var(&sum);

    if (sub)
    {
      sub_var(&basev, &offsetv, &sum);
    }
    else
    {
      add_var(&basev, &offsetv, &sum);
    }

    if (less)
    {
      result = (cmp_var(&valv, &sum) <= 0);
    }
    else
    {
      result = (cmp_var(&valv, &sum) >= 0);
    }

    free_var(&sum);
  }

  PG_FREE_IF_COPY(val, 0);
  PG_FREE_IF_COPY(base, 1);
  PG_FREE_IF_COPY(offset, 2);

  PG_RETURN_BOOL(result);
}

Datum
hash_numeric(PG_FUNCTION_ARGS)
{
  Numeric key = PG_GETARG_NUMERIC(0);
  Datum digit_hash;
  Datum result;
  int weight;
  int start_offset;
  int end_offset;
  int i;
  int hash_len;
  NumericDigit *digits;

                                                             
  if (NUMERIC_IS_NAN(key))
  {
    PG_RETURN_UINT32(0);
  }

  weight = NUMERIC_WEIGHT(key);
  start_offset = 0;
  end_offset = 0;

     
                                                                        
                                                                         
                                                                        
                                                                       
     
  digits = NUMERIC_DIGITS(key);
  for (i = 0; i < NUMERIC_NDIGITS(key); i++)
  {
    if (digits[i] != (NumericDigit)0)
    {
      break;
    }

    start_offset++;

       
                                                                           
                                                      
       
    weight--;
  }

     
                                                                            
                                     
     
  if (NUMERIC_NDIGITS(key) == start_offset)
  {
    PG_RETURN_UINT32(-1);
  }

  for (i = NUMERIC_NDIGITS(key) - 1; i >= 0; i--)
  {
    if (digits[i] != (NumericDigit)0)
    {
      break;
    }

    end_offset++;
  }

                                                                   
  Assert(start_offset + end_offset < NUMERIC_NDIGITS(key));

     
                                                                            
                                                                        
                                                                          
                                        
     
  hash_len = NUMERIC_NDIGITS(key) - start_offset - end_offset;
  digit_hash = hash_any((unsigned char *)(NUMERIC_DIGITS(key) + start_offset), hash_len * sizeof(NumericDigit));

                                  
  result = digit_hash ^ weight;

  PG_RETURN_DATUM(result);
}

   
                                                                           
                                       
   
Datum
hash_numeric_extended(PG_FUNCTION_ARGS)
{
  Numeric key = PG_GETARG_NUMERIC(0);
  uint64 seed = PG_GETARG_INT64(1);
  Datum digit_hash;
  Datum result;
  int weight;
  int start_offset;
  int end_offset;
  int i;
  int hash_len;
  NumericDigit *digits;

  if (NUMERIC_IS_NAN(key))
  {
    PG_RETURN_UINT64(seed);
  }

  weight = NUMERIC_WEIGHT(key);
  start_offset = 0;
  end_offset = 0;

  digits = NUMERIC_DIGITS(key);
  for (i = 0; i < NUMERIC_NDIGITS(key); i++)
  {
    if (digits[i] != (NumericDigit)0)
    {
      break;
    }

    start_offset++;

    weight--;
  }

  if (NUMERIC_NDIGITS(key) == start_offset)
  {
    PG_RETURN_UINT64(seed - 1);
  }

  for (i = NUMERIC_NDIGITS(key) - 1; i >= 0; i--)
  {
    if (digits[i] != (NumericDigit)0)
    {
      break;
    }

    end_offset++;
  }

  Assert(start_offset + end_offset < NUMERIC_NDIGITS(key));

  hash_len = NUMERIC_NDIGITS(key) - start_offset - end_offset;
  digit_hash = hash_any_extended((unsigned char *)(NUMERIC_DIGITS(key) + start_offset), hash_len * sizeof(NumericDigit), seed);

  result = UInt64GetDatum(DatumGetUInt64(digit_hash) ^ weight);

  PG_RETURN_DATUM(result);
}

                                                                          
   
                              
   
                                                                          
   

   
                   
   
                    
   
Datum
numeric_add(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  Numeric res;

  res = numeric_add_opt_error(num1, num2, NULL);

  PG_RETURN_NUMERIC(res);
}

   
                             
   
                                                                          
                                                                          
                                    
   
Numeric
numeric_add_opt_error(Numeric num1, Numeric num2, bool *have_error)
{
  NumericVar arg1;
  NumericVar arg2;
  NumericVar result;
  Numeric res;

     
                
     
  if (NUMERIC_IS_NAN(num1) || NUMERIC_IS_NAN(num2))
  {
    return make_result(&const_nan);
  }

     
                                                                        
     
  init_var_from_num(num1, &arg1);
  init_var_from_num(num2, &arg2);

  init_var(&result);
  add_var(&arg1, &arg2, &result);

  res = make_result_opt_error(&result, have_error);

  free_var(&result);

  return res;
}

   
                   
   
                                     
   
Datum
numeric_sub(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  Numeric res;

  res = numeric_sub_opt_error(num1, num2, NULL);

  PG_RETURN_NUMERIC(res);
}

   
                             
   
                                                                          
                                                                          
                                    
   
Numeric
numeric_sub_opt_error(Numeric num1, Numeric num2, bool *have_error)
{
  NumericVar arg1;
  NumericVar arg2;
  NumericVar result;
  Numeric res;

     
                
     
  if (NUMERIC_IS_NAN(num1) || NUMERIC_IS_NAN(num2))
  {
    return make_result(&const_nan);
  }

     
                                                                        
     
  init_var_from_num(num1, &arg1);
  init_var_from_num(num2, &arg2);

  init_var(&result);
  sub_var(&arg1, &arg2, &result);

  res = make_result_opt_error(&result, have_error);

  free_var(&result);

  return res;
}

   
                   
   
                                         
   
Datum
numeric_mul(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  Numeric res;

  res = numeric_mul_opt_error(num1, num2, NULL);

  PG_RETURN_NUMERIC(res);
}

   
                             
   
                                                                          
                                                                          
                                    
   
Numeric
numeric_mul_opt_error(Numeric num1, Numeric num2, bool *have_error)
{
  NumericVar arg1;
  NumericVar arg2;
  NumericVar result;
  Numeric res;

     
                
     
  if (NUMERIC_IS_NAN(num1) || NUMERIC_IS_NAN(num2))
  {
    return make_result(&const_nan);
  }

     
                                                                        
                                                                             
                                                                             
                                                                             
                                                                            
                                                                           
                                                                       
                                                                        
                                
     
  init_var_from_num(num1, &arg1);
  init_var_from_num(num2, &arg2);

  init_var(&result);
  mul_var(&arg1, &arg2, &result, arg1.dscale + arg2.dscale);

  if (result.dscale > NUMERIC_DSCALE_MAX)
  {
    round_var(&result, NUMERIC_DSCALE_MAX);
  }

  res = make_result_opt_error(&result, have_error);

  free_var(&result);

  return res;
}

   
                   
   
                                   
   
Datum
numeric_div(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  Numeric res;

  res = numeric_div_opt_error(num1, num2, NULL);

  PG_RETURN_NUMERIC(res);
}

   
                             
   
                                                                          
                                                                          
                                    
   
Numeric
numeric_div_opt_error(Numeric num1, Numeric num2, bool *have_error)
{
  NumericVar arg1;
  NumericVar arg2;
  NumericVar result;
  Numeric res;
  int rscale;

     
                
     
  if (NUMERIC_IS_NAN(num1) || NUMERIC_IS_NAN(num2))
  {
    return make_result(&const_nan);
  }

     
                          
     
  init_var_from_num(num1, &arg1);
  init_var_from_num(num2, &arg2);

  init_var(&result);

     
                                      
     
  rscale = select_div_scale(&arg1, &arg2);

     
                                                                  
     
  if (have_error && (arg2.ndigits == 0 || arg2.digits[0] == 0))
  {
    *have_error = true;
    return NULL;
  }

     
                                         
     
  div_var(&arg1, &arg2, &result, rscale, true);

  res = make_result_opt_error(&result, have_error);

  free_var(&result);

  return res;
}

   
                         
   
                                                                        
   
Datum
numeric_div_trunc(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  NumericVar arg1;
  NumericVar arg2;
  NumericVar result;
  Numeric res;

     
                
     
  if (NUMERIC_IS_NAN(num1) || NUMERIC_IS_NAN(num2))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

     
                          
     
  init_var_from_num(num1, &arg1);
  init_var_from_num(num2, &arg2);

  init_var(&result);

     
                                         
     
  div_var(&arg1, &arg2, &result, 0, false);

  res = make_result(&result);

  free_var(&result);

  PG_RETURN_NUMERIC(res);
}

   
                   
   
                                        
   
Datum
numeric_mod(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  Numeric res;

  res = numeric_mod_opt_error(num1, num2, NULL);

  PG_RETURN_NUMERIC(res);
}

   
                             
   
                                                                          
                                                                          
                                    
   
Numeric
numeric_mod_opt_error(Numeric num1, Numeric num2, bool *have_error)
{
  Numeric res;
  NumericVar arg1;
  NumericVar arg2;
  NumericVar result;

  if (NUMERIC_IS_NAN(num1) || NUMERIC_IS_NAN(num2))
  {
    return make_result(&const_nan);
  }

  init_var_from_num(num1, &arg1);
  init_var_from_num(num2, &arg2);

  init_var(&result);

     
                                                                  
     
  if (have_error && (arg2.ndigits == 0 || arg2.digits[0] == 0))
  {
    *have_error = true;
    return NULL;
  }

  mod_var(&arg1, &arg2, &result);

  res = make_result_opt_error(&result, NULL);

  free_var(&result);

  return res;
}

   
                   
   
                             
   
Datum
numeric_inc(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  NumericVar arg;
  Numeric res;

     
                
     
  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

     
                                      
     
  init_var_from_num(num, &arg);

  add_var(&arg, &const_one, &arg);

  res = make_result(&arg);

  free_var(&arg);

  PG_RETURN_NUMERIC(res);
}

   
                       
   
                                     
   
Datum
numeric_smaller(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);

     
                                                                             
                                                        
     
  if (cmp_numerics(num1, num2) < 0)
  {
    PG_RETURN_NUMERIC(num1);
  }
  else
  {
    PG_RETURN_NUMERIC(num2);
  }
}

   
                      
   
                                    
   
Datum
numeric_larger(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);

     
                                                                             
                                                        
     
  if (cmp_numerics(num1, num2) > 0)
  {
    PG_RETURN_NUMERIC(num1);
  }
  else
  {
    PG_RETURN_NUMERIC(num2);
  }
}

                                                                          
   
                           
   
                                                                          
   

   
                 
   
                     
   
Datum
numeric_fac(PG_FUNCTION_ARGS)
{
  int64 num = PG_GETARG_INT64(0);
  Numeric res;
  NumericVar fact;
  NumericVar result;

  if (num <= 1)
  {
    res = make_result(&const_one);
    PG_RETURN_NUMERIC(res);
  }
                                                     
  if (num > 32177)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value overflows numeric format")));
  }

  init_var(&fact);
  init_var(&result);

  int64_to_numericvar(num, &result);

  for (num = num - 1; num > 1; num--)
  {
                                                                  
    CHECK_FOR_INTERRUPTS();

    int64_to_numericvar(num, &fact);

    mul_var(&result, &fact, &result, 0);
  }

  res = make_result(&result);

  free_var(&fact);
  free_var(&result);

  PG_RETURN_NUMERIC(res);
}

   
                    
   
                                         
   
Datum
numeric_sqrt(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  Numeric res;
  NumericVar arg;
  NumericVar result;
  int sweight;
  int rscale;

     
                
     
  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

     
                                                                            
                                                                            
                                            
     
  init_var_from_num(num, &arg);

  init_var(&result);

                                                                  
  sweight = (arg.weight + 1) * DEC_DIGITS / 2 - 1;

  rscale = NUMERIC_MIN_SIG_DIGITS - sweight;
  rscale = Max(rscale, arg.dscale);
  rscale = Max(rscale, NUMERIC_MIN_DISPLAY_SCALE);
  rscale = Min(rscale, NUMERIC_MAX_DISPLAY_SCALE);

     
                                                              
     
  sqrt_var(&arg, &result, rscale);

  res = make_result(&result);

  free_var(&result);

  PG_RETURN_NUMERIC(res);
}

   
                   
   
                             
   
Datum
numeric_exp(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  Numeric res;
  NumericVar arg;
  NumericVar result;
  int rscale;
  double val;

     
                
     
  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

     
                                                                            
                                                                            
                                            
     
  init_var_from_num(num, &arg);

  init_var(&result);

                                                  
  val = numericvar_to_double_no_overflow(&arg);

     
                                                                          
                           
     
  val *= 0.434294481903252;

                                                            
  val = Max(val, -NUMERIC_MAX_RESULT_SCALE);
  val = Min(val, NUMERIC_MAX_RESULT_SCALE);

  rscale = NUMERIC_MIN_SIG_DIGITS - (int)val;
  rscale = Max(rscale, arg.dscale);
  rscale = Max(rscale, NUMERIC_MIN_DISPLAY_SCALE);
  rscale = Min(rscale, NUMERIC_MAX_DISPLAY_SCALE);

     
                                                             
     
  exp_var(&arg, &result, rscale);

  res = make_result(&result);

  free_var(&result);

  PG_RETURN_NUMERIC(res);
}

   
                  
   
                                      
   
Datum
numeric_ln(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  Numeric res;
  NumericVar arg;
  NumericVar result;
  int ln_dweight;
  int rscale;

     
                
     
  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

  init_var_from_num(num, &arg);
  init_var(&result);

                                      
  ln_dweight = estimate_ln_dweight(&arg);

  rscale = NUMERIC_MIN_SIG_DIGITS - ln_dweight;
  rscale = Max(rscale, arg.dscale);
  rscale = Max(rscale, NUMERIC_MIN_DISPLAY_SCALE);
  rscale = Min(rscale, NUMERIC_MAX_DISPLAY_SCALE);

  ln_var(&arg, &result, rscale);

  res = make_result(&result);

  free_var(&result);

  PG_RETURN_NUMERIC(res);
}

   
                   
   
                                              
   
Datum
numeric_log(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  Numeric res;
  NumericVar arg1;
  NumericVar arg2;
  NumericVar result;

     
                
     
  if (NUMERIC_IS_NAN(num1) || NUMERIC_IS_NAN(num2))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

     
                       
     
  init_var_from_num(num1, &arg1);
  init_var_from_num(num2, &arg2);
  init_var(&result);

     
                                                                            
                       
     
  log_var(&arg1, &arg2, &result);

  res = make_result(&result);

  free_var(&result);

  PG_RETURN_NUMERIC(res);
}

   
                     
   
                             
   
Datum
numeric_power(PG_FUNCTION_ARGS)
{
  Numeric num1 = PG_GETARG_NUMERIC(0);
  Numeric num2 = PG_GETARG_NUMERIC(1);
  Numeric res;
  NumericVar arg1;
  NumericVar arg2;
  NumericVar arg2_trunc;
  NumericVar result;

     
                                                                             
                                                                         
                                
     
  if (NUMERIC_IS_NAN(num1))
  {
    if (!NUMERIC_IS_NAN(num2))
    {
      init_var_from_num(num2, &arg2);
      if (cmp_var(&arg2, &const_zero) == 0)
      {
        PG_RETURN_NUMERIC(make_result(&const_one));
      }
    }
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }
  if (NUMERIC_IS_NAN(num2))
  {
    init_var_from_num(num1, &arg1);
    if (cmp_var(&arg1, &const_one) == 0)
    {
      PG_RETURN_NUMERIC(make_result(&const_one));
    }
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

     
                       
     
  init_var(&arg2_trunc);
  init_var(&result);
  init_var_from_num(num1, &arg1);
  init_var_from_num(num2, &arg2);

  set_var_from_var(&arg2, &arg2_trunc);
  trunc_var(&arg2_trunc, 0);

     
                                                                             
                                                                
                                                                           
                                                                          
                             
     
  if (cmp_var(&arg1, &const_zero) == 0 && cmp_var(&arg2, &const_zero) < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_POWER_FUNCTION), errmsg("zero raised to a negative power is undefined")));
  }

     
                                                                        
                             
     
  power_var(&arg1, &arg2, &result);

  res = make_result(&result);

  free_var(&result);
  free_var(&arg2_trunc);

  PG_RETURN_NUMERIC(res);
}

   
                     
   
                                                                              
   
Datum
numeric_scale(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);

  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_INT32(NUMERIC_DSCALE(num));
}

                                                                          
   
                             
   
                                                                          
   

Datum
int4_numeric(PG_FUNCTION_ARGS)
{
  int32 val = PG_GETARG_INT32(0);
  Numeric res;
  NumericVar result;

  init_var(&result);

  int64_to_numericvar((int64)val, &result);

  res = make_result(&result);

  free_var(&result);

  PG_RETURN_NUMERIC(res);
}

int32
numeric_int4_opt_error(Numeric num, bool *have_error)
{
  NumericVar x;
  int32 result;

                                              
  if (NUMERIC_IS_NAN(num))
  {
    if (have_error)
    {
      *have_error = true;
      return 0;
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot convert NaN to integer")));
    }
  }

                                                        
  init_var_from_num(num, &x);

  if (!numericvar_to_int32(&x, &result))
  {
    if (have_error)
    {
      *have_error = true;
      return 0;
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("integer out of range")));
    }
  }

  return result;
}

Datum
numeric_int4(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);

  PG_RETURN_INT32(numeric_int4_opt_error(num, NULL));
}

   
                                                                 
                                                                                 
                                         
   
static bool
numericvar_to_int32(const NumericVar *var, int32 *result)
{
  int64 val;

  if (!numericvar_to_int64(var, &val))
  {
    return false;
  }

  if (unlikely(val < PG_INT32_MIN) || unlikely(val > PG_INT32_MAX))
  {
    return false;
  }

                            
  *result = (int32)val;

  return true;
}

Datum
int8_numeric(PG_FUNCTION_ARGS)
{
  int64 val = PG_GETARG_INT64(0);
  Numeric res;
  NumericVar result;

  init_var(&result);

  int64_to_numericvar(val, &result);

  res = make_result(&result);

  free_var(&result);

  PG_RETURN_NUMERIC(res);
}

Datum
numeric_int8(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  NumericVar x;
  int64 result;

                                              
  if (NUMERIC_IS_NAN(num))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot convert NaN to bigint")));
  }

                                                     
  init_var_from_num(num, &x);

  if (!numericvar_to_int64(&x, &result))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("bigint out of range")));
  }

  PG_RETURN_INT64(result);
}

Datum
int2_numeric(PG_FUNCTION_ARGS)
{
  int16 val = PG_GETARG_INT16(0);
  Numeric res;
  NumericVar result;

  init_var(&result);

  int64_to_numericvar((int64)val, &result);

  res = make_result(&result);

  free_var(&result);

  PG_RETURN_NUMERIC(res);
}

Datum
numeric_int2(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  NumericVar x;
  int64 val;
  int16 result;

                                              
  if (NUMERIC_IS_NAN(num))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot convert NaN to smallint")));
  }

                                                     
  init_var_from_num(num, &x);

  if (!numericvar_to_int64(&x, &val))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("smallint out of range")));
  }

  if (unlikely(val < PG_INT16_MIN) || unlikely(val > PG_INT16_MAX))
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("smallint out of range")));
  }

                            
  result = (int16)val;

  PG_RETURN_INT16(result);
}

Datum
float8_numeric(PG_FUNCTION_ARGS)
{
  float8 val = PG_GETARG_FLOAT8(0);
  Numeric res;
  NumericVar result;
  char buf[DBL_DIG + 100];

  if (isnan(val))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

  if (isinf(val))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot convert infinity to numeric")));
  }

  snprintf(buf, sizeof(buf), "%.*g", DBL_DIG, val);

  init_var(&result);

                                                              
  (void)set_var_from_str(buf, buf, &result);

  res = make_result(&result);

  free_var(&result);

  PG_RETURN_NUMERIC(res);
}

Datum
numeric_float8(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  char *tmp;
  Datum result;

  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

  tmp = DatumGetCString(DirectFunctionCall1(numeric_out, NumericGetDatum(num)));

  result = DirectFunctionCall1(float8in, CStringGetDatum(tmp));

  pfree(tmp);

  PG_RETURN_DATUM(result);
}

   
                                                                   
   
                                                              
   
Datum
numeric_float8_no_overflow(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  double val;

  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_FLOAT8(get_float8_nan());
  }

  val = numeric_to_double_no_overflow(num);

  PG_RETURN_FLOAT8(val);
}

Datum
float4_numeric(PG_FUNCTION_ARGS)
{
  float4 val = PG_GETARG_FLOAT4(0);
  Numeric res;
  NumericVar result;
  char buf[FLT_DIG + 100];

  if (isnan(val))
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

  if (isinf(val))
  {
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("cannot convert infinity to numeric")));
  }

  snprintf(buf, sizeof(buf), "%.*g", FLT_DIG, val);

  init_var(&result);

                                                              
  (void)set_var_from_str(buf, buf, &result);

  res = make_result(&result);

  free_var(&result);

  PG_RETURN_NUMERIC(res);
}

Datum
numeric_float4(PG_FUNCTION_ARGS)
{
  Numeric num = PG_GETARG_NUMERIC(0);
  char *tmp;
  Datum result;

  if (NUMERIC_IS_NAN(num))
  {
    PG_RETURN_FLOAT4(get_float4_nan());
  }

  tmp = DatumGetCString(DirectFunctionCall1(numeric_out, NumericGetDatum(num)));

  result = DirectFunctionCall1(float4in, CStringGetDatum(tmp));

  pfree(tmp);

  PG_RETURN_DATUM(result);
}

                                                                          
   
                       
   
                                                                             
                                                                            
                                                                      
   
                                                                             
                                                                       
   
                                                                          
   

typedef struct NumericAggState
{
  bool calcSumX2;                                          
  MemoryContext agg_context;                                   
  int64 N;                                                   
  NumericSumAccum sumX;                                    
  NumericSumAccum sumX2;                                              
  int maxScale;                                             
  int64 maxScaleCount;                                                     
  int64 NaNcount;                                                          
} NumericAggState;

   
                                                                             
                                                          
   
static NumericAggState *
makeNumericAggState(FunctionCallInfo fcinfo, bool calcSumX2)
{
  NumericAggState *state;
  MemoryContext agg_context;
  MemoryContext old_context;

  if (!AggCheckCallContext(fcinfo, &agg_context))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  old_context = MemoryContextSwitchTo(agg_context);

  state = (NumericAggState *)palloc0(sizeof(NumericAggState));
  state->calcSumX2 = calcSumX2;
  state->agg_context = agg_context;

  MemoryContextSwitchTo(old_context);

  return state;
}

   
                                                                            
            
   
static NumericAggState *
makeNumericAggStateCurrentContext(bool calcSumX2)
{
  NumericAggState *state;

  state = (NumericAggState *)palloc0(sizeof(NumericAggState));
  state->calcSumX2 = calcSumX2;
  state->agg_context = CurrentMemoryContext;

  return state;
}

   
                                                                 
   
static void
do_numeric_accum(NumericAggState *state, Numeric newval)
{
  NumericVar X;
  NumericVar X2;
  MemoryContext old_context;

                                                 
  if (NUMERIC_IS_NAN(newval))
  {
    state->NaNcount++;
    return;
  }

                                                    
  init_var_from_num(newval, &X);

     
                                                                        
                                           
     
  if (X.dscale > state->maxScale)
  {
    state->maxScale = X.dscale;
    state->maxScaleCount = 1;
  }
  else if (X.dscale == state->maxScale)
  {
    state->maxScaleCount++;
  }

                                                             
  if (state->calcSumX2)
  {
    init_var(&X2);
    mul_var(&X, &X, &X2, X.dscale * 2);
  }

                                                               
  old_context = MemoryContextSwitchTo(state->agg_context);

  state->N++;

                       
  accum_sum_add(&(state->sumX), &X);

  if (state->calcSumX2)
  {
    accum_sum_add(&(state->sumX2), &X2);
  }

  MemoryContextSwitchTo(old_context);
}

   
                                                               
   
                                                                           
                                                     
   
                                                                       
                                                                         
                                                                           
                                                                           
                                                    
   
                                                                               
                                                                             
   
static bool
do_numeric_discard(NumericAggState *state, Numeric newval)
{
  NumericVar X;
  NumericVar X2;
  MemoryContext old_context;

                                                 
  if (NUMERIC_IS_NAN(newval))
  {
    state->NaNcount--;
    return true;
  }

                                                    
  init_var_from_num(newval, &X);

     
                                                                      
                                                                            
                                                                             
                                                                            
                                                          
     
  if (X.dscale == state->maxScale)
  {
    if (state->maxScaleCount > 1 || state->maxScale == 0)
    {
         
                                                                         
                           
         
      state->maxScaleCount--;
    }
    else if (state->N == 1)
    {
                                                                 
      state->maxScale = 0;
      state->maxScaleCount = 0;
    }
    else
    {
                                                        
      return false;
    }
  }

                                                             
  if (state->calcSumX2)
  {
    init_var(&X2);
    mul_var(&X, &X, &X2, X.dscale * 2);
  }

                                                               
  old_context = MemoryContextSwitchTo(state->agg_context);

  if (state->N-- > 1)
  {
                                               
    X.sign = (X.sign == NUMERIC_POS ? NUMERIC_NEG : NUMERIC_POS);
    accum_sum_add(&(state->sumX), &X);

    if (state->calcSumX2)
    {
                                              
      X2.sign = NUMERIC_NEG;
      accum_sum_add(&(state->sumX2), &X2);
    }
  }
  else
  {
                       
    Assert(state->N == 0);

    accum_sum_reset(&state->sumX);
    if (state->calcSumX2)
    {
      accum_sum_reset(&state->sumX2);
    }
  }

  MemoryContextSwitchTo(old_context);

  return true;
}

   
                                                                          
   
Datum
numeric_accum(PG_FUNCTION_ARGS)
{
  NumericAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (NumericAggState *)PG_GETARG_POINTER(0);

                                               
  if (state == NULL)
  {
    state = makeNumericAggState(fcinfo, true);
  }

  if (!PG_ARGISNULL(1))
  {
    do_numeric_accum(state, PG_GETARG_NUMERIC(1));
  }

  PG_RETURN_POINTER(state);
}

   
                                                                       
   
Datum
numeric_combine(PG_FUNCTION_ARGS)
{
  NumericAggState *state1;
  NumericAggState *state2;
  MemoryContext agg_context;
  MemoryContext old_context;

  if (!AggCheckCallContext(fcinfo, &agg_context))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  state1 = PG_ARGISNULL(0) ? NULL : (NumericAggState *)PG_GETARG_POINTER(0);
  state2 = PG_ARGISNULL(1) ? NULL : (NumericAggState *)PG_GETARG_POINTER(1);

  if (state2 == NULL)
  {
    PG_RETURN_POINTER(state1);
  }

                                                      
  if (state1 == NULL)
  {
    old_context = MemoryContextSwitchTo(agg_context);

    state1 = makeNumericAggStateCurrentContext(true);
    state1->N = state2->N;
    state1->NaNcount = state2->NaNcount;
    state1->maxScale = state2->maxScale;
    state1->maxScaleCount = state2->maxScaleCount;

    accum_sum_copy(&state1->sumX, &state2->sumX);
    accum_sum_copy(&state1->sumX2, &state2->sumX2);

    MemoryContextSwitchTo(old_context);

    PG_RETURN_POINTER(state1);
  }

  state1->N += state2->N;
  state1->NaNcount += state2->NaNcount;

  if (state2->N > 0)
  {
       
                                                                           
                                 
       
    if (state2->maxScale > state1->maxScale)
    {
      state1->maxScale = state2->maxScale;
      state1->maxScaleCount = state2->maxScaleCount;
    }
    else if (state2->maxScale == state1->maxScale)
    {
      state1->maxScaleCount += state2->maxScaleCount;
    }

                                                                 
    old_context = MemoryContextSwitchTo(agg_context);

                         
    accum_sum_combine(&state1->sumX, &state2->sumX);
    accum_sum_combine(&state1->sumX2, &state2->sumX2);

    MemoryContextSwitchTo(old_context);
  }
  PG_RETURN_POINTER(state1);
}

   
                                                                                
   
Datum
numeric_avg_accum(PG_FUNCTION_ARGS)
{
  NumericAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (NumericAggState *)PG_GETARG_POINTER(0);

                                               
  if (state == NULL)
  {
    state = makeNumericAggState(fcinfo, false);
  }

  if (!PG_ARGISNULL(1))
  {
    do_numeric_accum(state, PG_GETARG_NUMERIC(1));
  }

  PG_RETURN_POINTER(state);
}

   
                                                                     
   
Datum
numeric_avg_combine(PG_FUNCTION_ARGS)
{
  NumericAggState *state1;
  NumericAggState *state2;
  MemoryContext agg_context;
  MemoryContext old_context;

  if (!AggCheckCallContext(fcinfo, &agg_context))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  state1 = PG_ARGISNULL(0) ? NULL : (NumericAggState *)PG_GETARG_POINTER(0);
  state2 = PG_ARGISNULL(1) ? NULL : (NumericAggState *)PG_GETARG_POINTER(1);

  if (state2 == NULL)
  {
    PG_RETURN_POINTER(state1);
  }

                                                      
  if (state1 == NULL)
  {
    old_context = MemoryContextSwitchTo(agg_context);

    state1 = makeNumericAggStateCurrentContext(false);
    state1->N = state2->N;
    state1->NaNcount = state2->NaNcount;
    state1->maxScale = state2->maxScale;
    state1->maxScaleCount = state2->maxScaleCount;

    accum_sum_copy(&state1->sumX, &state2->sumX);

    MemoryContextSwitchTo(old_context);

    PG_RETURN_POINTER(state1);
  }

  state1->N += state2->N;
  state1->NaNcount += state2->NaNcount;

  if (state2->N > 0)
  {
       
                                                                           
                                 
       
    if (state2->maxScale > state1->maxScale)
    {
      state1->maxScale = state2->maxScale;
      state1->maxScaleCount = state2->maxScaleCount;
    }
    else if (state2->maxScale == state1->maxScale)
    {
      state1->maxScaleCount += state2->maxScaleCount;
    }

                                                                 
    old_context = MemoryContextSwitchTo(agg_context);

                         
    accum_sum_combine(&state1->sumX, &state2->sumX);

    MemoryContextSwitchTo(old_context);
  }
  PG_RETURN_POINTER(state1);
}

   
                         
                                                                        
           
   
Datum
numeric_avg_serialize(PG_FUNCTION_ARGS)
{
  NumericAggState *state;
  StringInfoData buf;
  Datum temp;
  bytea *sumX;
  bytea *result;
  NumericVar tmp_var;

                                                                
  if (!AggCheckCallContext(fcinfo, NULL))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  state = (NumericAggState *)PG_GETARG_POINTER(0);

     
                                                                         
                                                                         
                                                                         
                                                              
     
  init_var(&tmp_var);
  accum_sum_final(&state->sumX, &tmp_var);

  temp = DirectFunctionCall1(numeric_send, NumericGetDatum(make_result(&tmp_var)));
  sumX = DatumGetByteaPP(temp);
  free_var(&tmp_var);

  pq_begintypsend(&buf);

         
  pq_sendint64(&buf, state->N);

            
  pq_sendbytes(&buf, VARDATA_ANY(sumX), VARSIZE_ANY_EXHDR(sumX));

                
  pq_sendint32(&buf, state->maxScale);

                     
  pq_sendint64(&buf, state->maxScaleCount);

                
  pq_sendint64(&buf, state->NaNcount);

  result = pq_endtypsend(&buf);

  PG_RETURN_BYTEA_P(result);
}

   
                           
                                                                       
                         
   
Datum
numeric_avg_deserialize(PG_FUNCTION_ARGS)
{
  bytea *sstate;
  NumericAggState *result;
  Datum temp;
  NumericVar tmp_var;
  StringInfoData buf;

  if (!AggCheckCallContext(fcinfo, NULL))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  sstate = PG_GETARG_BYTEA_PP(0);

     
                                                                            
                                            
     
  initStringInfo(&buf);
  appendBinaryStringInfo(&buf, VARDATA_ANY(sstate), VARSIZE_ANY_EXHDR(sstate));

  result = makeNumericAggStateCurrentContext(false);

         
  result->N = pq_getmsgint64(&buf);

            
  temp = DirectFunctionCall3(numeric_recv, PointerGetDatum(&buf), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1));
  init_var_from_num(DatumGetNumeric(temp), &tmp_var);
  accum_sum_add(&(result->sumX), &tmp_var);

                
  result->maxScale = pq_getmsgint(&buf, 4);

                     
  result->maxScaleCount = pq_getmsgint64(&buf);

                
  result->NaNcount = pq_getmsgint64(&buf);

  pq_getmsgend(&buf);
  pfree(buf.data);

  PG_RETURN_POINTER(result);
}

   
                     
                                                                           
                   
   
Datum
numeric_serialize(PG_FUNCTION_ARGS)
{
  NumericAggState *state;
  StringInfoData buf;
  Datum temp;
  bytea *sumX;
  NumericVar tmp_var;
  bytea *sumX2;
  bytea *result;

                                                                
  if (!AggCheckCallContext(fcinfo, NULL))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  state = (NumericAggState *)PG_GETARG_POINTER(0);

     
                                                                         
                                                                         
                                                                         
                                                              
     
  init_var(&tmp_var);

  accum_sum_final(&state->sumX, &tmp_var);
  temp = DirectFunctionCall1(numeric_send, NumericGetDatum(make_result(&tmp_var)));
  sumX = DatumGetByteaPP(temp);

  accum_sum_final(&state->sumX2, &tmp_var);
  temp = DirectFunctionCall1(numeric_send, NumericGetDatum(make_result(&tmp_var)));
  sumX2 = DatumGetByteaPP(temp);

  free_var(&tmp_var);

  pq_begintypsend(&buf);

         
  pq_sendint64(&buf, state->N);

            
  pq_sendbytes(&buf, VARDATA_ANY(sumX), VARSIZE_ANY_EXHDR(sumX));

             
  pq_sendbytes(&buf, VARDATA_ANY(sumX2), VARSIZE_ANY_EXHDR(sumX2));

                
  pq_sendint32(&buf, state->maxScale);

                     
  pq_sendint64(&buf, state->maxScaleCount);

                
  pq_sendint64(&buf, state->NaNcount);

  result = pq_endtypsend(&buf);

  PG_RETURN_BYTEA_P(result);
}

   
                       
                                                                             
                   
   
Datum
numeric_deserialize(PG_FUNCTION_ARGS)
{
  bytea *sstate;
  NumericAggState *result;
  Datum temp;
  NumericVar sumX_var;
  NumericVar sumX2_var;
  StringInfoData buf;

  if (!AggCheckCallContext(fcinfo, NULL))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  sstate = PG_GETARG_BYTEA_PP(0);

     
                                                                            
                                            
     
  initStringInfo(&buf);
  appendBinaryStringInfo(&buf, VARDATA_ANY(sstate), VARSIZE_ANY_EXHDR(sstate));

  result = makeNumericAggStateCurrentContext(false);

         
  result->N = pq_getmsgint64(&buf);

            
  temp = DirectFunctionCall3(numeric_recv, PointerGetDatum(&buf), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1));
  init_var_from_num(DatumGetNumeric(temp), &sumX_var);
  accum_sum_add(&(result->sumX), &sumX_var);

             
  temp = DirectFunctionCall3(numeric_recv, PointerGetDatum(&buf), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1));
  init_var_from_num(DatumGetNumeric(temp), &sumX2_var);
  accum_sum_add(&(result->sumX2), &sumX2_var);

                
  result->maxScale = pq_getmsgint(&buf, 4);

                     
  result->maxScaleCount = pq_getmsgint64(&buf);

                
  result->NaNcount = pq_getmsgint64(&buf);

  pq_getmsgend(&buf);
  pfree(buf.data);

  PG_RETURN_POINTER(result);
}

   
                                                              
                                          
   
Datum
numeric_accum_inv(PG_FUNCTION_ARGS)
{
  NumericAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (NumericAggState *)PG_GETARG_POINTER(0);

                                         
  if (state == NULL)
  {
    elog(ERROR, "numeric_accum_inv called with NULL state");
  }

  if (!PG_ARGISNULL(1))
  {
                                                                   
    if (!do_numeric_discard(state, PG_GETARG_NUMERIC(1)))
    {
      PG_RETURN_NULL();
    }
  }

  PG_RETURN_POINTER(state);
}

   
                                                                        
                               
   
                                                                         
                                    
   
                                                                           
                                                  
   
                                                                            
                                                                  
   

#ifdef HAVE_INT128
typedef struct Int128AggState
{
  bool calcSumX2;                               
  int64 N;                                        
  int128 sumX;                                  
  int128 sumX2;                                            
} Int128AggState;

   
                                                                             
                                                          
   
static Int128AggState *
makeInt128AggState(FunctionCallInfo fcinfo, bool calcSumX2)
{
  Int128AggState *state;
  MemoryContext agg_context;
  MemoryContext old_context;

  if (!AggCheckCallContext(fcinfo, &agg_context))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  old_context = MemoryContextSwitchTo(agg_context);

  state = (Int128AggState *)palloc0(sizeof(Int128AggState));
  state->calcSumX2 = calcSumX2;

  MemoryContextSwitchTo(old_context);

  return state;
}

   
                                                                           
            
   
static Int128AggState *
makeInt128AggStateCurrentContext(bool calcSumX2)
{
  Int128AggState *state;

  state = (Int128AggState *)palloc0(sizeof(Int128AggState));
  state->calcSumX2 = calcSumX2;

  return state;
}

   
                                                                 
   
static void
do_int128_accum(Int128AggState *state, int128 newval)
{
  if (state->calcSumX2)
  {
    state->sumX2 += newval * newval;
  }

  state->sumX += newval;
  state->N++;
}

   
                                                    
   
static void
do_int128_discard(Int128AggState *state, int128 newval)
{
  if (state->calcSumX2)
  {
    state->sumX2 -= newval * newval;
  }

  state->sumX -= newval;
  state->N--;
}

typedef Int128AggState PolyNumAggState;
#define makePolyNumAggState makeInt128AggState
#define makePolyNumAggStateCurrentContext makeInt128AggStateCurrentContext
#else
typedef NumericAggState PolyNumAggState;
#define makePolyNumAggState makeNumericAggState
#define makePolyNumAggStateCurrentContext makeNumericAggStateCurrentContext
#endif

Datum
int2_accum(PG_FUNCTION_ARGS)
{
  PolyNumAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(0);

                                               
  if (state == NULL)
  {
    state = makePolyNumAggState(fcinfo, true);
  }

  if (!PG_ARGISNULL(1))
  {
#ifdef HAVE_INT128
    do_int128_accum(state, (int128)PG_GETARG_INT16(1));
#else
    Numeric newval;

    newval = DatumGetNumeric(DirectFunctionCall1(int2_numeric, PG_GETARG_DATUM(1)));
    do_numeric_accum(state, newval);
#endif
  }

  PG_RETURN_POINTER(state);
}

Datum
int4_accum(PG_FUNCTION_ARGS)
{
  PolyNumAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(0);

                                               
  if (state == NULL)
  {
    state = makePolyNumAggState(fcinfo, true);
  }

  if (!PG_ARGISNULL(1))
  {
#ifdef HAVE_INT128
    do_int128_accum(state, (int128)PG_GETARG_INT32(1));
#else
    Numeric newval;

    newval = DatumGetNumeric(DirectFunctionCall1(int4_numeric, PG_GETARG_DATUM(1)));
    do_numeric_accum(state, newval);
#endif
  }

  PG_RETURN_POINTER(state);
}

Datum
int8_accum(PG_FUNCTION_ARGS)
{
  NumericAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (NumericAggState *)PG_GETARG_POINTER(0);

                                               
  if (state == NULL)
  {
    state = makeNumericAggState(fcinfo, true);
  }

  if (!PG_ARGISNULL(1))
  {
    Numeric newval;

    newval = DatumGetNumeric(DirectFunctionCall1(int8_numeric, PG_GETARG_DATUM(1)));
    do_numeric_accum(state, newval);
  }

  PG_RETURN_POINTER(state);
}

   
                                                               
   
Datum
numeric_poly_combine(PG_FUNCTION_ARGS)
{
  PolyNumAggState *state1;
  PolyNumAggState *state2;
  MemoryContext agg_context;
  MemoryContext old_context;

  if (!AggCheckCallContext(fcinfo, &agg_context))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  state1 = PG_ARGISNULL(0) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(0);
  state2 = PG_ARGISNULL(1) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(1);

  if (state2 == NULL)
  {
    PG_RETURN_POINTER(state1);
  }

                                                      
  if (state1 == NULL)
  {
    old_context = MemoryContextSwitchTo(agg_context);

    state1 = makePolyNumAggState(fcinfo, true);
    state1->N = state2->N;

#ifdef HAVE_INT128
    state1->sumX = state2->sumX;
    state1->sumX2 = state2->sumX2;
#else
    accum_sum_copy(&state1->sumX, &state2->sumX);
    accum_sum_copy(&state1->sumX2, &state2->sumX2);
#endif

    MemoryContextSwitchTo(old_context);

    PG_RETURN_POINTER(state1);
  }

  if (state2->N > 0)
  {
    state1->N += state2->N;

#ifdef HAVE_INT128
    state1->sumX += state2->sumX;
    state1->sumX2 += state2->sumX2;
#else
                                                                 
    old_context = MemoryContextSwitchTo(agg_context);

                         
    accum_sum_combine(&state1->sumX, &state2->sumX);
    accum_sum_combine(&state1->sumX2, &state2->sumX2);

    MemoryContextSwitchTo(old_context);
#endif
  }
  PG_RETURN_POINTER(state1);
}

   
                          
                                                                       
                   
   
Datum
numeric_poly_serialize(PG_FUNCTION_ARGS)
{
  PolyNumAggState *state;
  StringInfoData buf;
  bytea *sumX;
  bytea *sumX2;
  bytea *result;

                                                                
  if (!AggCheckCallContext(fcinfo, NULL))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  state = (PolyNumAggState *)PG_GETARG_POINTER(0);

     
                                                                           
                                                                           
                                                                          
                                                                            
                                                                        
                                                            
     
  {
    Datum temp;
    NumericVar num;

    init_var(&num);

#ifdef HAVE_INT128
    int128_to_numericvar(state->sumX, &num);
#else
    accum_sum_final(&state->sumX, &num);
#endif
    temp = DirectFunctionCall1(numeric_send, NumericGetDatum(make_result(&num)));
    sumX = DatumGetByteaPP(temp);

#ifdef HAVE_INT128
    int128_to_numericvar(state->sumX2, &num);
#else
    accum_sum_final(&state->sumX2, &num);
#endif
    temp = DirectFunctionCall1(numeric_send, NumericGetDatum(make_result(&num)));
    sumX2 = DatumGetByteaPP(temp);

    free_var(&num);
  }

  pq_begintypsend(&buf);

         
  pq_sendint64(&buf, state->N);

            
  pq_sendbytes(&buf, VARDATA_ANY(sumX), VARSIZE_ANY_EXHDR(sumX));

             
  pq_sendbytes(&buf, VARDATA_ANY(sumX2), VARSIZE_ANY_EXHDR(sumX2));

  result = pq_endtypsend(&buf);

  PG_RETURN_BYTEA_P(result);
}

   
                            
                                                                         
                   
   
Datum
numeric_poly_deserialize(PG_FUNCTION_ARGS)
{
  bytea *sstate;
  PolyNumAggState *result;
  Datum sumX;
  NumericVar sumX_var;
  Datum sumX2;
  NumericVar sumX2_var;
  StringInfoData buf;

  if (!AggCheckCallContext(fcinfo, NULL))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  sstate = PG_GETARG_BYTEA_PP(0);

     
                                                                            
                                            
     
  initStringInfo(&buf);
  appendBinaryStringInfo(&buf, VARDATA_ANY(sstate), VARSIZE_ANY_EXHDR(sstate));

  result = makePolyNumAggStateCurrentContext(false);

         
  result->N = pq_getmsgint64(&buf);

            
  sumX = DirectFunctionCall3(numeric_recv, PointerGetDatum(&buf), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1));

             
  sumX2 = DirectFunctionCall3(numeric_recv, PointerGetDatum(&buf), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1));

  init_var_from_num(DatumGetNumeric(sumX), &sumX_var);
#ifdef HAVE_INT128
  numericvar_to_int128(&sumX_var, &result->sumX);
#else
  accum_sum_add(&result->sumX, &sumX_var);
#endif

  init_var_from_num(DatumGetNumeric(sumX2), &sumX2_var);
#ifdef HAVE_INT128
  numericvar_to_int128(&sumX2_var, &result->sumX2);
#else
  accum_sum_add(&result->sumX2, &sumX2_var);
#endif

  pq_getmsgend(&buf);
  pfree(buf.data);

  PG_RETURN_POINTER(result);
}

   
                                                                
   
Datum
int8_avg_accum(PG_FUNCTION_ARGS)
{
  PolyNumAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(0);

                                               
  if (state == NULL)
  {
    state = makePolyNumAggState(fcinfo, false);
  }

  if (!PG_ARGISNULL(1))
  {
#ifdef HAVE_INT128
    do_int128_accum(state, (int128)PG_GETARG_INT64(1));
#else
    Numeric newval;

    newval = DatumGetNumeric(DirectFunctionCall1(int8_numeric, PG_GETARG_DATUM(1)));
    do_numeric_accum(state, newval);
#endif
  }

  PG_RETURN_POINTER(state);
}

   
                                                                           
         
   
Datum
int8_avg_combine(PG_FUNCTION_ARGS)
{
  PolyNumAggState *state1;
  PolyNumAggState *state2;
  MemoryContext agg_context;
  MemoryContext old_context;

  if (!AggCheckCallContext(fcinfo, &agg_context))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  state1 = PG_ARGISNULL(0) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(0);
  state2 = PG_ARGISNULL(1) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(1);

  if (state2 == NULL)
  {
    PG_RETURN_POINTER(state1);
  }

                                                      
  if (state1 == NULL)
  {
    old_context = MemoryContextSwitchTo(agg_context);

    state1 = makePolyNumAggState(fcinfo, false);
    state1->N = state2->N;

#ifdef HAVE_INT128
    state1->sumX = state2->sumX;
#else
    accum_sum_copy(&state1->sumX, &state2->sumX);
#endif
    MemoryContextSwitchTo(old_context);

    PG_RETURN_POINTER(state1);
  }

  if (state2->N > 0)
  {
    state1->N += state2->N;

#ifdef HAVE_INT128
    state1->sumX += state2->sumX;
#else
                                                                 
    old_context = MemoryContextSwitchTo(agg_context);

                         
    accum_sum_combine(&state1->sumX, &state2->sumX);

    MemoryContextSwitchTo(old_context);
#endif
  }
  PG_RETURN_POINTER(state1);
}

   
                      
                                                            
                                  
   
Datum
int8_avg_serialize(PG_FUNCTION_ARGS)
{
  PolyNumAggState *state;
  StringInfoData buf;
  bytea *sumX;
  bytea *result;

                                                                
  if (!AggCheckCallContext(fcinfo, NULL))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  state = (PolyNumAggState *)PG_GETARG_POINTER(0);

     
                                                                           
                                                                           
                                                                         
                                                                            
                                                                             
                                          
     
  {
    Datum temp;
    NumericVar num;

    init_var(&num);

#ifdef HAVE_INT128
    int128_to_numericvar(state->sumX, &num);
#else
    accum_sum_final(&state->sumX, &num);
#endif
    temp = DirectFunctionCall1(numeric_send, NumericGetDatum(make_result(&num)));
    sumX = DatumGetByteaPP(temp);

    free_var(&num);
  }

  pq_begintypsend(&buf);

         
  pq_sendint64(&buf, state->N);

            
  pq_sendbytes(&buf, VARDATA_ANY(sumX), VARSIZE_ANY_EXHDR(sumX));

  result = pq_endtypsend(&buf);

  PG_RETURN_BYTEA_P(result);
}

   
                        
                                                 
   
Datum
int8_avg_deserialize(PG_FUNCTION_ARGS)
{
  bytea *sstate;
  PolyNumAggState *result;
  StringInfoData buf;
  Datum temp;
  NumericVar num;

  if (!AggCheckCallContext(fcinfo, NULL))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  sstate = PG_GETARG_BYTEA_PP(0);

     
                                                                            
                                            
     
  initStringInfo(&buf);
  appendBinaryStringInfo(&buf, VARDATA_ANY(sstate), VARSIZE_ANY_EXHDR(sstate));

  result = makePolyNumAggStateCurrentContext(false);

         
  result->N = pq_getmsgint64(&buf);

            
  temp = DirectFunctionCall3(numeric_recv, PointerGetDatum(&buf), ObjectIdGetDatum(InvalidOid), Int32GetDatum(-1));
  init_var_from_num(DatumGetNumeric(temp), &num);
#ifdef HAVE_INT128
  numericvar_to_int128(&num, &result->sumX);
#else
  accum_sum_add(&result->sumX, &num);
#endif

  pq_getmsgend(&buf);
  pfree(buf.data);

  PG_RETURN_POINTER(result);
}

   
                                                      
   

Datum
int2_accum_inv(PG_FUNCTION_ARGS)
{
  PolyNumAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(0);

                                         
  if (state == NULL)
  {
    elog(ERROR, "int2_accum_inv called with NULL state");
  }

  if (!PG_ARGISNULL(1))
  {
#ifdef HAVE_INT128
    do_int128_discard(state, (int128)PG_GETARG_INT16(1));
#else
    Numeric newval;

    newval = DatumGetNumeric(DirectFunctionCall1(int2_numeric, PG_GETARG_DATUM(1)));

                                                     
    if (!do_numeric_discard(state, newval))
    {
      elog(ERROR, "do_numeric_discard failed unexpectedly");
    }
#endif
  }

  PG_RETURN_POINTER(state);
}

Datum
int4_accum_inv(PG_FUNCTION_ARGS)
{
  PolyNumAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(0);

                                         
  if (state == NULL)
  {
    elog(ERROR, "int4_accum_inv called with NULL state");
  }

  if (!PG_ARGISNULL(1))
  {
#ifdef HAVE_INT128
    do_int128_discard(state, (int128)PG_GETARG_INT32(1));
#else
    Numeric newval;

    newval = DatumGetNumeric(DirectFunctionCall1(int4_numeric, PG_GETARG_DATUM(1)));

                                                     
    if (!do_numeric_discard(state, newval))
    {
      elog(ERROR, "do_numeric_discard failed unexpectedly");
    }
#endif
  }

  PG_RETURN_POINTER(state);
}

Datum
int8_accum_inv(PG_FUNCTION_ARGS)
{
  NumericAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (NumericAggState *)PG_GETARG_POINTER(0);

                                         
  if (state == NULL)
  {
    elog(ERROR, "int8_accum_inv called with NULL state");
  }

  if (!PG_ARGISNULL(1))
  {
    Numeric newval;

    newval = DatumGetNumeric(DirectFunctionCall1(int8_numeric, PG_GETARG_DATUM(1)));

                                                     
    if (!do_numeric_discard(state, newval))
    {
      elog(ERROR, "do_numeric_discard failed unexpectedly");
    }
  }

  PG_RETURN_POINTER(state);
}

Datum
int8_avg_accum_inv(PG_FUNCTION_ARGS)
{
  PolyNumAggState *state;

  state = PG_ARGISNULL(0) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(0);

                                         
  if (state == NULL)
  {
    elog(ERROR, "int8_avg_accum_inv called with NULL state");
  }

  if (!PG_ARGISNULL(1))
  {
#ifdef HAVE_INT128
    do_int128_discard(state, (int128)PG_GETARG_INT64(1));
#else
    Numeric newval;

    newval = DatumGetNumeric(DirectFunctionCall1(int8_numeric, PG_GETARG_DATUM(1)));

                                                     
    if (!do_numeric_discard(state, newval))
    {
      elog(ERROR, "do_numeric_discard failed unexpectedly");
    }
#endif
  }

  PG_RETURN_POINTER(state);
}

Datum
numeric_poly_sum(PG_FUNCTION_ARGS)
{
#ifdef HAVE_INT128
  PolyNumAggState *state;
  Numeric res;
  NumericVar result;

  state = PG_ARGISNULL(0) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(0);

                                                     
  if (state == NULL || state->N == 0)
  {
    PG_RETURN_NULL();
  }

  init_var(&result);

  int128_to_numericvar(state->sumX, &result);

  res = make_result(&result);

  free_var(&result);

  PG_RETURN_NUMERIC(res);
#else
  return numeric_sum(fcinfo);
#endif
}

Datum
numeric_poly_avg(PG_FUNCTION_ARGS)
{
#ifdef HAVE_INT128
  PolyNumAggState *state;
  NumericVar result;
  Datum countd, sumd;

  state = PG_ARGISNULL(0) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(0);

                                                     
  if (state == NULL || state->N == 0)
  {
    PG_RETURN_NULL();
  }

  init_var(&result);

  int128_to_numericvar(state->sumX, &result);

  countd = DirectFunctionCall1(int8_numeric, Int64GetDatumFast(state->N));
  sumd = NumericGetDatum(make_result(&result));

  free_var(&result);

  PG_RETURN_DATUM(DirectFunctionCall2(numeric_div, sumd, countd));
#else
  return numeric_avg(fcinfo);
#endif
}

Datum
numeric_avg(PG_FUNCTION_ARGS)
{
  NumericAggState *state;
  Datum N_datum;
  Datum sumX_datum;
  NumericVar sumX_var;

  state = PG_ARGISNULL(0) ? NULL : (NumericAggState *)PG_GETARG_POINTER(0);

                                                     
  if (state == NULL || (state->N + state->NaNcount) == 0)
  {
    PG_RETURN_NULL();
  }

  if (state->NaNcount > 0)                                       
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

  N_datum = DirectFunctionCall1(int8_numeric, Int64GetDatum(state->N));

  init_var(&sumX_var);
  accum_sum_final(&state->sumX, &sumX_var);
  sumX_datum = NumericGetDatum(make_result(&sumX_var));
  free_var(&sumX_var);

  PG_RETURN_DATUM(DirectFunctionCall2(numeric_div, sumX_datum, N_datum));
}

Datum
numeric_sum(PG_FUNCTION_ARGS)
{
  NumericAggState *state;
  NumericVar sumX_var;
  Numeric result;

  state = PG_ARGISNULL(0) ? NULL : (NumericAggState *)PG_GETARG_POINTER(0);

                                                     
  if (state == NULL || (state->N + state->NaNcount) == 0)
  {
    PG_RETURN_NULL();
  }

  if (state->NaNcount > 0)                                       
  {
    PG_RETURN_NUMERIC(make_result(&const_nan));
  }

  init_var(&sumX_var);
  accum_sum_final(&state->sumX, &sumX_var);
  result = make_result(&sumX_var);
  free_var(&sumX_var);

  PG_RETURN_NUMERIC(result);
}

   
                                                            
                                                        
                                                        
                                                                      
                                                        
                    
   
                                                                 
                                                 
   
static Numeric
numeric_stddev_internal(NumericAggState *state, bool variance, bool sample, bool *is_null)
{
  Numeric res;
  NumericVar vN, vsumX, vsumX2, vNminus1;
  const NumericVar *comp;
  int rscale;

                                                 
  if (state == NULL || (state->N + state->NaNcount) == 0)
  {
    *is_null = true;
    return NULL;
  }

  *is_null = false;

  if (state->NaNcount > 0)
  {
    return make_result(&const_nan);
  }

  init_var(&vN);
  init_var(&vsumX);
  init_var(&vsumX2);

  int64_to_numericvar(state->N, &vN);
  accum_sum_final(&(state->sumX), &vsumX);
  accum_sum_final(&(state->sumX2), &vsumX2);

     
                                                                             
                                                           
     
  if (sample)
  {
    comp = &const_one;
  }
  else
  {
    comp = &const_zero;
  }

  if (cmp_var(&vN, comp) <= 0)
  {
    *is_null = true;
    return NULL;
  }

  init_var(&vNminus1);
  sub_var(&vN, &const_one, &vNminus1);

                                        
  rscale = vsumX.dscale * 2;

  mul_var(&vsumX, &vsumX, &vsumX, rscale);                          
  mul_var(&vN, &vsumX2, &vsumX2, rscale);                          
  sub_var(&vsumX2, &vsumX, &vsumX2);                                    

  if (cmp_var(&vsumX2, &const_zero) <= 0)
  {
                                                                     
    res = make_result(&const_zero);
  }
  else
  {
    if (sample)
    {
      mul_var(&vN, &vNminus1, &vNminus1, 0);                  
    }
    else
    {
      mul_var(&vN, &vN, &vNminus1, 0);            
    }
    rscale = select_div_scale(&vsumX2, &vNminus1);
    div_var(&vsumX2, &vNminus1, &vsumX, rscale, true);               
    if (!variance)
    {
      sqrt_var(&vsumX, &vsumX, rscale);             
    }

    res = make_result(&vsumX);
  }

  free_var(&vNminus1);
  free_var(&vsumX);
  free_var(&vsumX2);

  return res;
}

Datum
numeric_var_samp(PG_FUNCTION_ARGS)
{
  NumericAggState *state;
  Numeric res;
  bool is_null;

  state = PG_ARGISNULL(0) ? NULL : (NumericAggState *)PG_GETARG_POINTER(0);

  res = numeric_stddev_internal(state, true, true, &is_null);

  if (is_null)
  {
    PG_RETURN_NULL();
  }
  else
  {
    PG_RETURN_NUMERIC(res);
  }
}

Datum
numeric_stddev_samp(PG_FUNCTION_ARGS)
{
  NumericAggState *state;
  Numeric res;
  bool is_null;

  state = PG_ARGISNULL(0) ? NULL : (NumericAggState *)PG_GETARG_POINTER(0);

  res = numeric_stddev_internal(state, false, true, &is_null);

  if (is_null)
  {
    PG_RETURN_NULL();
  }
  else
  {
    PG_RETURN_NUMERIC(res);
  }
}

Datum
numeric_var_pop(PG_FUNCTION_ARGS)
{
  NumericAggState *state;
  Numeric res;
  bool is_null;

  state = PG_ARGISNULL(0) ? NULL : (NumericAggState *)PG_GETARG_POINTER(0);

  res = numeric_stddev_internal(state, true, false, &is_null);

  if (is_null)
  {
    PG_RETURN_NULL();
  }
  else
  {
    PG_RETURN_NUMERIC(res);
  }
}

Datum
numeric_stddev_pop(PG_FUNCTION_ARGS)
{
  NumericAggState *state;
  Numeric res;
  bool is_null;

  state = PG_ARGISNULL(0) ? NULL : (NumericAggState *)PG_GETARG_POINTER(0);

  res = numeric_stddev_internal(state, false, false, &is_null);

  if (is_null)
  {
    PG_RETURN_NULL();
  }
  else
  {
    PG_RETURN_NUMERIC(res);
  }
}

#ifdef HAVE_INT128
static Numeric
numeric_poly_stddev_internal(Int128AggState *state, bool variance, bool sample, bool *is_null)
{
  NumericAggState numstate;
  Numeric res;

                                     
  memset(&numstate, 0, sizeof(NumericAggState));

  if (state)
  {
    NumericVar tmp_var;

    numstate.N = state->N;

    init_var(&tmp_var);

    int128_to_numericvar(state->sumX, &tmp_var);
    accum_sum_add(&numstate.sumX, &tmp_var);

    int128_to_numericvar(state->sumX2, &tmp_var);
    accum_sum_add(&numstate.sumX2, &tmp_var);

    free_var(&tmp_var);
  }

  res = numeric_stddev_internal(&numstate, variance, sample, is_null);

  if (numstate.sumX.ndigits > 0)
  {
    pfree(numstate.sumX.pos_digits);
    pfree(numstate.sumX.neg_digits);
  }
  if (numstate.sumX2.ndigits > 0)
  {
    pfree(numstate.sumX2.pos_digits);
    pfree(numstate.sumX2.neg_digits);
  }

  return res;
}
#endif

Datum
numeric_poly_var_samp(PG_FUNCTION_ARGS)
{
#ifdef HAVE_INT128
  PolyNumAggState *state;
  Numeric res;
  bool is_null;

  state = PG_ARGISNULL(0) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(0);

  res = numeric_poly_stddev_internal(state, true, true, &is_null);

  if (is_null)
  {
    PG_RETURN_NULL();
  }
  else
  {
    PG_RETURN_NUMERIC(res);
  }
#else
  return numeric_var_samp(fcinfo);
#endif
}

Datum
numeric_poly_stddev_samp(PG_FUNCTION_ARGS)
{
#ifdef HAVE_INT128
  PolyNumAggState *state;
  Numeric res;
  bool is_null;

  state = PG_ARGISNULL(0) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(0);

  res = numeric_poly_stddev_internal(state, false, true, &is_null);

  if (is_null)
  {
    PG_RETURN_NULL();
  }
  else
  {
    PG_RETURN_NUMERIC(res);
  }
#else
  return numeric_stddev_samp(fcinfo);
#endif
}

Datum
numeric_poly_var_pop(PG_FUNCTION_ARGS)
{
#ifdef HAVE_INT128
  PolyNumAggState *state;
  Numeric res;
  bool is_null;

  state = PG_ARGISNULL(0) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(0);

  res = numeric_poly_stddev_internal(state, true, false, &is_null);

  if (is_null)
  {
    PG_RETURN_NULL();
  }
  else
  {
    PG_RETURN_NUMERIC(res);
  }
#else
  return numeric_var_pop(fcinfo);
#endif
}

Datum
numeric_poly_stddev_pop(PG_FUNCTION_ARGS)
{
#ifdef HAVE_INT128
  PolyNumAggState *state;
  Numeric res;
  bool is_null;

  state = PG_ARGISNULL(0) ? NULL : (PolyNumAggState *)PG_GETARG_POINTER(0);

  res = numeric_poly_stddev_internal(state, false, false, &is_null);

  if (is_null)
  {
    PG_RETURN_NULL();
  }
  else
  {
    PG_RETURN_NUMERIC(res);
  }
#else
  return numeric_stddev_pop(fcinfo);
#endif
}

   
                                                   
   
                                                                         
                                                                     
                                                                             
                                                                          
                                  
   
                                                                    
                                                                             
                                                                             
                                                                           
                                                                               
                                                                   
   
                                                                  
                                                                           
   

Datum
int2_sum(PG_FUNCTION_ARGS)
{
  int64 newval;

  if (PG_ARGISNULL(0))
  {
                                          
    if (PG_ARGISNULL(1))
    {
      PG_RETURN_NULL();                        
    }
                                           
    newval = (int64)PG_GETARG_INT16(1);
    PG_RETURN_INT64(newval);
  }

     
                                                                         
                                                                            
                                                                          
                                                                           
           
     
#ifndef USE_FLOAT8_BYVAL                        
  if (AggCheckCallContext(fcinfo, NULL))
  {
    int64 *oldsum = (int64 *)PG_GETARG_POINTER(0);

                                                                  
    if (!PG_ARGISNULL(1))
    {
      *oldsum = *oldsum + (int64)PG_GETARG_INT16(1);
    }

    PG_RETURN_POINTER(oldsum);
  }
  else
#endif
  {
    int64 oldsum = PG_GETARG_INT64(0);

                                                   
    if (PG_ARGISNULL(1))
    {
      PG_RETURN_INT64(oldsum);
    }

                                
    newval = oldsum + (int64)PG_GETARG_INT16(1);

    PG_RETURN_INT64(newval);
  }
}

Datum
int4_sum(PG_FUNCTION_ARGS)
{
  int64 newval;

  if (PG_ARGISNULL(0))
  {
                                          
    if (PG_ARGISNULL(1))
    {
      PG_RETURN_NULL();                        
    }
                                           
    newval = (int64)PG_GETARG_INT32(1);
    PG_RETURN_INT64(newval);
  }

     
                                                                         
                                                                            
                                                                          
                                                                           
           
     
#ifndef USE_FLOAT8_BYVAL                        
  if (AggCheckCallContext(fcinfo, NULL))
  {
    int64 *oldsum = (int64 *)PG_GETARG_POINTER(0);

                                                                  
    if (!PG_ARGISNULL(1))
    {
      *oldsum = *oldsum + (int64)PG_GETARG_INT32(1);
    }

    PG_RETURN_POINTER(oldsum);
  }
  else
#endif
  {
    int64 oldsum = PG_GETARG_INT64(0);

                                                   
    if (PG_ARGISNULL(1))
    {
      PG_RETURN_INT64(oldsum);
    }

                                
    newval = oldsum + (int64)PG_GETARG_INT32(1);

    PG_RETURN_INT64(newval);
  }
}

   
                                                                       
   
Datum
int8_sum(PG_FUNCTION_ARGS)
{
  Numeric oldsum;
  Datum newval;

  if (PG_ARGISNULL(0))
  {
                                          
    if (PG_ARGISNULL(1))
    {
      PG_RETURN_NULL();                        
    }
                                           
    newval = DirectFunctionCall1(int8_numeric, PG_GETARG_DATUM(1));
    PG_RETURN_DATUM(newval);
  }

     
                                                                            
                                                                             
                                   
     

  oldsum = PG_GETARG_NUMERIC(0);

                                                 
  if (PG_ARGISNULL(1))
  {
    PG_RETURN_NUMERIC(oldsum);
  }

                              
  newval = DirectFunctionCall1(int8_numeric, PG_GETARG_DATUM(1));

  PG_RETURN_DATUM(DirectFunctionCall2(numeric_add, NumericGetDatum(oldsum), newval));
}

   
                                                                  
                                                       
   
                                                                  
                                                                             
                                
   

typedef struct Int8TransTypeData
{
  int64 count;
  int64 sum;
} Int8TransTypeData;

Datum
int2_avg_accum(PG_FUNCTION_ARGS)
{
  ArrayType *transarray;
  int16 newval = PG_GETARG_INT16(1);
  Int8TransTypeData *transdata;

     
                                                                         
                                                                             
                                           
     
  if (AggCheckCallContext(fcinfo, NULL))
  {
    transarray = PG_GETARG_ARRAYTYPE_P(0);
  }
  else
  {
    transarray = PG_GETARG_ARRAYTYPE_P_COPY(0);
  }

  if (ARR_HASNULL(transarray) || ARR_SIZE(transarray) != ARR_OVERHEAD_NONULLS(1) + sizeof(Int8TransTypeData))
  {
    elog(ERROR, "expected 2-element int8 array");
  }

  transdata = (Int8TransTypeData *)ARR_DATA_PTR(transarray);
  transdata->count++;
  transdata->sum += newval;

  PG_RETURN_ARRAYTYPE_P(transarray);
}

Datum
int4_avg_accum(PG_FUNCTION_ARGS)
{
  ArrayType *transarray;
  int32 newval = PG_GETARG_INT32(1);
  Int8TransTypeData *transdata;

     
                                                                         
                                                                             
                                           
     
  if (AggCheckCallContext(fcinfo, NULL))
  {
    transarray = PG_GETARG_ARRAYTYPE_P(0);
  }
  else
  {
    transarray = PG_GETARG_ARRAYTYPE_P_COPY(0);
  }

  if (ARR_HASNULL(transarray) || ARR_SIZE(transarray) != ARR_OVERHEAD_NONULLS(1) + sizeof(Int8TransTypeData))
  {
    elog(ERROR, "expected 2-element int8 array");
  }

  transdata = (Int8TransTypeData *)ARR_DATA_PTR(transarray);
  transdata->count++;
  transdata->sum += newval;

  PG_RETURN_ARRAYTYPE_P(transarray);
}

Datum
int4_avg_combine(PG_FUNCTION_ARGS)
{
  ArrayType *transarray1;
  ArrayType *transarray2;
  Int8TransTypeData *state1;
  Int8TransTypeData *state2;

  if (!AggCheckCallContext(fcinfo, NULL))
  {
    elog(ERROR, "aggregate function called in non-aggregate context");
  }

  transarray1 = PG_GETARG_ARRAYTYPE_P(0);
  transarray2 = PG_GETARG_ARRAYTYPE_P(1);

  if (ARR_HASNULL(transarray1) || ARR_SIZE(transarray1) != ARR_OVERHEAD_NONULLS(1) + sizeof(Int8TransTypeData))
  {
    elog(ERROR, "expected 2-element int8 array");
  }

  if (ARR_HASNULL(transarray2) || ARR_SIZE(transarray2) != ARR_OVERHEAD_NONULLS(1) + sizeof(Int8TransTypeData))
  {
    elog(ERROR, "expected 2-element int8 array");
  }

  state1 = (Int8TransTypeData *)ARR_DATA_PTR(transarray1);
  state2 = (Int8TransTypeData *)ARR_DATA_PTR(transarray2);

  state1->count += state2->count;
  state1->sum += state2->sum;

  PG_RETURN_ARRAYTYPE_P(transarray1);
}

Datum
int2_avg_accum_inv(PG_FUNCTION_ARGS)
{
  ArrayType *transarray;
  int16 newval = PG_GETARG_INT16(1);
  Int8TransTypeData *transdata;

     
                                                                         
                                                                             
                                           
     
  if (AggCheckCallContext(fcinfo, NULL))
  {
    transarray = PG_GETARG_ARRAYTYPE_P(0);
  }
  else
  {
    transarray = PG_GETARG_ARRAYTYPE_P_COPY(0);
  }

  if (ARR_HASNULL(transarray) || ARR_SIZE(transarray) != ARR_OVERHEAD_NONULLS(1) + sizeof(Int8TransTypeData))
  {
    elog(ERROR, "expected 2-element int8 array");
  }

  transdata = (Int8TransTypeData *)ARR_DATA_PTR(transarray);
  transdata->count--;
  transdata->sum -= newval;

  PG_RETURN_ARRAYTYPE_P(transarray);
}

Datum
int4_avg_accum_inv(PG_FUNCTION_ARGS)
{
  ArrayType *transarray;
  int32 newval = PG_GETARG_INT32(1);
  Int8TransTypeData *transdata;

     
                                                                         
                                                                             
                                           
     
  if (AggCheckCallContext(fcinfo, NULL))
  {
    transarray = PG_GETARG_ARRAYTYPE_P(0);
  }
  else
  {
    transarray = PG_GETARG_ARRAYTYPE_P_COPY(0);
  }

  if (ARR_HASNULL(transarray) || ARR_SIZE(transarray) != ARR_OVERHEAD_NONULLS(1) + sizeof(Int8TransTypeData))
  {
    elog(ERROR, "expected 2-element int8 array");
  }

  transdata = (Int8TransTypeData *)ARR_DATA_PTR(transarray);
  transdata->count--;
  transdata->sum -= newval;

  PG_RETURN_ARRAYTYPE_P(transarray);
}

Datum
int8_avg(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  Int8TransTypeData *transdata;
  Datum countd, sumd;

  if (ARR_HASNULL(transarray) || ARR_SIZE(transarray) != ARR_OVERHEAD_NONULLS(1) + sizeof(Int8TransTypeData))
  {
    elog(ERROR, "expected 2-element int8 array");
  }
  transdata = (Int8TransTypeData *)ARR_DATA_PTR(transarray);

                                               
  if (transdata->count == 0)
  {
    PG_RETURN_NULL();
  }

  countd = DirectFunctionCall1(int8_numeric, Int64GetDatumFast(transdata->count));
  sumd = DirectFunctionCall1(int8_numeric, Int64GetDatumFast(transdata->sum));

  PG_RETURN_DATUM(DirectFunctionCall2(numeric_div, sumd, countd));
}

   
                                                                
                            
   
Datum
int2int4_sum(PG_FUNCTION_ARGS)
{
  ArrayType *transarray = PG_GETARG_ARRAYTYPE_P(0);
  Int8TransTypeData *transdata;

  if (ARR_HASNULL(transarray) || ARR_SIZE(transarray) != ARR_OVERHEAD_NONULLS(1) + sizeof(Int8TransTypeData))
  {
    elog(ERROR, "expected 2-element int8 array");
  }
  transdata = (Int8TransTypeData *)ARR_DATA_PTR(transarray);

                                               
  if (transdata->count == 0)
  {
    PG_RETURN_NULL();
  }

  PG_RETURN_DATUM(Int64GetDatumFast(transdata->sum));
}

                                                                          
   
                 
   
                                                                          
   

#ifdef NUMERIC_DEBUG

   
                                                                        
   
static void
dump_numeric(const char *str, Numeric num)
{
  NumericDigit *digits = NUMERIC_DIGITS(num);
  int ndigits;
  int i;

  ndigits = NUMERIC_NDIGITS(num);

  printf("%s: NUMERIC w=%d d=%d ", str, NUMERIC_WEIGHT(num), NUMERIC_DSCALE(num));
  switch (NUMERIC_SIGN(num))
  {
  case NUMERIC_POS:
    printf("POS");
    break;
  case NUMERIC_NEG:
    printf("NEG");
    break;
  case NUMERIC_NAN:
    printf("NaN");
    break;
  default:
    printf("SIGN=0x%x", NUMERIC_SIGN(num));
    break;
  }

  for (i = 0; i < ndigits; i++)
  {
    printf(" %0*d", DEC_DIGITS, digits[i]);
  }
  printf("\n");
}

   
                                                                  
   
static void
dump_var(const char *str, NumericVar *var)
{
  int i;

  printf("%s: VAR w=%d d=%d ", str, var->weight, var->dscale);
  switch (var->sign)
  {
  case NUMERIC_POS:
    printf("POS");
    break;
  case NUMERIC_NEG:
    printf("NEG");
    break;
  case NUMERIC_NAN:
    printf("NaN");
    break;
  default:
    printf("SIGN=0x%x", var->sign);
    break;
  }

  for (i = 0; i < var->ndigits; i++)
  {
    printf(" %0*d", DEC_DIGITS, var->digits[i]);
  }

  printf("\n");
}
#endif                    

                                                                          
   
                          
   
                                                                    
                                                                   
   
                                                                          
   

   
                 
   
                                                                               
   
static void
alloc_var(NumericVar *var, int ndigits)
{
  digitbuf_free(var->buf);
  var->buf = digitbuf_alloc(ndigits + 1);
  var->buf[0] = 0;                               
  var->digits = var->buf + 1;
  var->ndigits = ndigits;
}

   
                
   
                                                          
   
static void
free_var(NumericVar *var)
{
  digitbuf_free(var->buf);
  var->buf = NULL;
  var->digits = NULL;
  var->sign = NUMERIC_NAN;
}

   
                
   
                           
                                    
   
static void
zero_var(NumericVar *var)
{
  digitbuf_free(var->buf);
  var->buf = NULL;
  var->digits = NULL;
  var->ndigits = 0;
  var->weight = 0;                                                   
  var->sign = NUMERIC_POS;                          
}

   
                      
   
                                                     
   
                                                                            
                                                                          
                                                          
   
                                                                          
                                                                           
   
static const char *
set_var_from_str(const char *str, const char *cp, NumericVar *dest)
{
  bool have_dp = false;
  int i;
  unsigned char *decdigits;
  int sign = NUMERIC_POS;
  int dweight = -1;
  int ddigits;
  int dscale = 0;
  int weight;
  int ndigits;
  int offset;
  NumericDigit *digits;

     
                                                                           
                                                                    
     
  switch (*cp)
  {
  case '+':
    sign = NUMERIC_POS;
    cp++;
    break;

  case '-':
    sign = NUMERIC_NEG;
    cp++;
    break;
  }

  if (*cp == '.')
  {
    have_dp = true;
    cp++;
  }

  if (!isdigit((unsigned char)*cp))
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "numeric", str)));
  }

  decdigits = (unsigned char *)palloc(strlen(cp) + DEC_DIGITS * 2);

                                                 
  memset(decdigits, 0, DEC_DIGITS);
  i = DEC_DIGITS;

  while (*cp)
  {
    if (isdigit((unsigned char)*cp))
    {
      decdigits[i++] = *cp++ - '0';
      if (!have_dp)
      {
        dweight++;
      }
      else
      {
        dscale++;
      }
    }
    else if (*cp == '.')
    {
      if (have_dp)
      {
        ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "numeric", str)));
      }
      have_dp = true;
      cp++;
    }
    else
    {
      break;
    }
  }

  ddigits = i - DEC_DIGITS;
                                                  
  memset(decdigits + i, 0, DEC_DIGITS - 1);

                               
  if (*cp == 'e' || *cp == 'E')
  {
    long exponent;
    char *endptr;

    cp++;
    exponent = strtol(cp, &endptr, 10);
    if (endptr == cp)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "numeric", str)));
    }
    cp = endptr;

       
                                                                  
                                                                    
                                                                       
                                                                        
                                                                          
                                                                           
       
    if (exponent >= INT_MAX / 2 || exponent <= -(INT_MAX / 2))
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value overflows numeric format")));
    }
    dweight += (int)exponent;
    dscale -= (int)exponent;
    if (dscale < 0)
    {
      dscale = 0;
    }
  }

     
                                                                             
                                                                             
                                                                     
                                          
     
  if (dweight >= 0)
  {
    weight = (dweight + 1 + DEC_DIGITS - 1) / DEC_DIGITS - 1;
  }
  else
  {
    weight = -((-dweight - 1) / DEC_DIGITS + 1);
  }
  offset = (weight + 1) * DEC_DIGITS - (dweight + 1);
  ndigits = (ddigits + offset + DEC_DIGITS - 1) / DEC_DIGITS;

  alloc_var(dest, ndigits);
  dest->sign = sign;
  dest->weight = weight;
  dest->dscale = dscale;

  i = DEC_DIGITS - offset;
  digits = dest->digits;

  while (ndigits-- > 0)
  {
#if DEC_DIGITS == 4
    *digits++ = ((decdigits[i] * 10 + decdigits[i + 1]) * 10 + decdigits[i + 2]) * 10 + decdigits[i + 3];
#elif DEC_DIGITS == 2
    *digits++ = decdigits[i] * 10 + decdigits[i + 1];
#elif DEC_DIGITS == 1
    *digits++ = decdigits[i];
#else
#error unsupported NBASE
#endif
    i += DEC_DIGITS;
  }

  pfree(decdigits);

                                                                       
  strip_var(dest);

                                        
  return cp;
}

   
                        
   
                                                
   
static void
set_var_from_num(Numeric num, NumericVar *dest)
{
  int ndigits;

  ndigits = NUMERIC_NDIGITS(num);

  alloc_var(dest, ndigits);

  dest->weight = NUMERIC_WEIGHT(num);
  dest->sign = NUMERIC_SIGN(num);
  dest->dscale = NUMERIC_DSCALE(num);

  memcpy(dest->digits, NUMERIC_DIGITS(num), ndigits * sizeof(NumericDigit));
}

   
                         
   
                                                                        
                                                                           
                                                                             
                                                                           
                                             
   
                                                                           
                                                                            
                                                                           
                                                           
   
static void
init_var_from_num(Numeric num, NumericVar *dest)
{
  dest->ndigits = NUMERIC_NDIGITS(num);
  dest->weight = NUMERIC_WEIGHT(num);
  dest->sign = NUMERIC_SIGN(num);
  dest->dscale = NUMERIC_DSCALE(num);
  dest->digits = NUMERIC_DIGITS(num);
  dest->buf = NULL;                                   
}

   
                        
   
                                  
   
static void
set_var_from_var(const NumericVar *value, NumericVar *dest)
{
  NumericDigit *newbuf;

  newbuf = digitbuf_alloc(value->ndigits + 1);
  newbuf[0] = 0;                                        
  if (value->ndigits > 0)                                       
  {
    memcpy(newbuf + 1, value->digits, value->ndigits * sizeof(NumericDigit));
  }

  digitbuf_free(dest->buf);

  memmove(dest, value, sizeof(NumericVar));
  dest->buf = newbuf;
  dest->digits = newbuf + 1;
}

   
                        
   
                                                               
                                                                         
                              
   
static char *
get_str_from_var(const NumericVar *var)
{
  int dscale;
  char *str;
  char *cp;
  char *endcp;
  int i;
  int d;
  NumericDigit dig;

#if DEC_DIGITS > 1
  NumericDigit d1;
#endif

  dscale = var->dscale;

     
                                    
     
                                                                             
                                                                            
                                                                          
                                                         
     
  i = (var->weight + 1) * DEC_DIGITS;
  if (i <= 0)
  {
    i = 1;
  }

  str = palloc(i + dscale + DEC_DIGITS + 2);
  cp = str;

     
                                       
     
  if (var->sign == NUMERIC_NEG)
  {
    *cp++ = '-';
  }

     
                                                
     
  if (var->weight < 0)
  {
    d = var->weight + 1;
    *cp++ = '0';
  }
  else
  {
    for (d = 0; d <= var->weight; d++)
    {
      dig = (d < var->ndigits) ? var->digits[d] : 0;
                                                                     
#if DEC_DIGITS == 4
      {
        bool putit = (d > 0);

        d1 = dig / 1000;
        dig -= d1 * 1000;
        putit |= (d1 > 0);
        if (putit)
        {
          *cp++ = d1 + '0';
        }
        d1 = dig / 100;
        dig -= d1 * 100;
        putit |= (d1 > 0);
        if (putit)
        {
          *cp++ = d1 + '0';
        }
        d1 = dig / 10;
        dig -= d1 * 10;
        putit |= (d1 > 0);
        if (putit)
        {
          *cp++ = d1 + '0';
        }
        *cp++ = dig + '0';
      }
#elif DEC_DIGITS == 2
      d1 = dig / 10;
      dig -= d1 * 10;
      if (d1 > 0 || d > 0)
      {
        *cp++ = d1 + '0';
      }
      *cp++ = dig + '0';
#elif DEC_DIGITS == 1
      *cp++ = dig + '0';
#else
#error unsupported NBASE
#endif
    }
  }

     
                                                                             
                                                                            
             
     
  if (dscale > 0)
  {
    *cp++ = '.';
    endcp = cp + dscale;
    for (i = 0; i < dscale; d++, i += DEC_DIGITS)
    {
      dig = (d >= 0 && d < var->ndigits) ? var->digits[d] : 0;
#if DEC_DIGITS == 4
      d1 = dig / 1000;
      dig -= d1 * 1000;
      *cp++ = d1 + '0';
      d1 = dig / 100;
      dig -= d1 * 100;
      *cp++ = d1 + '0';
      d1 = dig / 10;
      dig -= d1 * 10;
      *cp++ = d1 + '0';
      *cp++ = dig + '0';
#elif DEC_DIGITS == 2
      d1 = dig / 10;
      dig -= d1 * 10;
      *cp++ = d1 + '0';
      *cp++ = dig + '0';
#elif DEC_DIGITS == 1
      *cp++ = dig + '0';
#else
#error unsupported NBASE
#endif
    }
    cp = endcp;
  }

     
                                        
     
  *cp = '\0';
  return str;
}

   
                            
   
                                                                          
                                                               
   
                                                                        
                                                   
   
                                                                         
                                                                        
                                   
   
                                                           
   
                                                      
   
                                                                             
                                                                
   
                              
   
static char *
get_str_from_var_sci(const NumericVar *var, int rscale)
{
  int32 exponent;
  NumericVar tmp_var;
  size_t len;
  char *str;
  char *sig_out;

  if (rscale < 0)
  {
    rscale = 0;
  }

     
                                                               
     
                                                                         
                                                 
     
  if (var->ndigits > 0)
  {
    exponent = (var->weight + 1) * DEC_DIGITS;

       
                                                                           
                                  
       
    exponent -= DEC_DIGITS - (int)log10(var->digits[0]);
  }
  else
  {
       
                                                   
       
                                                                         
                                                                          
                  
       
    exponent = 0;
  }

     
                                                                          
                                    
     
  init_var(&tmp_var);

  power_ten_int(exponent, &tmp_var);
  div_var(var, &tmp_var, &tmp_var, rscale, true);
  sig_out = get_str_from_var(&tmp_var);

  free_var(&tmp_var);

     
                                    
     
                                                                   
                                                                         
                                                         
     
  len = strlen(sig_out) + 13;
  str = palloc(len);
  snprintf(str, len, "%se%+03d", sig_out, exponent);

  pfree(sig_out);

  return str;
}

   
                             
   
                                                                 
                                                                        
                                                                           
              
   
static Numeric
make_result_opt_error(const NumericVar *var, bool *have_error)
{
  Numeric result;
  NumericDigit *digits = var->digits;
  int weight = var->weight;
  int sign = var->sign;
  int n;
  Size len;

  if (sign == NUMERIC_NAN)
  {
    result = (Numeric)palloc(NUMERIC_HDRSZ_SHORT);

    SET_VARSIZE(result, NUMERIC_HDRSZ_SHORT);
    result->choice.n_header = NUMERIC_NAN;
                                        

    dump_numeric("make_result()", result);
    return result;
  }

  n = var->ndigits;

                               
  while (n > 0 && *digits == 0)
  {
    digits++;
    weight--;
    n--;
  }
                                
  while (n > 0 && digits[n - 1] == 0)
  {
    n--;
  }

                                                           
  if (n == 0)
  {
    weight = 0;
    sign = NUMERIC_POS;
  }

                        
  if (NUMERIC_CAN_BE_SHORT(var->dscale, weight))
  {
    len = NUMERIC_HDRSZ_SHORT + n * sizeof(NumericDigit);
    result = (Numeric)palloc(len);
    SET_VARSIZE(result, len);
    result->choice.n_short.n_header = (sign == NUMERIC_NEG ? (NUMERIC_SHORT | NUMERIC_SHORT_SIGN_MASK) : NUMERIC_SHORT) | (var->dscale << NUMERIC_SHORT_DSCALE_SHIFT) | (weight < 0 ? NUMERIC_SHORT_WEIGHT_SIGN_MASK : 0) | (weight & NUMERIC_SHORT_WEIGHT_MASK);
  }
  else
  {
    len = NUMERIC_HDRSZ + n * sizeof(NumericDigit);
    result = (Numeric)palloc(len);
    SET_VARSIZE(result, len);
    result->choice.n_long.n_sign_dscale = sign | (var->dscale & NUMERIC_DSCALE_MASK);
    result->choice.n_long.n_weight = weight;
  }

  Assert(NUMERIC_NDIGITS(result) == n);
  if (n > 0)
  {
    memcpy(NUMERIC_DIGITS(result), digits, n * sizeof(NumericDigit));
  }

                                          
  if (NUMERIC_WEIGHT(result) != weight || NUMERIC_DSCALE(result) != var->dscale)
  {
    if (have_error)
    {
      *have_error = true;
      return NULL;
    }
    else
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value overflows numeric format")));
    }
  }

  dump_numeric("make_result()", result);
  return result;
}

   
                   
   
                                                                          
   
static Numeric
make_result(const NumericVar *var)
{
  return make_result_opt_error(var, NULL);
}

   
                    
   
                                                               
                 
   
static void
apply_typmod(NumericVar *var, int32 typmod)
{
  int precision;
  int scale;
  int maxdigits;
  int ddigits;
  int i;

                                                   
  if (typmod < (int32)(VARHDRSZ))
  {
    return;
  }

  typmod -= VARHDRSZ;
  precision = (typmod >> 16) & 0xffff;
  scale = typmod & 0xffff;
  maxdigits = precision - scale;

                                                   
  round_var(var, scale);

     
                                                                         
                                                                             
                                                                          
                                                                           
                                                    
     
  ddigits = (var->weight + 1) * DEC_DIGITS;
  if (ddigits > maxdigits)
  {
                                                              
    for (i = 0; i < var->ndigits; i++)
    {
      NumericDigit dig = var->digits[i];

      if (dig)
      {
                                                           
#if DEC_DIGITS == 4
        if (dig < 10)
        {
          ddigits -= 3;
        }
        else if (dig < 100)
        {
          ddigits -= 2;
        }
        else if (dig < 1000)
        {
          ddigits -= 1;
        }
#elif DEC_DIGITS == 2
        if (dig < 10)
        {
          ddigits -= 1;
        }
#elif DEC_DIGITS == 1
                           
#else
#error unsupported NBASE
#endif
        if (ddigits > maxdigits)
        {
          ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("numeric field overflow"),
                             errdetail("A field with precision %d, scale %d must round to an absolute value less than %s%d.", precision, scale,
                                                        
                                 maxdigits ? "10^" : "", maxdigits ? maxdigits : 1)));
        }
        break;
      }
      ddigits -= DEC_DIGITS;
    }
  }
}

   
                                                
   
                                                                         
   
static bool
numericvar_to_int64(const NumericVar *var, int64 *result)
{
  NumericDigit *digits;
  int ndigits;
  int weight;
  int i;
  int64 val;
  bool neg;
  NumericVar rounded;

                                
  init_var(&rounded);
  set_var_from_var(var, &rounded);
  round_var(&rounded, 0);

                            
  strip_var(&rounded);
  ndigits = rounded.ndigits;
  if (ndigits == 0)
  {
    *result = 0;
    free_var(&rounded);
    return true;
  }

     
                                                                           
                                                                          
     
  weight = rounded.weight;
  Assert(weight >= 0 && ndigits <= weight + 1);

     
                                                                   
                                                                             
                                                                           
     
  digits = rounded.digits;
  neg = (rounded.sign == NUMERIC_NEG);
  val = -digits[0];
  for (i = 1; i <= weight; i++)
  {
    if (unlikely(pg_mul_s64_overflow(val, NBASE, &val)))
    {
      free_var(&rounded);
      return false;
    }

    if (i < ndigits)
    {
      if (unlikely(pg_sub_s64_overflow(val, digits[i], &val)))
      {
        free_var(&rounded);
        return false;
      }
    }
  }

  free_var(&rounded);

  if (!neg)
  {
    if (unlikely(val == PG_INT64_MIN))
    {
      return false;
    }
    val = -val;
  }
  *result = val;

  return true;
}

   
                                  
   
static void
int64_to_numericvar(int64 val, NumericVar *var)
{
  uint64 uval, newuval;
  NumericDigit *ptr;
  int ndigits;

                                                                       
  alloc_var(var, 20 / DEC_DIGITS);
  if (val < 0)
  {
    var->sign = NUMERIC_NEG;
    uval = -val;
  }
  else
  {
    var->sign = NUMERIC_POS;
    uval = val;
  }
  var->dscale = 0;
  if (val == 0)
  {
    var->ndigits = 0;
    var->weight = 0;
    return;
  }
  ptr = var->digits + var->ndigits;
  ndigits = 0;
  do
  {
    ptr--;
    ndigits++;
    newuval = uval / NBASE;
    *ptr = uval - newuval * NBASE;
    uval = newuval;
  } while (uval);
  var->digits = ptr;
  var->ndigits = ndigits;
  var->weight = ndigits - 1;
}

#ifdef HAVE_INT128
   
                                                  
   
                                                                         
   
static bool
numericvar_to_int128(const NumericVar *var, int128 *result)
{
  NumericDigit *digits;
  int ndigits;
  int weight;
  int i;
  int128 val, oldval;
  bool neg;
  NumericVar rounded;

                                
  init_var(&rounded);
  set_var_from_var(var, &rounded);
  round_var(&rounded, 0);

                            
  strip_var(&rounded);
  ndigits = rounded.ndigits;
  if (ndigits == 0)
  {
    *result = 0;
    free_var(&rounded);
    return true;
  }

     
                                                                           
                                                                          
     
  weight = rounded.weight;
  Assert(weight >= 0 && ndigits <= weight + 1);

                            
  digits = rounded.digits;
  neg = (rounded.sign == NUMERIC_NEG);
  val = digits[0];
  for (i = 1; i <= weight; i++)
  {
    oldval = val;
    val *= NBASE;
    if (i < ndigits)
    {
      val += digits[i];
    }

       
                                                                    
                                                                         
                                                                     
                                                                           
                
       
    if ((val / NBASE) != oldval)                         
    {
      if (!neg || (-val) != val || val == 0 || oldval < 0)
      {
        free_var(&rounded);
        return false;
      }
    }
  }

  free_var(&rounded);

  *result = neg ? -val : val;
  return true;
}

   
                                       
   
static void
int128_to_numericvar(int128 val, NumericVar *var)
{
  uint128 uval, newuval;
  NumericDigit *ptr;
  int ndigits;

                                                                        
  alloc_var(var, 40 / DEC_DIGITS);
  if (val < 0)
  {
    var->sign = NUMERIC_NEG;
    uval = -val;
  }
  else
  {
    var->sign = NUMERIC_POS;
    uval = val;
  }
  var->dscale = 0;
  if (val == 0)
  {
    var->ndigits = 0;
    var->weight = 0;
    return;
  }
  ptr = var->digits + var->ndigits;
  ndigits = 0;
  do
  {
    ptr--;
    ndigits++;
    newuval = uval / NBASE;
    *ptr = uval - newuval * NBASE;
    uval = newuval;
  } while (uval);
  var->digits = ptr;
  var->ndigits = ndigits;
  var->weight = ndigits - 1;
}
#endif

   
                                                                   
   
static double
numeric_to_double_no_overflow(Numeric num)
{
  char *tmp;
  double val;
  char *endptr;

  tmp = DatumGetCString(DirectFunctionCall1(numeric_out, NumericGetDatum(num)));

                                                     
  val = strtod(tmp, &endptr);
  if (*endptr != '\0')
  {
                              
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "double precision", tmp)));
  }

  pfree(tmp);

  return val;
}

                                          
static double
numericvar_to_double_no_overflow(const NumericVar *var)
{
  char *tmp;
  double val;
  char *endptr;

  tmp = get_str_from_var(var);

                                                     
  val = strtod(tmp, &endptr);
  if (*endptr != '\0')
  {
                              
    ereport(ERROR, (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION), errmsg("invalid input syntax for type %s: \"%s\"", "double precision", tmp)));
  }

  pfree(tmp);

  return val;
}

   
               
   
                                                                     
                           
   
static int
cmp_var(const NumericVar *var1, const NumericVar *var2)
{
  return cmp_var_common(var1->digits, var1->ndigits, var1->weight, var1->sign, var2->digits, var2->ndigits, var2->weight, var2->sign);
}

   
                      
   
                                                                
                           
   
static int
cmp_var_common(const NumericDigit *var1digits, int var1ndigits, int var1weight, int var1sign, const NumericDigit *var2digits, int var2ndigits, int var2weight, int var2sign)
{
  if (var1ndigits == 0)
  {
    if (var2ndigits == 0)
    {
      return 0;
    }
    if (var2sign == NUMERIC_NEG)
    {
      return 1;
    }
    return -1;
  }
  if (var2ndigits == 0)
  {
    if (var1sign == NUMERIC_POS)
    {
      return 1;
    }
    return -1;
  }

  if (var1sign == NUMERIC_POS)
  {
    if (var2sign == NUMERIC_NEG)
    {
      return 1;
    }
    return cmp_abs_common(var1digits, var1ndigits, var1weight, var2digits, var2ndigits, var2weight);
  }

  if (var2sign == NUMERIC_POS)
  {
    return -1;
  }

  return cmp_abs_common(var2digits, var2ndigits, var2weight, var1digits, var1ndigits, var1weight);
}

   
               
   
                                                                         
                                                                 
   
static void
add_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result)
{
     
                                                         
     
  if (var1->sign == NUMERIC_POS)
  {
    if (var2->sign == NUMERIC_POS)
    {
         
                                                             
         
      add_abs(var1, var2, result);
      result->sign = NUMERIC_POS;
    }
    else
    {
         
                                                                         
         
      switch (cmp_abs(var1, var2))
      {
      case 0:
                      
                                  
                         
                      
           
        zero_var(result);
        result->dscale = Max(var1->dscale, var2->dscale);
        break;

      case 1:
                      
                                 
                                             
                      
           
        sub_abs(var1, var2, result);
        result->sign = NUMERIC_POS;
        break;

      case -1:
                      
                                 
                                             
                      
           
        sub_abs(var2, var1, result);
        result->sign = NUMERIC_NEG;
        break;
      }
    }
  }
  else
  {
    if (var2->sign == NUMERIC_POS)
    {
                    
                                            
                                      
                    
         
      switch (cmp_abs(var1, var2))
      {
      case 0:
                      
                                  
                         
                      
           
        zero_var(result);
        result->dscale = Max(var1->dscale, var2->dscale);
        break;

      case 1:
                      
                                 
                                             
                      
           
        sub_abs(var1, var2, result);
        result->sign = NUMERIC_NEG;
        break;

      case -1:
                      
                                 
                                             
                      
           
        sub_abs(var2, var1, result);
        result->sign = NUMERIC_POS;
        break;
      }
    }
    else
    {
                    
                           
                                           
                    
         
      add_abs(var1, var2, result);
      result->sign = NUMERIC_NEG;
    }
  }
}

   
               
   
                                                                         
                                                                 
   
static void
sub_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result)
{
     
                                                         
     
  if (var1->sign == NUMERIC_POS)
  {
    if (var2->sign == NUMERIC_NEG)
    {
                    
                                            
                                           
                    
         
      add_abs(var1, var2, result);
      result->sign = NUMERIC_POS;
    }
    else
    {
                    
                           
                                      
                    
         
      switch (cmp_abs(var1, var2))
      {
      case 0:
                      
                                  
                         
                      
           
        zero_var(result);
        result->dscale = Max(var1->dscale, var2->dscale);
        break;

      case 1:
                      
                                 
                                             
                      
           
        sub_abs(var1, var2, result);
        result->sign = NUMERIC_POS;
        break;

      case -1:
                      
                                 
                                             
                      
           
        sub_abs(var2, var1, result);
        result->sign = NUMERIC_NEG;
        break;
      }
    }
  }
  else
  {
    if (var2->sign == NUMERIC_NEG)
    {
                    
                           
                                      
                    
         
      switch (cmp_abs(var1, var2))
      {
      case 0:
                      
                                  
                         
                      
           
        zero_var(result);
        result->dscale = Max(var1->dscale, var2->dscale);
        break;

      case 1:
                      
                                 
                                             
                      
           
        sub_abs(var1, var2, result);
        result->sign = NUMERIC_NEG;
        break;

      case -1:
                      
                                 
                                             
                      
           
        sub_abs(var2, var1, result);
        result->sign = NUMERIC_POS;
        break;
      }
    }
    else
    {
                    
                                            
                                           
                    
         
      add_abs(var1, var2, result);
      result->sign = NUMERIC_NEG;
    }
  }
}

   
               
   
                                                                      
                                                                           
   
static void
mul_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result, int rscale)
{
  int res_ndigits;
  int res_sign;
  int res_weight;
  int maxdigits;
  int *dig;
  int carry;
  int maxdig;
  int newdig;
  int var1ndigits;
  int var2ndigits;
  NumericDigit *var1digits;
  NumericDigit *var2digits;
  NumericDigit *res_digits;
  int i, i1, i2;

     
                                                                           
                                                                            
                                                                           
                                                                        
                                               
     
  if (var1->ndigits > var2->ndigits)
  {
    const NumericVar *tmp = var1;

    var1 = var2;
    var2 = tmp;
  }

                                                                 
  var1ndigits = var1->ndigits;
  var2ndigits = var2->ndigits;
  var1digits = var1->digits;
  var2digits = var2->digits;

  if (var1ndigits == 0 || var2ndigits == 0)
  {
                                                  
    zero_var(result);
    result->dscale = rscale;
    return;
  }

                                                           
  if (var1->sign == var2->sign)
  {
    res_sign = NUMERIC_POS;
  }
  else
  {
    res_sign = NUMERIC_NEG;
  }
  res_weight = var1->weight + var2->weight + 2;

     
                                                                            
                                                                             
                                                                        
                                                                            
                                                                          
                                                                       
     
                                                                          
                                                                        
                                                                             
     
  res_ndigits = var1ndigits + var2ndigits + 1;
  maxdigits = res_weight + 1 + (rscale + DEC_DIGITS - 1) / DEC_DIGITS + MUL_GUARD_DIGITS;
  res_ndigits = Min(res_ndigits, maxdigits);

  if (res_ndigits < 3)
  {
                                                             
    zero_var(result);
    result->dscale = rscale;
    return;
  }

     
                                                                      
                                                                           
                                               
     
                                                                            
                                                                         
                                                                           
                                                                            
                                                                        
                                     
     
                                                                        
                                                                         
                                                           
     
  dig = (int *)palloc0(res_ndigits * sizeof(int));
  maxdig = 0;

     
                                                                          
                                                                            
                       
     
                                                                             
                                                                          
                                           
     
  for (i1 = Min(var1ndigits - 1, res_ndigits - 3); i1 >= 0; i1--)
  {
    int var1digit = var1digits[i1];

    if (var1digit == 0)
    {
      continue;
    }

                            
    maxdig += var1digit;
    if (maxdig > (INT_MAX - INT_MAX / NBASE) / (NBASE - 1))
    {
                      
      carry = 0;
      for (i = res_ndigits - 1; i >= 0; i--)
      {
        newdig = dig[i] + carry;
        if (newdig >= NBASE)
        {
          carry = newdig / NBASE;
          newdig -= carry * NBASE;
        }
        else
        {
          carry = 0;
        }
        dig[i] = newdig;
      }
      Assert(carry == 0);
                                                   
      maxdig = 1 + var1digit;
    }

       
                                                                  
       
                                                                         
                                                                       
       
    for (i2 = Min(var2ndigits - 1, res_ndigits - i1 - 3), i = i1 + i2 + 2; i2 >= 0; i2--)
    {
      dig[i--] += var1digit * var2digits[i2];
    }
  }

     
                                                                             
                                                                          
                                                          
     
  alloc_var(result, res_ndigits);
  res_digits = result->digits;
  carry = 0;
  for (i = res_ndigits - 1; i >= 0; i--)
  {
    newdig = dig[i] + carry;
    if (newdig >= NBASE)
    {
      carry = newdig / NBASE;
      newdig -= carry * NBASE;
    }
    else
    {
      carry = 0;
    }
    res_digits[i] = newdig;
  }
  Assert(carry == 0);

  pfree(dig);

     
                                                           
     
  result->weight = res_weight;
  result->sign = res_sign;

                                                       
  round_var(result, rscale);

                                         
  strip_var(result);
}

   
               
   
                                                                            
                                                                
                                                                        
                                              
   
static void
div_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result, int rscale, bool round)
{
  int div_ndigits;
  int res_ndigits;
  int res_sign;
  int res_weight;
  int carry;
  int borrow;
  int divisor1;
  int divisor2;
  NumericDigit *dividend;
  NumericDigit *divisor;
  NumericDigit *res_digits;
  int i;
  int j;

                                                                 
  int var1ndigits = var1->ndigits;
  int var2ndigits = var2->ndigits;

     
                                                                   
                           
     
  if (var2ndigits == 0 || var2->digits[0] == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
  }

     
                           
     
  if (var1ndigits == 0)
  {
    zero_var(result);
    result->dscale = rscale;
    return;
  }

     
                                                                          
                                                                       
                                                                    
     
  if (var1->sign == var2->sign)
  {
    res_sign = NUMERIC_POS;
  }
  else
  {
    res_sign = NUMERIC_NEG;
  }
  res_weight = var1->weight - var2->weight;
                                                                
  res_ndigits = res_weight + 1 + (rscale + DEC_DIGITS - 1) / DEC_DIGITS;
                                 
  res_ndigits = Max(res_ndigits, 1);
                                                                          
  if (round)
  {
    res_ndigits++;
  }

     
                                                                      
                                                                         
                                                                     
                                                                        
                                 
     
  div_ndigits = res_ndigits + var2ndigits;
  div_ndigits = Max(div_ndigits, var1ndigits);

     
                                                                           
                                                                        
                                                                           
                                                                         
                                                                           
                                                                           
                                                                          
                                                                        
     
  dividend = (NumericDigit *)palloc0((div_ndigits + var2ndigits + 2) * sizeof(NumericDigit));
  divisor = dividend + (div_ndigits + 1);
  memcpy(dividend + 1, var1->digits, var1ndigits * sizeof(NumericDigit));
  memcpy(divisor + 1, var2->digits, var2ndigits * sizeof(NumericDigit));

     
                                                                          
     
  alloc_var(result, res_ndigits);
  res_digits = result->digits;

  if (var2ndigits == 1)
  {
       
                                                                           
                                         
       
    divisor1 = divisor[1];
    carry = 0;
    for (i = 0; i < res_ndigits; i++)
    {
      carry = carry * NBASE + dividend[i + 1];
      res_digits[i] = carry / divisor1;
      carry = carry % divisor1;
    }
  }
  else
  {
       
                                                                       
                         
       
                                                                       
                                                                     
                                                                       
                                            
       
    if (divisor[1] < HALF_NBASE)
    {
      int d = NBASE / (divisor[1] + 1);

      carry = 0;
      for (i = var2ndigits; i > 0; i--)
      {
        carry += divisor[i] * d;
        divisor[i] = carry % NBASE;
        carry = carry / NBASE;
      }
      Assert(carry == 0);
      carry = 0;
                                                                     
      for (i = var1ndigits; i >= 0; i--)
      {
        carry += dividend[i] * d;
        dividend[i] = carry % NBASE;
        carry = carry / NBASE;
      }
      Assert(carry == 0);
      Assert(divisor[1] >= HALF_NBASE);
    }
                                                                 
    divisor1 = divisor[1];
    divisor2 = divisor[2];

       
                                                                           
                                                                        
                                                                  
                                    
       
    for (j = 0; j < res_ndigits; j++)
    {
                                                                      
      int next2digits = dividend[j] * NBASE + dividend[j + 1];
      int qhat;

         
                                                                         
                                                                     
                                                                    
                   
         
      if (next2digits == 0)
      {
        res_digits[j] = 0;
        continue;
      }

      if (dividend[j] == divisor1)
      {
        qhat = NBASE - 1;
      }
      else
      {
        qhat = next2digits / divisor1;
      }

         
                                                                     
                                                                       
                                                                       
                                                            
         
      while (divisor2 * qhat > (next2digits - qhat * divisor1) * NBASE + dividend[j + 2])
      {
        qhat--;
      }

                                                                   
      if (qhat > 0)
      {
           
                                                                    
                                                                 
                                                                    
           
        carry = 0;
        borrow = 0;
        for (i = var2ndigits; i >= 0; i--)
        {
          carry += divisor[i] * qhat;
          borrow -= carry % NBASE;
          carry = carry / NBASE;
          borrow += dividend[j + i];
          if (borrow < 0)
          {
            dividend[j + i] = borrow + NBASE;
            borrow = -1;
          }
          else
          {
            dividend[j + i] = borrow;
            borrow = 0;
          }
        }
        Assert(carry == 0);

           
                                                                  
                                                                    
                                                                   
                                                                       
                                                                     
                                              
           
        if (borrow)
        {
          qhat--;
          carry = 0;
          for (i = var2ndigits; i >= 0; i--)
          {
            carry += dividend[j + i] + divisor[i];
            if (carry >= NBASE)
            {
              dividend[j + i] = carry - NBASE;
              carry = 1;
            }
            else
            {
              dividend[j + i] = carry;
              carry = 0;
            }
          }
                                                                    
          Assert(carry == 1);
        }
      }

                                                   
      res_digits[j] = qhat;
    }
  }

  pfree(dividend);

     
                                                                       
     
  result->weight = res_weight;
  result->sign = res_sign;

                                                                   
  if (round)
  {
    round_var(result, rscale);
  }
  else
  {
    trunc_var(result, rscale);
  }

                                         
  strip_var(result);
}

   
                    
   
                                                                           
                                                                            
                                                                      
                                                                        
                                                                          
                                                                    
                                                                            
                                                                          
   
                                                                        
                                                                           
                                                                          
                                                                         
                            
   
static void
div_var_fast(const NumericVar *var1, const NumericVar *var2, NumericVar *result, int rscale, bool round)
{
  int div_ndigits;
  int res_sign;
  int res_weight;
  int *div;
  int qdigit;
  int carry;
  int maxdiv;
  int newdig;
  NumericDigit *res_digits;
  double fdividend, fdivisor, fdivisorinverse, fquotient;
  int qi;
  int i;

                                                                 
  int var1ndigits = var1->ndigits;
  int var2ndigits = var2->ndigits;
  NumericDigit *var1digits = var1->digits;
  NumericDigit *var2digits = var2->digits;

     
                                                                   
                           
     
  if (var2ndigits == 0 || var2digits[0] == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
  }

     
                           
     
  if (var1ndigits == 0)
  {
    zero_var(result);
    result->dscale = rscale;
    return;
  }

     
                                                                         
     
  if (var1->sign == var2->sign)
  {
    res_sign = NUMERIC_POS;
  }
  else
  {
    res_sign = NUMERIC_NEG;
  }
  res_weight = var1->weight - var2->weight + 1;
                                                                
  div_ndigits = res_weight + 1 + (rscale + DEC_DIGITS - 1) / DEC_DIGITS;
                                           
  div_ndigits += DIV_GUARD_DIGITS;
  if (div_ndigits < DIV_GUARD_DIGITS)
  {
    div_ndigits = DIV_GUARD_DIGITS;
  }
                                                                        
  if (div_ndigits < var1ndigits)
  {
    div_ndigits = var1ndigits;
  }

     
                                                                      
                                                                           
                                               
     
                                                                   
                                                                            
                                                                      
                                                                         
                                                                          
                                           
     
  div = (int *)palloc0((div_ndigits + 1) * sizeof(int));
  for (i = 0; i < var1ndigits; i++)
  {
    div[i + 1] = var1digits[i];
  }

     
                                                                             
                                                                             
                                                                            
                                                
     
  fdivisor = (double)var2digits[0];
  for (i = 1; i < 4; i++)
  {
    fdivisor *= NBASE;
    if (i < var2ndigits)
    {
      fdivisor += (double)var2digits[i];
    }
  }
  fdivisorinverse = 1.0 / fdivisor;

     
                                                                           
                                                                          
                                                                          
                                                                            
                                                                       
                                                                           
     
                                                                        
                                                                           
                                                                        
     
                                                                             
                                                                            
                                                                       
                                                                          
     
  maxdiv = 1;

     
                                                                         
     
  for (qi = 0; qi < div_ndigits; qi++)
  {
                                                
    fdividend = (double)div[qi];
    for (i = 1; i < 4; i++)
    {
      fdividend *= NBASE;
      if (qi + i <= div_ndigits)
      {
        fdividend += (double)div[qi + i];
      }
    }
                                                  
    fquotient = fdividend * fdivisorinverse;
    qdigit = (fquotient >= 0.0) ? ((int)fquotient) : (((int)fquotient) - 1);                                 

    if (qdigit != 0)
    {
                                        
      maxdiv += Abs(qdigit);
      if (maxdiv > (INT_MAX - INT_MAX / NBASE - 1) / (NBASE - 1))
      {
                        
        carry = 0;
        for (i = div_ndigits; i > qi; i--)
        {
          newdig = div[i] + carry;
          if (newdig < 0)
          {
            carry = -((-newdig - 1) / NBASE) - 1;
            newdig -= carry * NBASE;
          }
          else if (newdig >= NBASE)
          {
            carry = newdig / NBASE;
            newdig -= carry * NBASE;
          }
          else
          {
            carry = 0;
          }
          div[i] = newdig;
        }
        newdig = div[qi] + carry;
        div[qi] = newdig;

           
                                                                       
                                                                    
                                                                  
           
        maxdiv = 1;

           
                                                                
                                                        
           
        fdividend = (double)div[qi];
        for (i = 1; i < 4; i++)
        {
          fdividend *= NBASE;
          if (qi + i <= div_ndigits)
          {
            fdividend += (double)div[qi + i];
          }
        }
                                                      
        fquotient = fdividend * fdivisorinverse;
        qdigit = (fquotient >= 0.0) ? ((int)fquotient) : (((int)fquotient) - 1);                                 
        maxdiv += Abs(qdigit);
      }

         
                                                               
         
                                                                         
                                                                         
                                                                     
                                                                         
                                                                       
                                                                         
         
      if (qdigit != 0)
      {
        int istop = Min(var2ndigits, div_ndigits - qi + 1);

        for (i = 0; i < istop; i++)
        {
          div[qi + i] -= qdigit * var2digits[i];
        }
      }
    }

       
                                                                          
                                             
       
                                                                         
                                                                          
                                                                           
                                                                         
                                                                          
                                                                          
                                                                         
                                                                          
                                                                     
                                                                
       
                                                                        
                                                                       
                                                                          
                                                                         
                                                                         
                                                                           
       
    div[qi + 1] += div[qi] * NBASE;

    div[qi] = qdigit;
  }

     
                                                                      
     
  fdividend = (double)div[qi];
  for (i = 1; i < 4; i++)
  {
    fdividend *= NBASE;
  }
  fquotient = fdividend * fdivisorinverse;
  qdigit = (fquotient >= 0.0) ? ((int)fquotient) : (((int)fquotient) - 1);                                 
  div[qi] = qdigit;

     
                                                                            
                                                                       
                                                                         
                                                                             
                                                                         
                                    
     
  alloc_var(result, div_ndigits + 1);
  res_digits = result->digits;
  carry = 0;
  for (i = div_ndigits; i >= 0; i--)
  {
    newdig = div[i] + carry;
    if (newdig < 0)
    {
      carry = -((-newdig - 1) / NBASE) - 1;
      newdig -= carry * NBASE;
    }
    else if (newdig >= NBASE)
    {
      carry = newdig / NBASE;
      newdig -= carry * NBASE;
    }
    else
    {
      carry = 0;
    }
    res_digits[i] = newdig;
  }
  Assert(carry == 0);

  pfree(div);

     
                                                           
     
  result->weight = res_weight;
  result->sign = res_sign;

                                                       
  if (round)
  {
    round_var(result, rscale);
  }
  else
  {
    trunc_var(result, rscale);
  }

                                         
  strip_var(result);
}

   
                                        
   
                                                                 
   
static int
select_div_scale(const NumericVar *var1, const NumericVar *var2)
{
  int weight1, weight2, qweight, i;
  NumericDigit firstdigit1, firstdigit2;
  int rscale;

     
                                                                             
                                                                 
                                                                        
                                                                        
                                   
     

                                                                        

  weight1 = 0;                                    
  firstdigit1 = 0;
  for (i = 0; i < var1->ndigits; i++)
  {
    firstdigit1 = var1->digits[i];
    if (firstdigit1 != 0)
    {
      weight1 = var1->weight - i;
      break;
    }
  }

  weight2 = 0;                                    
  firstdigit2 = 0;
  for (i = 0; i < var2->ndigits; i++)
  {
    firstdigit2 = var2->digits[i];
    if (firstdigit2 != 0)
    {
      weight2 = var2->weight - i;
      break;
    }
  }

     
                                                                         
                                                            
     
  qweight = weight1 - weight2;
  if (firstdigit1 <= firstdigit2)
  {
    qweight--;
  }

                           
  rscale = NUMERIC_MIN_SIG_DIGITS - qweight * DEC_DIGITS;
  rscale = Max(rscale, var1->dscale);
  rscale = Max(rscale, var2->dscale);
  rscale = Max(rscale, NUMERIC_MIN_DISPLAY_SCALE);
  rscale = Min(rscale, NUMERIC_MAX_DISPLAY_SCALE);

  return rscale;
}

   
               
   
                                                          
   
static void
mod_var(const NumericVar *var1, const NumericVar *var2, NumericVar *result)
{
  NumericVar tmp;

  init_var(&tmp);

               
                                   
                                  
                                                              
                
     
  div_var(var1, var2, &tmp, 0, false);

  mul_var(var2, &tmp, &tmp, var2->dscale);

  sub_var(var1, &tmp, result);

  free_var(&tmp);
}

   
                
   
                                                                     
                     
   
static void
ceil_var(const NumericVar *var, NumericVar *result)
{
  NumericVar tmp;

  init_var(&tmp);
  set_var_from_var(var, &tmp);

  trunc_var(&tmp, 0);

  if (var->sign == NUMERIC_POS && cmp_var(var, &tmp) != 0)
  {
    add_var(&tmp, &const_one, &tmp);
  }

  set_var_from_var(&tmp, result);
  free_var(&tmp);
}

   
                 
   
                                                                 
                     
   
static void
floor_var(const NumericVar *var, NumericVar *result)
{
  NumericVar tmp;

  init_var(&tmp);
  set_var_from_var(var, &tmp);

  trunc_var(&tmp, 0);

  if (var->sign == NUMERIC_NEG && cmp_var(var, &tmp) != 0)
  {
    sub_var(&tmp, &const_one, &tmp);
  }

  set_var_from_var(&tmp, result);
  free_var(&tmp);
}

   
                
   
                                                         
   
static void
sqrt_var(const NumericVar *arg, NumericVar *result, int rscale)
{
  NumericVar tmp_arg;
  NumericVar tmp_val;
  NumericVar last_val;
  int local_rscale;
  int stat;

  local_rscale = rscale + 8;

  stat = cmp_var(arg, &const_zero);
  if (stat == 0)
  {
    zero_var(result);
    result->dscale = rscale;
    return;
  }

     
                                                                            
                                                     
     
  if (stat < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_POWER_FUNCTION), errmsg("cannot take square root of a negative number")));
  }

  init_var(&tmp_arg);
  init_var(&tmp_val);
  init_var(&last_val);

                                                     
  set_var_from_var(arg, &tmp_arg);

     
                                              
     
  alloc_var(result, 1);
  result->digits[0] = tmp_arg.digits[0] / 2;
  if (result->digits[0] == 0)
  {
    result->digits[0] = 1;
  }
  result->weight = tmp_arg.weight / 2;
  result->sign = NUMERIC_POS;

  set_var_from_var(result, &last_val);

  for (;;)
  {
    div_var_fast(&tmp_arg, result, &tmp_val, local_rscale, true);

    add_var(result, &tmp_val, result);
    mul_var(result, &const_zero_point_five, result, local_rscale);

    if (cmp_var(&last_val, result) == 0)
    {
      break;
    }
    set_var_from_var(result, &last_val);
  }

  free_var(&last_val);
  free_var(&tmp_val);
  free_var(&tmp_arg);

                                    
  round_var(result, rscale);
}

   
               
   
                                                                   
   
static void
exp_var(const NumericVar *arg, NumericVar *result, int rscale)
{
  NumericVar x;
  NumericVar elem;
  NumericVar ni;
  double val;
  int dweight;
  int ndiv2;
  int sig_digits;
  int local_rscale;

  init_var(&x);
  init_var(&elem);
  init_var(&ni);

  set_var_from_var(arg, &x);

     
                                                                            
                                                                         
     
  val = numericvar_to_double_no_overflow(&x);

                                        
                                                              
  if (Abs(val) >= NUMERIC_MAX_RESULT_SCALE * 3)
  {
    if (val > 0)
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value overflows numeric format")));
    }
    zero_var(result);
    result->dscale = rscale;
    return;
  }

                                                  
  dweight = (int)(val * 0.434294481903252);

     
                                                                             
                                                                
     
  if (Abs(val) > 0.01)
  {
    NumericVar tmp;

    init_var(&tmp);
    set_var_from_var(&const_two, &tmp);

    ndiv2 = 1;
    val /= 2;

    while (Abs(val) > 0.01)
    {
      ndiv2++;
      val /= 2;
      add_var(&tmp, &tmp, &tmp);
    }

    local_rscale = x.dscale + ndiv2;
    div_var_fast(&x, &tmp, &x, local_rscale, true);

    free_var(&tmp);
  }
  else
  {
    ndiv2 = 0;
  }

     
                                                                          
                                                                         
                                                                           
                                                                            
                                                                   
     
  sig_digits = 1 + dweight + rscale + (int)(ndiv2 * 0.301029995663981);
  sig_digits = Max(sig_digits, 0) + 8;

  local_rscale = sig_digits - 1;

     
                           
     
                                            
     
                                                                            
                                                                          
     
  add_var(&const_one, &x, result);

  mul_var(&x, &x, &elem, local_rscale);
  set_var_from_var(&const_two, &ni);
  div_var_fast(&elem, &ni, &elem, local_rscale, true);

  while (elem.ndigits != 0)
  {
    add_var(result, &elem, result);

    mul_var(&elem, &x, &elem, local_rscale);
    add_var(&ni, &const_one, &ni);
    div_var_fast(&elem, &ni, &elem, local_rscale, true);
  }

     
                                                                           
                                                                             
                    
     
  while (ndiv2-- > 0)
  {
    local_rscale = sig_digits - result->weight * 2 * DEC_DIGITS;
    local_rscale = Max(local_rscale, NUMERIC_MIN_DISPLAY_SCALE);
    mul_var(result, result, result, local_rscale);
  }

                                 
  round_var(result, rscale);

  free_var(&x);
  free_var(&elem);
  free_var(&ni);
}

   
                                                                             
                          
   
                                                                          
                                                                       
   
                                                                             
                                                                       
                                                                            
   
static int
estimate_ln_dweight(const NumericVar *var)
{
  int ln_dweight;

                                                                          
  if (var->sign != NUMERIC_POS)
  {
    return 0;
  }

  if (cmp_var(var, &const_zero_point_nine) >= 0 && cmp_var(var, &const_one_point_one) <= 0)
  {
       
                         
       
                                                                      
                                                                   
       
    NumericVar x;

    init_var(&x);
    sub_var(var, &const_one, &x);

    if (x.ndigits > 0)
    {
                                                             
      ln_dweight = x.weight * DEC_DIGITS + (int)log10(x.digits[0]);
    }
    else
    {
                                                                       
      ln_dweight = 0;
    }

    free_var(&x);
  }
  else
  {
       
                                                                        
                                                                           
                              
       
    if (var->ndigits > 0)
    {
      int digits;
      int dweight;
      double ln_var;

      digits = var->digits[0];
      dweight = var->weight * DEC_DIGITS;

      if (var->ndigits > 1)
      {
        digits = digits * NBASE + var->digits[1];
        dweight -= DEC_DIGITS;
      }

                   
                                            
                                                     
                   
         
      ln_var = log((double)digits) + dweight * 2.302585092994046;
      ln_dweight = (int)log10(Abs(ln_var));
    }
    else
    {
                                                                       
      ln_dweight = 0;
    }
  }

  return ln_dweight;
}

   
              
   
                                
   
static void
ln_var(const NumericVar *arg, NumericVar *result, int rscale)
{
  NumericVar x;
  NumericVar xx;
  NumericVar ni;
  NumericVar elem;
  NumericVar fact;
  int local_rscale;
  int cmp;

  cmp = cmp_var(arg, &const_zero);
  if (cmp == 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_LOG), errmsg("cannot take logarithm of zero")));
  }
  else if (cmp < 0)
  {
    ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_LOG), errmsg("cannot take logarithm of a negative number")));
  }

  init_var(&x);
  init_var(&xx);
  init_var(&ni);
  init_var(&elem);
  init_var(&fact);

  set_var_from_var(arg, &x);
  set_var_from_var(&const_two, &fact);

     
                                                                            
     
                                                                             
                                                                         
                                                                            
                                              
     
  while (cmp_var(&x, &const_zero_point_nine) <= 0)
  {
    local_rscale = rscale - x.weight * DEC_DIGITS / 2 + 8;
    local_rscale = Max(local_rscale, NUMERIC_MIN_DISPLAY_SCALE);
    sqrt_var(&x, &x, local_rscale);
    mul_var(&fact, &const_two, &fact, 0);
  }
  while (cmp_var(&x, &const_one_point_one) >= 0)
  {
    local_rscale = rscale - x.weight * DEC_DIGITS / 2 + 8;
    local_rscale = Max(local_rscale, NUMERIC_MIN_DISPLAY_SCALE);
    sqrt_var(&x, &x, local_rscale);
    mul_var(&fact, &const_two, &fact, 0);
  }

     
                                                         
     
                             
     
                                                                           
                                            
     
                                                                      
                                      
     
  local_rscale = rscale + 8;

  sub_var(&x, &const_one, result);
  add_var(&x, &const_one, &elem);
  div_var_fast(result, &elem, result, local_rscale, true);
  set_var_from_var(result, &xx);
  mul_var(result, result, &x, local_rscale);

  set_var_from_var(&const_one, &ni);

  for (;;)
  {
    add_var(&ni, &const_two, &ni);
    mul_var(&xx, &x, &xx, local_rscale);
    div_var_fast(&xx, &ni, &elem, local_rscale, true);

    if (elem.ndigits == 0)
    {
      break;
    }

    add_var(result, &elem, result);

    if (elem.weight < (result->weight - local_rscale * 2 / DEC_DIGITS))
    {
      break;
    }
  }

                                                                          
  mul_var(result, &fact, result, rscale);

  free_var(&x);
  free_var(&xx);
  free_var(&ni);
  free_var(&elem);
  free_var(&fact);
}

   
               
   
                                                 
   
                                                    
   
static void
log_var(const NumericVar *base, const NumericVar *num, NumericVar *result)
{
  NumericVar ln_base;
  NumericVar ln_num;
  int ln_base_dweight;
  int ln_num_dweight;
  int result_dweight;
  int rscale;
  int ln_base_rscale;
  int ln_num_rscale;

  init_var(&ln_base);
  init_var(&ln_num);

                                                                    
  ln_base_dweight = estimate_ln_dweight(base);
  ln_num_dweight = estimate_ln_dweight(num);
  result_dweight = ln_num_dweight - ln_base_dweight;

     
                                                                  
                                                                           
                            
     
  rscale = NUMERIC_MIN_SIG_DIGITS - result_dweight;
  rscale = Max(rscale, base->dscale);
  rscale = Max(rscale, num->dscale);
  rscale = Max(rscale, NUMERIC_MIN_DISPLAY_SCALE);
  rscale = Min(rscale, NUMERIC_MAX_DISPLAY_SCALE);

     
                                                                         
                                               
     
  ln_base_rscale = rscale + result_dweight - ln_base_dweight + 8;
  ln_base_rscale = Max(ln_base_rscale, NUMERIC_MIN_DISPLAY_SCALE);

  ln_num_rscale = rscale + result_dweight - ln_num_dweight + 8;
  ln_num_rscale = Max(ln_num_rscale, NUMERIC_MIN_DISPLAY_SCALE);

                               
  ln_var(base, &ln_base, ln_base_rscale);
  ln_var(num, &ln_num, ln_num_rscale);

                                              
  div_var_fast(&ln_num, &ln_base, result, rscale, true);

  free_var(&ln_num);
  free_var(&ln_base);
}

   
                 
   
                                  
   
                                                    
   
static void
power_var(const NumericVar *base, const NumericVar *exp, NumericVar *result)
{
  int res_sign;
  NumericVar abs_base;
  NumericVar ln_base;
  NumericVar ln_num;
  int ln_dweight;
  int rscale;
  int sig_digits;
  int local_rscale;
  double val;

                                                                  
  if (exp->ndigits == 0 || exp->ndigits <= exp->weight + 1)
  {
                                                
    int64 expval64;

    if (numericvar_to_int64(exp, &expval64))
    {
      if (expval64 >= PG_INT32_MIN && expval64 <= PG_INT32_MAX)
      {
                                 
        rscale = NUMERIC_MIN_SIG_DIGITS;
        rscale = Max(rscale, base->dscale);
        rscale = Max(rscale, NUMERIC_MIN_DISPLAY_SCALE);
        rscale = Min(rscale, NUMERIC_MAX_DISPLAY_SCALE);

        power_var_int(base, (int)expval64, result, rscale);
        return;
      }
    }
  }

     
                                                                          
                                 
     
  if (cmp_var(base, &const_zero) == 0)
  {
    set_var_from_var(&const_zero, result);
    result->dscale = NUMERIC_MIN_SIG_DIGITS;                       
    return;
  }

  init_var(&abs_base);
  init_var(&ln_base);
  init_var(&ln_num);

     
                                                                             
                                                         
     
  if (base->sign == NUMERIC_NEG)
  {
       
                                                                        
                                                                  
       
    if (exp->ndigits > 0 && exp->ndigits > exp->weight + 1)
    {
      ereport(ERROR, (errcode(ERRCODE_INVALID_ARGUMENT_FOR_POWER_FUNCTION), errmsg("a negative number raised to a non-integer power yields a complex result")));
    }

                                    
    if (exp->ndigits > 0 && exp->ndigits == exp->weight + 1 && (exp->digits[exp->ndigits - 1] & 1))
    {
      res_sign = NUMERIC_NEG;
    }
    else
    {
      res_sign = NUMERIC_POS;
    }

                                        
    set_var_from_var(base, &abs_base);
    abs_base.sign = NUMERIC_POS;
    base = &abs_base;
  }
  else
  {
    res_sign = NUMERIC_POS;
  }

               
                                                                        
                                                                       
                                                          
     
                                           
                                                                   
     
                                                                             
                                                                         
                                                                           
                                                                          
                                                                             
                                                                          
                                                                        
                                                                 
               
     
  ln_dweight = estimate_ln_dweight(base);

     
                                                                            
                                                                           
                                                                        
     
  local_rscale = 8 - ln_dweight;
  local_rscale = Max(local_rscale, NUMERIC_MIN_DISPLAY_SCALE);

  ln_var(base, &ln_base, local_rscale);

  mul_var(&ln_base, exp, &ln_num, local_rscale);

  val = numericvar_to_double_no_overflow(&ln_num);

                                                        
  if (Abs(val) > NUMERIC_MAX_RESULT_SCALE * 3.01)
  {
    if (val > 0)
    {
      ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value overflows numeric format")));
    }
    zero_var(result);
    result->dscale = NUMERIC_MAX_DISPLAY_SCALE;
    return;
  }

  val *= 0.434294481903252;                                        

                               
  rscale = NUMERIC_MIN_SIG_DIGITS - (int)val;
  rscale = Max(rscale, base->dscale);
  rscale = Max(rscale, exp->dscale);
  rscale = Max(rscale, NUMERIC_MIN_DISPLAY_SCALE);
  rscale = Min(rscale, NUMERIC_MAX_DISPLAY_SCALE);

                                                 
  sig_digits = rscale + (int)val;
  sig_digits = Max(sig_digits, 0);

                                                             
  local_rscale = sig_digits - ln_dweight + 8;
  local_rscale = Max(local_rscale, NUMERIC_MIN_DISPLAY_SCALE);

                                   

  ln_var(base, &ln_base, local_rscale);

  mul_var(&ln_base, exp, &ln_num, local_rscale);

  exp_var(&ln_num, result, rscale);

  if (res_sign == NUMERIC_NEG && result->ndigits > 0)
  {
    result->sign = NUMERIC_NEG;
  }

  free_var(&ln_num);
  free_var(&ln_base);
  free_var(&abs_base);
}

   
                     
   
                                                            
   
static void
power_var_int(const NumericVar *base, int exp, NumericVar *result, int rscale)
{
  double f;
  int p;
  int i;
  int sig_digits;
  unsigned int mask;
  bool neg;
  NumericVar base_prod;
  int local_rscale;

                                                                 
  switch (exp)
  {
  case 0:

       
                                                                      
                                                                    
                                          
                                                                           
       
    set_var_from_var(&const_one, result);
    result->dscale = rscale;                       
    return;
  case 1:
    set_var_from_var(base, result);
    round_var(result, rscale);
    return;
  case -1:
    div_var(&const_one, base, result, rscale, true);
    return;
  case 2:
    mul_var(base, base, result, rscale);
    return;
  default:
    break;
  }

                                                      
  if (base->ndigits == 0)
  {
    if (exp < 0)
    {
      ereport(ERROR, (errcode(ERRCODE_DIVISION_BY_ZERO), errmsg("division by zero")));
    }
    zero_var(result);
    result->dscale = rscale;
    return;
  }

     
                                                                      
                     
     
                                                                            
                                         
     
  f = base->digits[0];
  p = base->weight * DEC_DIGITS;

  for (i = 1; i < base->ndigits && i * DEC_DIGITS < 16; i++)
  {
    f = f * NBASE + base->digits[i];
    p -= DEC_DIGITS;
  }

               
                              
                                                                
               
     
  f = exp * (log10(f) + p);

     
                                                                             
                                        
     
  if (f > 3 * SHRT_MAX * DEC_DIGITS)
  {
    ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value overflows numeric format")));
  }
  if (f + 1 < -rscale || f + 1 < -NUMERIC_MAX_DISPLAY_SCALE)
  {
    zero_var(result);
    result->dscale = rscale;
    return;
  }

     
                                                                            
                                                               
     
  sig_digits = 1 + rscale + (int)f;

     
                                                                            
                                                                           
                                                      
     
  sig_digits += (int)log(fabs((double)exp)) + 8;

     
                                                  
     
  neg = (exp < 0);
  mask = Abs(exp);

  init_var(&base_prod);
  set_var_from_var(base, &base_prod);

  if (mask & 1)
  {
    set_var_from_var(base, result);
  }
  else
  {
    set_var_from_var(&const_one, result);
  }

  while ((mask >>= 1) > 0)
  {
       
                                                                     
                                                                       
                                                                     
       
    local_rscale = sig_digits - 2 * base_prod.weight * DEC_DIGITS;
    local_rscale = Min(local_rscale, 2 * base_prod.dscale);
    local_rscale = Max(local_rscale, NUMERIC_MIN_DISPLAY_SCALE);

    mul_var(&base_prod, &base_prod, &base_prod, local_rscale);

    if (mask & 1)
    {
      local_rscale = sig_digits - (base_prod.weight + result->weight) * DEC_DIGITS;
      local_rscale = Min(local_rscale, base_prod.dscale + result->dscale);
      local_rscale = Max(local_rscale, NUMERIC_MIN_DISPLAY_SCALE);

      mul_var(&base_prod, result, result, local_rscale);
    }

       
                                                                           
                                                                           
                                                                           
                                                                      
                                                                           
                                                                   
       
    if (base_prod.weight > SHRT_MAX || result->weight > SHRT_MAX)
    {
                                                                  
      if (!neg)
      {
        ereport(ERROR, (errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), errmsg("value overflows numeric format")));
      }
      zero_var(result);
      neg = false;
      break;
    }
  }

  free_var(&base_prod);

                                                                
  if (neg)
  {
    div_var_fast(&const_one, result, result, rscale, true);
  }
  else
  {
    round_var(result, rscale);
  }
}

   
                     
   
                                                                             
                                                                          
   
static void
power_ten_int(int exp, NumericVar *result)
{
                                                             
  set_var_from_var(&const_one, result);

                                                    
  result->dscale = exp < 0 ? -exp : 0;

                                                          
  if (exp >= 0)
  {
    result->weight = exp / DEC_DIGITS;
  }
  else
  {
    result->weight = (exp + 1) / DEC_DIGITS - 1;
  }

  exp -= result->weight * DEC_DIGITS;

                                                           
  while (exp-- > 0)
  {
    result->digits[0] *= 10;
  }
}

                                                                          
   
                                                                  
                         
   
                                                                          
   

              
               
   
                                                
                                         
                                    
                                   
              
   
static int
cmp_abs(const NumericVar *var1, const NumericVar *var2)
{
  return cmp_abs_common(var1->digits, var1->ndigits, var1->weight, var2->digits, var2->ndigits, var2->weight);
}

              
                      
   
                                                                
                           
              
   
static int
cmp_abs_common(const NumericDigit *var1digits, int var1ndigits, int var1weight, const NumericDigit *var2digits, int var2ndigits, int var2weight)
{
  int i1 = 0;
  int i2 = 0;

                                                      

  while (var1weight > var2weight && i1 < var1ndigits)
  {
    if (var1digits[i1++] != 0)
    {
      return 1;
    }
    var1weight--;
  }
  while (var2weight > var1weight && i2 < var2ndigits)
  {
    if (var2digits[i2++] != 0)
    {
      return -1;
    }
    var2weight--;
  }

                                                                 

  if (var1weight == var2weight)
  {
    while (i1 < var1ndigits && i2 < var2ndigits)
    {
      int stat = var1digits[i1++] - var2digits[i2++];

      if (stat)
      {
        if (stat > 0)
        {
          return 1;
        }
        return -1;
      }
    }
  }

     
                                                                             
                                                        
     
  while (i1 < var1ndigits)
  {
    if (var1digits[i1++] != 0)
    {
      return 1;
    }
  }
  while (i2 < var2ndigits)
  {
    if (var2digits[i2++] != 0)
    {
      return -1;
    }
  }

  return 0;
}

   
               
   
                                                         
                                                             
   
static void
add_abs(const NumericVar *var1, const NumericVar *var2, NumericVar *result)
{
  NumericDigit *res_buf;
  NumericDigit *res_digits;
  int res_ndigits;
  int res_weight;
  int res_rscale, rscale1, rscale2;
  int res_dscale;
  int i, i1, i2;
  int carry = 0;

                                                                 
  int var1ndigits = var1->ndigits;
  int var2ndigits = var2->ndigits;
  NumericDigit *var1digits = var1->digits;
  NumericDigit *var2digits = var2->digits;

  res_weight = Max(var1->weight, var2->weight) + 1;

  res_dscale = Max(var1->dscale, var2->dscale);

                                                              
  rscale1 = var1->ndigits - var1->weight - 1;
  rscale2 = var2->ndigits - var2->weight - 1;
  res_rscale = Max(rscale1, rscale2);

  res_ndigits = res_rscale + res_weight + 1;
  if (res_ndigits <= 0)
  {
    res_ndigits = 1;
  }

  res_buf = digitbuf_alloc(res_ndigits + 1);
  res_buf[0] = 0;                                     
  res_digits = res_buf + 1;

  i1 = res_rscale + var1->weight + 1;
  i2 = res_rscale + var2->weight + 1;
  for (i = res_ndigits - 1; i >= 0; i--)
  {
    i1--;
    i2--;
    if (i1 >= 0 && i1 < var1ndigits)
    {
      carry += var1digits[i1];
    }
    if (i2 >= 0 && i2 < var2ndigits)
    {
      carry += var2digits[i2];
    }

    if (carry >= NBASE)
    {
      res_digits[i] = carry - NBASE;
      carry = 1;
    }
    else
    {
      res_digits[i] = carry;
      carry = 0;
    }
  }

  Assert(carry == 0);                                            

  digitbuf_free(result->buf);
  result->ndigits = res_ndigits;
  result->buf = res_buf;
  result->digits = res_digits;
  result->weight = res_weight;
  result->dscale = res_dscale;

                                      
  strip_var(result);
}

   
             
   
                                                                       
                                                                  
                   
   
                                                    
   
static void
sub_abs(const NumericVar *var1, const NumericVar *var2, NumericVar *result)
{
  NumericDigit *res_buf;
  NumericDigit *res_digits;
  int res_ndigits;
  int res_weight;
  int res_rscale, rscale1, rscale2;
  int res_dscale;
  int i, i1, i2;
  int borrow = 0;

                                                                 
  int var1ndigits = var1->ndigits;
  int var2ndigits = var2->ndigits;
  NumericDigit *var1digits = var1->digits;
  NumericDigit *var2digits = var2->digits;

  res_weight = var1->weight;

  res_dscale = Max(var1->dscale, var2->dscale);

                                                              
  rscale1 = var1->ndigits - var1->weight - 1;
  rscale2 = var2->ndigits - var2->weight - 1;
  res_rscale = Max(rscale1, rscale2);

  res_ndigits = res_rscale + res_weight + 1;
  if (res_ndigits <= 0)
  {
    res_ndigits = 1;
  }

  res_buf = digitbuf_alloc(res_ndigits + 1);
  res_buf[0] = 0;                                     
  res_digits = res_buf + 1;

  i1 = res_rscale + var1->weight + 1;
  i2 = res_rscale + var2->weight + 1;
  for (i = res_ndigits - 1; i >= 0; i--)
  {
    i1--;
    i2--;
    if (i1 >= 0 && i1 < var1ndigits)
    {
      borrow += var1digits[i1];
    }
    if (i2 >= 0 && i2 < var2ndigits)
    {
      borrow -= var2digits[i2];
    }

    if (borrow < 0)
    {
      res_digits[i] = borrow + NBASE;
      borrow = -1;
    }
    else
    {
      res_digits[i] = borrow;
      borrow = 0;
    }
  }

  Assert(borrow == 0);                                      

  digitbuf_free(result->buf);
  result->ndigits = res_ndigits;
  result->buf = res_buf;
  result->digits = res_digits;
  result->weight = res_weight;
  result->dscale = res_dscale;

                                      
  strip_var(result);
}

   
             
   
                                                                       
                                                                      
                                      
   
static void
round_var(NumericVar *var, int rscale)
{
  NumericDigit *digits = var->digits;
  int di;
  int ndigits;
  int carry;

  var->dscale = rscale;

                             
  di = (var->weight + 1) * DEC_DIGITS + rscale;

     
                                                                           
                                                                 
     
  if (di < 0)
  {
    var->ndigits = 0;
    var->weight = 0;
    var->sign = NUMERIC_POS;
  }
  else
  {
                             
    ndigits = (di + DEC_DIGITS - 1) / DEC_DIGITS;

                                                                    
    di %= DEC_DIGITS;

    if (ndigits < var->ndigits || (ndigits == var->ndigits && di > 0))
    {
      var->ndigits = ndigits;

#if DEC_DIGITS == 1
                           
      carry = (digits[ndigits] >= HALF_NBASE) ? 1 : 0;
#else
      if (di == 0)
      {
        carry = (digits[ndigits] >= HALF_NBASE) ? 1 : 0;
      }
      else
      {
                                                
        int extra, pow10;

#if DEC_DIGITS == 4
        pow10 = round_powers[di];
#elif DEC_DIGITS == 2
        pow10 = 10;
#else
#error unsupported NBASE
#endif
        extra = digits[--ndigits] % pow10;
        digits[ndigits] -= extra;
        carry = 0;
        if (extra >= pow10 / 2)
        {
          pow10 += digits[ndigits];
          if (pow10 >= NBASE)
          {
            pow10 -= NBASE;
            carry = 1;
          }
          digits[ndigits] = pow10;
        }
      }
#endif

                                     
      while (carry)
      {
        carry += digits[--ndigits];
        if (carry >= NBASE)
        {
          digits[ndigits] = carry - NBASE;
          carry = 1;
        }
        else
        {
          digits[ndigits] = carry;
          carry = 0;
        }
      }

      if (ndigits < 0)
      {
        Assert(ndigits == -1);                                      
        Assert(var->digits > var->buf);
        var->digits--;
        var->ndigits++;
        var->weight++;
      }
    }
  }
}

   
             
   
                                                                            
                                                                      
                                        
   
static void
trunc_var(NumericVar *var, int rscale)
{
  int di;
  int ndigits;

  var->dscale = rscale;

                             
  di = (var->weight + 1) * DEC_DIGITS + rscale;

     
                                             
     
  if (di <= 0)
  {
    var->ndigits = 0;
    var->weight = 0;
    var->sign = NUMERIC_POS;
  }
  else
  {
                             
    ndigits = (di + DEC_DIGITS - 1) / DEC_DIGITS;

    if (ndigits <= var->ndigits)
    {
      var->ndigits = ndigits;

#if DEC_DIGITS == 1
                                                
#else
                                                                      
      di %= DEC_DIGITS;

      if (di > 0)
      {
                                                   
        NumericDigit *digits = var->digits;
        int extra, pow10;

#if DEC_DIGITS == 4
        pow10 = round_powers[di];
#elif DEC_DIGITS == 2
        pow10 = 10;
#else
#error unsupported NBASE
#endif
        extra = digits[--ndigits] % pow10;
        digits[ndigits] -= extra;
      }
#endif
    }
  }
}

   
             
   
                                                                 
   
static void
strip_var(NumericVar *var)
{
  NumericDigit *digits = var->digits;
  int ndigits = var->ndigits;

                            
  while (ndigits > 0 && *digits == 0)
  {
    digits++;
    var->weight--;
    ndigits--;
  }

                             
  while (ndigits > 0 && digits[ndigits - 1] == 0)
  {
    ndigits--;
  }

                                                   
  if (ndigits == 0)
  {
    var->sign = NUMERIC_POS;
    var->weight = 0;
  }

  var->digits = digits;
  var->ndigits = ndigits;
}

                                                                          
   
                                  
   
                                                                          
   

   
                                                                          
                   
   
static void
accum_sum_reset(NumericSumAccum *accum)
{
  int i;

  accum->dscale = 0;
  for (i = 0; i < accum->ndigits; i++)
  {
    accum->pos_digits[i] = 0;
    accum->neg_digits[i] = 0;
  }
}

   
                           
   
static void
accum_sum_add(NumericSumAccum *accum, const NumericVar *val)
{
  int32 *accum_digits;
  int i, val_i;
  int val_ndigits;
  NumericDigit *val_digits;

     
                                                                 
                                                                         
                                                                           
                                                                           
                                                             
     
  if (accum->num_uncarried == NBASE - 1)
  {
    accum_sum_carry(accum);
  }

     
                                                                             
                    
     
  accum_sum_rescale(accum, val);

       
  if (val->sign == NUMERIC_POS)
  {
    accum_digits = accum->pos_digits;
  }
  else
  {
    accum_digits = accum->neg_digits;
  }

                                                           
  val_ndigits = val->ndigits;
  val_digits = val->digits;

  i = accum->weight - val->weight;
  for (val_i = 0; val_i < val_ndigits; val_i++)
  {
    accum_digits[i] += (int32)val_digits[val_i];
    i++;
  }

  accum->num_uncarried++;
}

   
                      
   
static void
accum_sum_carry(NumericSumAccum *accum)
{
  int i;
  int ndigits;
  int32 *dig;
  int32 carry;
  int32 newdig = 0;

     
                                                                            
            
     
  if (accum->num_uncarried == 0)
  {
    return;
  }

     
                                                                         
                                                                          
                                                                      
                                                                      
                                                                         
     
  Assert(accum->pos_digits[0] == 0 && accum->neg_digits[0] == 0);

  ndigits = accum->ndigits;

                                           
  dig = accum->pos_digits;
  carry = 0;
  for (i = ndigits - 1; i >= 0; i--)
  {
    newdig = dig[i] + carry;
    if (newdig >= NBASE)
    {
      carry = newdig / NBASE;
      newdig -= carry * NBASE;
    }
    else
    {
      carry = 0;
    }
    dig[i] = newdig;
  }
                                                               
  if (newdig > 0)
  {
    accum->have_carry_space = false;
  }

                                         
  dig = accum->neg_digits;
  carry = 0;
  for (i = ndigits - 1; i >= 0; i--)
  {
    newdig = dig[i] + carry;
    if (newdig >= NBASE)
    {
      carry = newdig / NBASE;
      newdig -= carry * NBASE;
    }
    else
    {
      carry = 0;
    }
    dig[i] = newdig;
  }
  if (newdig > 0)
  {
    accum->have_carry_space = false;
  }

  accum->num_uncarried = 0;
}

   
                                                  
   
                                                                          
                                     
   
static void
accum_sum_rescale(NumericSumAccum *accum, const NumericVar *val)
{
  int old_weight = accum->weight;
  int old_ndigits = accum->ndigits;
  int accum_ndigits;
  int accum_weight;
  int accum_rscale;
  int val_rscale;

  accum_weight = old_weight;
  accum_ndigits = old_ndigits;

     
                                                                          
                                                                       
            
     
                                                                         
                                                                            
                                                  
     
  if (val->weight >= accum_weight)
  {
    accum_weight = val->weight + 1;
    accum_ndigits = accum_ndigits + (accum_weight - old_weight);
  }

     
                                                                       
                                                                             
                                               
     
  else if (!accum->have_carry_space)
  {
    accum_weight++;
    accum_ndigits++;
  }

                                                 
  accum_rscale = accum_ndigits - accum_weight - 1;
  val_rscale = val->ndigits - val->weight - 1;
  if (val_rscale > accum_rscale)
  {
    accum_ndigits = accum_ndigits + (val_rscale - accum_rscale);
  }

  if (accum_ndigits != old_ndigits || accum_weight != old_weight)
  {
    int32 *new_pos_digits;
    int32 *new_neg_digits;
    int weightdiff;

    weightdiff = accum_weight - old_weight;

    new_pos_digits = palloc0(accum_ndigits * sizeof(int32));
    new_neg_digits = palloc0(accum_ndigits * sizeof(int32));

    if (accum->pos_digits)
    {
      memcpy(&new_pos_digits[weightdiff], accum->pos_digits, old_ndigits * sizeof(int32));
      pfree(accum->pos_digits);

      memcpy(&new_neg_digits[weightdiff], accum->neg_digits, old_ndigits * sizeof(int32));
      pfree(accum->neg_digits);
    }

    accum->pos_digits = new_pos_digits;
    accum->neg_digits = new_neg_digits;

    accum->weight = accum_weight;
    accum->ndigits = accum_ndigits;

    Assert(accum->pos_digits[0] == 0 && accum->neg_digits[0] == 0);
    accum->have_carry_space = true;
  }

  if (val->dscale > accum->dscale)
  {
    accum->dscale = val->dscale;
  }
}

   
                                                                          
                                                                  
   
                                                                          
                                                  
   
static void
accum_sum_final(NumericSumAccum *accum, NumericVar *result)
{
  int i;
  NumericVar pos_var;
  NumericVar neg_var;

  if (accum->ndigits == 0)
  {
    set_var_from_var(&const_zero, result);
    return;
  }

                           
  accum_sum_carry(accum);

                                                                      
  init_var(&pos_var);
  init_var(&neg_var);

  pos_var.ndigits = neg_var.ndigits = accum->ndigits;
  pos_var.weight = neg_var.weight = accum->weight;
  pos_var.dscale = neg_var.dscale = accum->dscale;
  pos_var.sign = NUMERIC_POS;
  neg_var.sign = NUMERIC_NEG;

  pos_var.buf = pos_var.digits = digitbuf_alloc(accum->ndigits);
  neg_var.buf = neg_var.digits = digitbuf_alloc(accum->ndigits);

  for (i = 0; i < accum->ndigits; i++)
  {
    Assert(accum->pos_digits[i] < NBASE);
    pos_var.digits[i] = (int16)accum->pos_digits[i];

    Assert(accum->neg_digits[i] < NBASE);
    neg_var.digits[i] = (int16)accum->neg_digits[i];
  }

                             
  add_var(&pos_var, &neg_var, result);

                                      
  strip_var(result);
}

   
                                
   
                                                                           
                       
   
static void
accum_sum_copy(NumericSumAccum *dst, NumericSumAccum *src)
{
  dst->pos_digits = palloc(src->ndigits * sizeof(int32));
  dst->neg_digits = palloc(src->ndigits * sizeof(int32));

  memcpy(dst->pos_digits, src->pos_digits, src->ndigits * sizeof(int32));
  memcpy(dst->neg_digits, src->neg_digits, src->ndigits * sizeof(int32));
  dst->num_uncarried = src->num_uncarried;
  dst->ndigits = src->ndigits;
  dst->weight = src->weight;
  dst->dscale = src->dscale;
}

   
                                                   
   
static void
accum_sum_combine(NumericSumAccum *accum, NumericSumAccum *accum2)
{
  NumericVar tmp_var;

  init_var(&tmp_var);

  accum_sum_final(accum2, &tmp_var);
  accum_sum_add(accum, &tmp_var);

  free_var(&tmp_var);
}
