                                                                              
   
                                                   
   
                                                                         
   
                  
                      
   
                                                                              
                                                                               
                   
   
                            
   
                                                                       
                         
   
                                                        
                                                    
   
                                                                               
                                        
   
                                                       
                                               
   
                                                                               
                                                                            
                                    
   
                                                                              
   

   
                              
   
                                                                            
                                    
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "common/shortest_dec.h"

   
                                                                            
                                                                       
                                            
   
#if !defined(HAVE_INT128) && defined(_MSC_VER) && !defined(RYU_ONLY_64_BIT_OPS) && defined(_M_X64)
#define HAS_64_BIT_INTRINSICS
#endif

#include "ryu_common.h"
#include "digit_table.h"
#include "d2s_full_table.h"
#include "d2s_intrinsics.h"

#define DOUBLE_MANTISSA_BITS 52
#define DOUBLE_EXPONENT_BITS 11
#define DOUBLE_BIAS 1023

#define DOUBLE_POW5_INV_BITCOUNT 122
#define DOUBLE_POW5_BITCOUNT 121

static inline uint32
pow5Factor(uint64 value)
{
  uint32 count = 0;

  for (;;)
  {
    Assert(value != 0);
    const uint64 q = div5(value);
    const uint32 r = (uint32)(value - 5 * q);

    if (r != 0)
    {
      break;
    }

    value = q;
    ++count;
  }
  return count;
}

                                                 
static inline bool
multipleOfPowerOf5(const uint64 value, const uint32 p)
{
     
                                                                   
                 
     
  return pow5Factor(value) >= p;
}

                                                 
static inline bool
multipleOfPowerOf2(const uint64 value, const uint32 p)
{
                                           
  return (value & ((UINT64CONST(1) << p) - 1)) == 0;
}

   
                                                                       
   
                   
   
                                                                            
                                                                      
                                                                         
                                                                         
              
   
          
   
                                                                              
                                                                               
                                                                             
                                                                               
                                     
   
                                      
   
                                                       
                                                                               
                                                                    
   
                                                                           
                                                                        
                                                                             
                                                                             
                       
   
                                                                        
                                             
   
                                                                            
                                                 
   
                                                                          
                                                                       
                                                                      
                                                                          
                         
                                                                     
                                                                             
                                                          
   
#if defined(HAVE_INT128)

                                   
static inline uint64
mulShift(const uint64 m, const uint64 *const mul, const int32 j)
{
  const uint128 b0 = ((uint128)m) * mul[0];
  const uint128 b2 = ((uint128)m) * mul[1];

  return (uint64)(((b0 >> 64) + b2) >> (j - 64));
}

static inline uint64
mulShiftAll(const uint64 m, const uint64 *const mul, const int32 j, uint64 *const vp, uint64 *const vm, const uint32 mmShift)
{
  *vp = mulShift(4 * m + 2, mul, j);
  *vm = mulShift(4 * m - 1 - mmShift, mul, j);
  return mulShift(4 * m, mul, j);
}

#elif defined(HAS_64_BIT_INTRINSICS)

static inline uint64
mulShift(const uint64 m, const uint64 *const mul, const int32 j)
{
                            
  uint64 high1;

           
  const uint64 low1 = umul128(m, mul[1], &high1);

          
  uint64 high0;
  uint64 sum;

          
  umul128(m, mul[0], &high0);
         
  sum = high0 + low1;

  if (sum < high0)
  {
    ++high1;
                             
  }
  return shiftright128(sum, high1, j - 64);
}

static inline uint64
mulShiftAll(const uint64 m, const uint64 *const mul, const int32 j, uint64 *const vp, uint64 *const vm, const uint32 mmShift)
{
  *vp = mulShift(4 * m + 2, mul, j);
  *vm = mulShift(4 * m - 1 - mmShift, mul, j);
  return mulShift(4 * m, mul, j);
}

#else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
                                           

static inline uint64
mulShiftAll(uint64 m, const uint64 *const mul, const int32 j, uint64 *const vp, uint64 *const vm, const uint32 mmShift)
{
  m <<= 1;                           

  uint64 tmp;
  const uint64 lo = umul128(m, mul[0], &tmp);
  uint64 hi;
  const uint64 mid = tmp + umul128(m, mul[1], &hi);

  hi += mid < tmp;                       

  const uint64 lo2 = lo + mul[0];
  const uint64 mid2 = mid + mul[1] + (lo2 < lo);
  const uint64 hi2 = hi + (mid2 < mid);

  *vp = shiftright128(mid2, hi2, j - 64 - 1);

  if (mmShift == 1)
  {
    const uint64 lo3 = lo - mul[0];
    const uint64 mid3 = mid - mul[1] - (lo3 > lo);
    const uint64 hi3 = hi - (mid3 > mid);

    *vm = shiftright128(mid3, hi3, j - 64 - 1);
  }
  else
  {
    const uint64 lo3 = lo + lo;
    const uint64 mid3 = mid + mid + (lo3 < lo);
    const uint64 hi3 = hi + hi + (mid3 < mid);
    const uint64 lo4 = lo3 - mul[0];
    const uint64 mid4 = mid3 - mul[1] - (lo4 > lo3);
    const uint64 hi4 = hi3 - (mid4 > mid3);

    *vm = shiftright128(mid4, hi4, j - 64);
  }

  return shiftright128(mid, hi, j - 64 - 1);
}

#endif                               

static inline uint32
decimalLength(const uint64 v)
{
                                            
                                                                           
                                                                      
                                                      
  Assert(v < 100000000000000000L);
  if (v >= 10000000000000000L)
  {
    return 17;
  }
  if (v >= 1000000000000000L)
  {
    return 16;
  }
  if (v >= 100000000000000L)
  {
    return 15;
  }
  if (v >= 10000000000000L)
  {
    return 14;
  }
  if (v >= 1000000000000L)
  {
    return 13;
  }
  if (v >= 100000000000L)
  {
    return 12;
  }
  if (v >= 10000000000L)
  {
    return 11;
  }
  if (v >= 1000000000L)
  {
    return 10;
  }
  if (v >= 100000000L)
  {
    return 9;
  }
  if (v >= 10000000L)
  {
    return 8;
  }
  if (v >= 1000000L)
  {
    return 7;
  }
  if (v >= 100000L)
  {
    return 6;
  }
  if (v >= 10000L)
  {
    return 5;
  }
  if (v >= 1000L)
  {
    return 4;
  }
  if (v >= 100L)
  {
    return 3;
  }
  if (v >= 10L)
  {
    return 2;
  }
  return 1;
}

                                                
typedef struct floating_decimal_64
{
  uint64 mantissa;
  int32 exponent;
} floating_decimal_64;

static inline floating_decimal_64
d2d(const uint64 ieeeMantissa, const uint32 ieeeExponent)
{
  int32 e2;
  uint64 m2;

  if (ieeeExponent == 0)
  {
                                                                             
    e2 = 1 - DOUBLE_BIAS - DOUBLE_MANTISSA_BITS - 2;
    m2 = ieeeMantissa;
  }
  else
  {
    e2 = ieeeExponent - DOUBLE_BIAS - DOUBLE_MANTISSA_BITS - 2;
    m2 = (UINT64CONST(1) << DOUBLE_MANTISSA_BITS) | ieeeMantissa;
  }

#if STRICTLY_SHORTEST
  const bool even = (m2 & 1) == 0;
  const bool acceptBounds = even;
#else
  const bool acceptBounds = false;
#endif

                                                                        
  const uint64 mv = 4 * m2;

                                                               
  const uint32 mmShift = ieeeMantissa != 0 || ieeeExponent <= 1;

                                             
                               
                                     

                                                                         
  uint64 vr, vp, vm;
  int32 e10;
  bool vmIsTrailingZeros = false;
  bool vrIsTrailingZeros = false;

  if (e2 >= 0)
  {
       
                                                                 
                    
       
                                                                    
       
    const uint32 q = log10Pow2(e2) - (e2 > 3);
    const int32 k = DOUBLE_POW5_INV_BITCOUNT + pow5bits(q) - 1;
    const int32 i = -e2 + q + k;

    e10 = q;

    vr = mulShiftAll(m2, DOUBLE_POW5_INV_SPLIT[q], i, &vp, &vm, mmShift);

    if (q <= 21)
    {
         
                                                                       
                                                                     
                     
         
                                                                    
         
      const uint32 mvMod5 = (uint32)(mv - 5 * div5(mv));

      if (mvMod5 == 0)
      {
        vrIsTrailingZeros = multipleOfPowerOf5(mv, q);
      }
      else if (acceptBounds)
      {
               
                                                            
                                                          
                                                           
               
           
        vmIsTrailingZeros = multipleOfPowerOf5(mv - 1 - mmShift, q);
      }
      else
      {
                                                       
        vp -= multipleOfPowerOf5(mv + 2, q);
      }
    }
  }
  else
  {
       
                                                                           
       
    const uint32 q = log10Pow5(-e2) - (-e2 > 1);
    const int32 i = -e2 - q;
    const int32 k = pow5bits(i) - DOUBLE_POW5_BITCOUNT;
    const int32 j = q - k;

    e10 = q + e2;

    vr = mulShiftAll(m2, DOUBLE_POW5_SPLIT[i], j, &vp, &vm, mmShift);

    if (q <= 1)
    {
         
                                                                   
                          
         
                                                                       
      vrIsTrailingZeros = true;
      if (acceptBounds)
      {
           
                                                                 
                         
           
        vmIsTrailingZeros = mmShift == 1;
      }
      else
      {
           
                                                                      
           
        --vp;
      }
    }
    else if (q < 63)
    {
                                                   
         
                                                                       
         
                                                                
                                                              
                                                

         
                                                                
                   
         
      vrIsTrailingZeros = multipleOfPowerOf2(mv, q - 1);
    }
  }

     
                                                                         
                            
     
  uint32 removed = 0;
  uint8 lastRemovedDigit = 0;
  uint64 output;

                                        
  if (vmIsTrailingZeros || vrIsTrailingZeros)
  {
                                                     
    for (;;)
    {
      const uint64 vpDiv10 = div10(vp);
      const uint64 vmDiv10 = div10(vm);

      if (vpDiv10 <= vmDiv10)
      {
        break;
      }

      const uint32 vmMod10 = (uint32)(vm - 10 * vmDiv10);
      const uint64 vrDiv10 = div10(vr);
      const uint32 vrMod10 = (uint32)(vr - 10 * vrDiv10);

      vmIsTrailingZeros &= vmMod10 == 0;
      vrIsTrailingZeros &= lastRemovedDigit == 0;
      lastRemovedDigit = (uint8)vrMod10;
      vr = vrDiv10;
      vp = vpDiv10;
      vm = vmDiv10;
      ++removed;
    }

    if (vmIsTrailingZeros)
    {
      for (;;)
      {
        const uint64 vmDiv10 = div10(vm);
        const uint32 vmMod10 = (uint32)(vm - 10 * vmDiv10);

        if (vmMod10 != 0)
        {
          break;
        }

        const uint64 vpDiv10 = div10(vp);
        const uint64 vrDiv10 = div10(vr);
        const uint32 vrMod10 = (uint32)(vr - 10 * vrDiv10);

        vrIsTrailingZeros &= lastRemovedDigit == 0;
        lastRemovedDigit = (uint8)vrMod10;
        vr = vrDiv10;
        vp = vpDiv10;
        vm = vmDiv10;
        ++removed;
      }
    }

    if (vrIsTrailingZeros && lastRemovedDigit == 5 && vr % 2 == 0)
    {
                                                         
      lastRemovedDigit = 4;
    }

       
                                                                          
           
       
    output = vr + ((vr == vm && (!acceptBounds || !vmIsTrailingZeros)) || lastRemovedDigit >= 5);
  }
  else
  {
       
                                                                       
                         
       
    bool roundUp = false;
    const uint64 vpDiv100 = div100(vp);
    const uint64 vmDiv100 = div100(vm);

    if (vpDiv100 > vmDiv100)
    {
                                                              
      const uint64 vrDiv100 = div100(vr);
      const uint32 vrMod100 = (uint32)(vr - 100 * vrDiv100);

      roundUp = vrMod100 >= 50;
      vr = vrDiv100;
      vp = vpDiv100;
      vm = vmDiv100;
      removed += 2;
    }

           
                                                                   
              
       
                                                                   
                 
       
                                                                
              
       
                                                         
           
       
    for (;;)
    {
      const uint64 vpDiv10 = div10(vp);
      const uint64 vmDiv10 = div10(vm);

      if (vpDiv10 <= vmDiv10)
      {
        break;
      }

      const uint64 vrDiv10 = div10(vr);
      const uint32 vrMod10 = (uint32)(vr - 10 * vrDiv10);

      roundUp = vrMod10 >= 5;
      vr = vrDiv10;
      vp = vpDiv10;
      vm = vmDiv10;
      ++removed;
    }

       
                                                                          
           
       
    output = vr + (vr == vm || roundUp);
  }

  const int32 exp = e10 + removed;

  floating_decimal_64 fd;

  fd.exponent = exp;
  fd.mantissa = output;
  return fd;
}

static inline int
to_chars_df(const floating_decimal_64 v, const uint32 olength, char *const result)
{
                                                 
  int index = 0;

  uint64 output = v.mantissa;
  int32 exp = v.exponent;

         
                                                             
                                                   
     
                                                                          
                                                  
     
                              
                                        
                                  
                                                  
                                                   
     
  uint32 i = 0;
  int32 nexp = exp + olength;

  if (nexp <= 0)
  {
                                                
    Assert(nexp >= -3);
                    
    index = 2 - nexp;
                                           
    memcpy(result, "0.000000", 8);
  }
  else if (exp < 0)
  {
       
                                                                     
       
    index = 1;
  }
  else
  {
       
                                                                           
                                                                          
                                               
       
    Assert(exp < 16 && exp + olength <= 16);
    memset(result, '0', 16);
  }

     
                                                                            
                                                                          
                                                                    
     
  if ((output >> 32) != 0)
  {
                                    
    const uint64 q = div1e8(output);
    uint32 output2 = (uint32)(output - 100000000 * q);
    const uint32 c = output2 % 10000;

    output = q;
    output2 /= 10000;

    const uint32 d = output2 % 10000;
    const uint32 c0 = (c % 100) << 1;
    const uint32 c1 = (c / 100) << 1;
    const uint32 d0 = (d % 100) << 1;
    const uint32 d1 = (d / 100) << 1;

    memcpy(result + index + olength - i - 2, DIGIT_TABLE + c0, 2);
    memcpy(result + index + olength - i - 4, DIGIT_TABLE + c1, 2);
    memcpy(result + index + olength - i - 6, DIGIT_TABLE + d0, 2);
    memcpy(result + index + olength - i - 8, DIGIT_TABLE + d1, 2);
    i += 8;
  }

  uint32 output2 = (uint32)output;

  while (output2 >= 10000)
  {
    const uint32 c = output2 - 10000 * (output2 / 10000);
    const uint32 c0 = (c % 100) << 1;
    const uint32 c1 = (c / 100) << 1;

    output2 /= 10000;
    memcpy(result + index + olength - i - 2, DIGIT_TABLE + c0, 2);
    memcpy(result + index + olength - i - 4, DIGIT_TABLE + c1, 2);
    i += 4;
  }
  if (output2 >= 100)
  {
    const uint32 c = (output2 % 100) << 1;

    output2 /= 100;
    memcpy(result + index + olength - i - 2, DIGIT_TABLE + c, 2);
    i += 2;
  }
  if (output2 >= 10)
  {
    const uint32 c = output2 << 1;

    memcpy(result + index + olength - i - 2, DIGIT_TABLE + c, 2);
  }
  else
  {
    result[index] = (char)('0' + output2);
  }

  if (index == 1)
  {
       
                                                                        
                                                                 
                                                                 
       
    Assert(nexp < 16);
                                                                  
    if (nexp & 8)
    {
      memmove(result + index - 1, result + index, 8);
      index += 8;
    }
    if (nexp & 4)
    {
      memmove(result + index - 1, result + index, 4);
      index += 4;
    }
    if (nexp & 2)
    {
      memmove(result + index - 1, result + index, 2);
      index += 2;
    }
    if (nexp & 1)
    {
      result[index - 1] = result[index];
    }
    result[nexp] = '.';
    index = olength + 1;
  }
  else if (exp >= 0)
  {
                                                                          
    index = olength + exp;
  }
  else
  {
    index = olength + (2 - nexp);
  }

  return index;
}

static inline int
to_chars(floating_decimal_64 v, const bool sign, char *const result)
{
                                                 
  int index = 0;

  uint64 output = v.mantissa;
  uint32 olength = decimalLength(output);
  int32 exp = v.exponent + olength - 1;

  if (sign)
  {
    result[index++] = '-';
  }

     
                                                                      
                                                                         
                                                                    
     
  if (exp >= -4 && exp < 15)
  {
    return to_chars_df(v, olength, result + index) + sign;
  }

     
                                                                          
                                                                        
                                                                          
                                                                           
                                         
     
                                                                           
                                                                          
                                                            
     
                                                                        
                                                                          
                                                                      
                                                           
     
  if (v.exponent == 0)
  {
    while ((output & 1) == 0)
    {
      const uint64 q = div10(output);
      const uint32 r = (uint32)(output - 10 * q);

      if (r != 0)
      {
        break;
      }
      output = q;
      --olength;
    }
  }

         
                               
     
                                          
     
                                                
                                                   
                                                       
       
                                        
         
     

  uint32 i = 0;

     
                                                                            
                                                                          
                                                                    
     
  if ((output >> 32) != 0)
  {
                                    
    const uint64 q = div1e8(output);
    uint32 output2 = (uint32)(output - 100000000 * q);

    output = q;

    const uint32 c = output2 % 10000;

    output2 /= 10000;

    const uint32 d = output2 % 10000;
    const uint32 c0 = (c % 100) << 1;
    const uint32 c1 = (c / 100) << 1;
    const uint32 d0 = (d % 100) << 1;
    const uint32 d1 = (d / 100) << 1;

    memcpy(result + index + olength - i - 1, DIGIT_TABLE + c0, 2);
    memcpy(result + index + olength - i - 3, DIGIT_TABLE + c1, 2);
    memcpy(result + index + olength - i - 5, DIGIT_TABLE + d0, 2);
    memcpy(result + index + olength - i - 7, DIGIT_TABLE + d1, 2);
    i += 8;
  }

  uint32 output2 = (uint32)output;

  while (output2 >= 10000)
  {
    const uint32 c = output2 - 10000 * (output2 / 10000);

    output2 /= 10000;

    const uint32 c0 = (c % 100) << 1;
    const uint32 c1 = (c / 100) << 1;

    memcpy(result + index + olength - i - 1, DIGIT_TABLE + c0, 2);
    memcpy(result + index + olength - i - 3, DIGIT_TABLE + c1, 2);
    i += 4;
  }
  if (output2 >= 100)
  {
    const uint32 c = (output2 % 100) << 1;

    output2 /= 100;
    memcpy(result + index + olength - i - 1, DIGIT_TABLE + c, 2);
    i += 2;
  }
  if (output2 >= 10)
  {
    const uint32 c = output2 << 1;

       
                                                                        
               
       
    result[index + olength - i] = DIGIT_TABLE[c + 1];
    result[index] = DIGIT_TABLE[c];
  }
  else
  {
    result[index] = (char)('0' + output2);
  }

                                      
  if (olength > 1)
  {
    result[index + 1] = '.';
    index += olength + 1;
  }
  else
  {
    ++index;
  }

                           
  result[index++] = 'e';
  if (exp < 0)
  {
    result[index++] = '-';
    exp = -exp;
  }
  else
  {
    result[index++] = '+';
  }

  if (exp >= 100)
  {
    const int32 c = exp % 10;

    memcpy(result + index, DIGIT_TABLE + 2 * (exp / 10), 2);
    result[index + 2] = (char)('0' + c);
    index += 3;
  }
  else
  {
    memcpy(result + index, DIGIT_TABLE + 2 * exp, 2);
    index += 2;
  }

  return index;
}

static inline bool
d2d_small_int(const uint64 ieeeMantissa, const uint32 ieeeExponent, floating_decimal_64 *v)
{
  const int32 e2 = (int32)ieeeExponent - DOUBLE_BIAS - DOUBLE_MANTISSA_BITS;

     
                                                                             
                                                                          
     

  if (e2 >= -DOUBLE_MANTISSA_BITS && e2 <= 0)
  {
           
                                                   
                                     
       
                                                                         
                                                                          
                                                                         
                                                                    
                                                                 
       
    const uint64 mask = (UINT64CONST(1) << -e2) - 1;
    const uint64 fraction = ieeeMantissa & mask;

    if (fraction == 0)
    {
             
                                                 
                                                              
                                                              
                          
         
      const uint64 m2 = (UINT64CONST(1) << DOUBLE_MANTISSA_BITS) | ieeeMantissa;

      v->mantissa = m2 >> -e2;
      v->exponent = 0;
      return true;
    }
  }

  return false;
}

   
                                                                       
                                                                               
                                              
   
                                       
   
int
double_to_shortest_decimal_bufn(double f, char *result)
{
     
                                                                        
                      
     
  const uint64 bits = double_to_bits(f);

                                                      
  const bool ieeeSign = ((bits >> (DOUBLE_MANTISSA_BITS + DOUBLE_EXPONENT_BITS)) & 1) != 0;
  const uint64 ieeeMantissa = bits & ((UINT64CONST(1) << DOUBLE_MANTISSA_BITS) - 1);
  const uint32 ieeeExponent = (uint32)((bits >> DOUBLE_MANTISSA_BITS) & ((1u << DOUBLE_EXPONENT_BITS) - 1));

                                                        
  if (ieeeExponent == ((1u << DOUBLE_EXPONENT_BITS) - 1u) || (ieeeExponent == 0 && ieeeMantissa == 0))
  {
    return copy_special_str(result, ieeeSign, (ieeeExponent != 0), (ieeeMantissa != 0));
  }

  floating_decimal_64 v;
  const bool isSmallInt = d2d_small_int(ieeeMantissa, ieeeExponent, &v);

  if (!isSmallInt)
  {
    v = d2d(ieeeMantissa, ieeeExponent);
  }

  return to_chars(v, ieeeSign, result);
}

   
                                                                      
                                                                            
                                                  
   
                              
   
int
double_to_shortest_decimal_buf(double f, char *result)
{
  const int index = double_to_shortest_decimal_bufn(f, result);

                             
  Assert(index < DOUBLE_SHORTEST_DECIMAL_LEN);
  result[index] = '\0';
  return index;
}

   
                                                                            
                                                        
   
                                                 
   
char *
double_to_shortest_decimal(double f)
{
  char *const result = (char *)palloc(DOUBLE_SHORTEST_DECIMAL_LEN);

  double_to_shortest_decimal_buf(f, result);
  return result;
}
