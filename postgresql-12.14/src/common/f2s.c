                                                                              
   
                                                   
   
                                                                         
   
                  
                      
   
                                                                              
                                                                               
                   
   
                            
   
                                                                       
                         
   
                                                        
                                                    
   
                                                                               
                                        
   
                                                       
                                               
   
                                                                               
                                                                            
                                    
   
                                                                              
   

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "common/shortest_dec.h"

#include "ryu_common.h"
#include "digit_table.h"

#define FLOAT_MANTISSA_BITS 23
#define FLOAT_EXPONENT_BITS 8
#define FLOAT_BIAS 127

   
                                                                       
                                            
   
#define FLOAT_POW5_INV_BITCOUNT 59
static const uint64 FLOAT_POW5_INV_SPLIT[31] = {UINT64CONST(576460752303423489), UINT64CONST(461168601842738791), UINT64CONST(368934881474191033), UINT64CONST(295147905179352826), UINT64CONST(472236648286964522), UINT64CONST(377789318629571618), UINT64CONST(302231454903657294), UINT64CONST(483570327845851670), UINT64CONST(386856262276681336), UINT64CONST(309485009821345069), UINT64CONST(495176015714152110), UINT64CONST(396140812571321688), UINT64CONST(316912650057057351), UINT64CONST(507060240091291761), UINT64CONST(405648192073033409), UINT64CONST(324518553658426727), UINT64CONST(519229685853482763), UINT64CONST(415383748682786211), UINT64CONST(332306998946228969), UINT64CONST(531691198313966350), UINT64CONST(425352958651173080), UINT64CONST(340282366920938464), UINT64CONST(544451787073501542), UINT64CONST(435561429658801234), UINT64CONST(348449143727040987), UINT64CONST(557518629963265579), UINT64CONST(446014903970612463), UINT64CONST(356811923176489971),
    UINT64CONST(570899077082383953), UINT64CONST(456719261665907162), UINT64CONST(365375409332725730)};
#define FLOAT_POW5_BITCOUNT 61
static const uint64 FLOAT_POW5_SPLIT[47] = {UINT64CONST(1152921504606846976), UINT64CONST(1441151880758558720), UINT64CONST(1801439850948198400), UINT64CONST(2251799813685248000), UINT64CONST(1407374883553280000), UINT64CONST(1759218604441600000), UINT64CONST(2199023255552000000), UINT64CONST(1374389534720000000), UINT64CONST(1717986918400000000), UINT64CONST(2147483648000000000), UINT64CONST(1342177280000000000), UINT64CONST(1677721600000000000), UINT64CONST(2097152000000000000), UINT64CONST(1310720000000000000), UINT64CONST(1638400000000000000), UINT64CONST(2048000000000000000), UINT64CONST(1280000000000000000), UINT64CONST(1600000000000000000), UINT64CONST(2000000000000000000), UINT64CONST(1250000000000000000), UINT64CONST(1562500000000000000), UINT64CONST(1953125000000000000), UINT64CONST(1220703125000000000), UINT64CONST(1525878906250000000), UINT64CONST(1907348632812500000), UINT64CONST(1192092895507812500), UINT64CONST(1490116119384765625), UINT64CONST(1862645149230957031),
    UINT64CONST(1164153218269348144), UINT64CONST(1455191522836685180), UINT64CONST(1818989403545856475), UINT64CONST(2273736754432320594), UINT64CONST(1421085471520200371), UINT64CONST(1776356839400250464), UINT64CONST(2220446049250313080), UINT64CONST(1387778780781445675), UINT64CONST(1734723475976807094), UINT64CONST(2168404344971008868), UINT64CONST(1355252715606880542), UINT64CONST(1694065894508600678), UINT64CONST(2117582368135750847), UINT64CONST(1323488980084844279), UINT64CONST(1654361225106055349), UINT64CONST(2067951531382569187), UINT64CONST(1292469707114105741), UINT64CONST(1615587133892632177), UINT64CONST(2019483917365790221)};

static inline uint32
pow5Factor(uint32 value)
{
  uint32 count = 0;

  for (;;)
  {
    Assert(value != 0);
    const uint32 q = value / 5;
    const uint32 r = value % 5;

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
multipleOfPowerOf5(const uint32 value, const uint32 p)
{
  return pow5Factor(value) >= p;
}

                                                 
static inline bool
multipleOfPowerOf2(const uint32 value, const uint32 p)
{
                                         
  return (value & ((1u << p) - 1)) == 0;
}

   
                                                                        
                                                      
   
static inline uint32
mulShift(const uint32 m, const uint64 factor, const int32 shift)
{
     
                                                                     
               
     
  const uint32 factorLo = (uint32)(factor);
  const uint32 factorHi = (uint32)(factor >> 32);
  const uint64 bits0 = (uint64)m * factorLo;
  const uint64 bits1 = (uint64)m * factorHi;

  Assert(shift > 32);

#ifdef RYU_32_BIT_PLATFORM

     
                                                                         
                                                                       
     
  const uint32 bits0Hi = (uint32)(bits0 >> 32);
  uint32 bits1Lo = (uint32)(bits1);
  uint32 bits1Hi = (uint32)(bits1 >> 32);

  bits1Lo += bits0Hi;
  bits1Hi += (bits1Lo < bits0Hi);

  const int32 s = shift - 32;

  return (bits1Hi << (32 - s)) | (bits1Lo >> s);

#else                          

  const uint64 sum = (bits0 >> 32) + bits1;
  const uint64 shiftedSum = sum >> (shift - 32);

  Assert(shiftedSum <= PG_UINT32_MAX);
  return (uint32)shiftedSum;

#endif                          
}

static inline uint32
mulPow5InvDivPow2(const uint32 m, const uint32 q, const int32 j)
{
  return mulShift(m, FLOAT_POW5_INV_SPLIT[q], j);
}

static inline uint32
mulPow5divPow2(const uint32 m, const uint32 i, const int32 j)
{
  return mulShift(m, FLOAT_POW5_SPLIT[i], j);
}

static inline uint32
decimalLength(const uint32 v)
{
                                                          
                                                     
  Assert(v < 1000000000);
  if (v >= 100000000)
  {
    return 9;
  }
  if (v >= 10000000)
  {
    return 8;
  }
  if (v >= 1000000)
  {
    return 7;
  }
  if (v >= 100000)
  {
    return 6;
  }
  if (v >= 10000)
  {
    return 5;
  }
  if (v >= 1000)
  {
    return 4;
  }
  if (v >= 100)
  {
    return 3;
  }
  if (v >= 10)
  {
    return 2;
  }
  return 1;
}

                                                
typedef struct floating_decimal_32
{
  uint32 mantissa;
  int32 exponent;
} floating_decimal_32;

static inline floating_decimal_32
f2d(const uint32 ieeeMantissa, const uint32 ieeeExponent)
{
  int32 e2;
  uint32 m2;

  if (ieeeExponent == 0)
  {
                                                                             
    e2 = 1 - FLOAT_BIAS - FLOAT_MANTISSA_BITS - 2;
    m2 = ieeeMantissa;
  }
  else
  {
    e2 = ieeeExponent - FLOAT_BIAS - FLOAT_MANTISSA_BITS - 2;
    m2 = (1u << FLOAT_MANTISSA_BITS) | ieeeMantissa;
  }

#if STRICTLY_SHORTEST
  const bool even = (m2 & 1) == 0;
  const bool acceptBounds = even;
#else
  const bool acceptBounds = false;
#endif

                                                                        
  const uint32 mv = 4 * m2;
  const uint32 mp = 4 * m2 + 2;

                                                               
  const uint32 mmShift = ieeeMantissa != 0 || ieeeExponent <= 1;
  const uint32 mm = 4 * m2 - 1 - mmShift;

                                                                        
  uint32 vr, vp, vm;
  int32 e10;
  bool vmIsTrailingZeros = false;
  bool vrIsTrailingZeros = false;
  uint8 lastRemovedDigit = 0;

  if (e2 >= 0)
  {
    const uint32 q = log10Pow2(e2);

    e10 = q;

    const int32 k = FLOAT_POW5_INV_BITCOUNT + pow5bits(q) - 1;
    const int32 i = -e2 + q + k;

    vr = mulPow5InvDivPow2(mv, q, i);
    vp = mulPow5InvDivPow2(mp, q, i);
    vm = mulPow5InvDivPow2(mm, q, i);

    if (q != 0 && (vp - 1) / 10 <= vm / 10)
    {
         
                                                                       
                                                                     
                                                                     
                                                       
         
      const int32 l = FLOAT_POW5_INV_BITCOUNT + pow5bits(q - 1) - 1;

      lastRemovedDigit = (uint8)(mulPow5InvDivPow2(mv, q - 1, -e2 + q - 1 + l) % 10);
    }
    if (q <= 9)
    {
         
                                                                         
                                   
         
                                                                    
         
      if (mv % 5 == 0)
      {
        vrIsTrailingZeros = multipleOfPowerOf5(mv, q);
      }
      else if (acceptBounds)
      {
        vmIsTrailingZeros = multipleOfPowerOf5(mm, q);
      }
      else
      {
        vp -= multipleOfPowerOf5(mp, q);
      }
    }
  }
  else
  {
    const uint32 q = log10Pow5(-e2);

    e10 = q + e2;

    const int32 i = -e2 - q;
    const int32 k = pow5bits(i) - FLOAT_POW5_BITCOUNT;
    int32 j = q - k;

    vr = mulPow5divPow2(mv, i, j);
    vp = mulPow5divPow2(mp, i, j);
    vm = mulPow5divPow2(mm, i, j);

    if (q != 0 && (vp - 1) / 10 <= vm / 10)
    {
      j = q - 1 - (pow5bits(i + 1) - FLOAT_POW5_BITCOUNT);
      lastRemovedDigit = (uint8)(mulPow5divPow2(mv, i + 1, j) % 10);
    }
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
    else if (q < 31)
    {
                                                   
      vrIsTrailingZeros = multipleOfPowerOf2(mv, q - 1);
    }
  }

     
                                                                         
                            
     
  uint32 removed = 0;
  uint32 output;

  if (vmIsTrailingZeros || vrIsTrailingZeros)
  {
                                                     
    while (vp / 10 > vm / 10)
    {
      vmIsTrailingZeros &= vm - (vm / 10) * 10 == 0;
      vrIsTrailingZeros &= lastRemovedDigit == 0;
      lastRemovedDigit = (uint8)(vr % 10);
      vr /= 10;
      vp /= 10;
      vm /= 10;
      ++removed;
    }
    if (vmIsTrailingZeros)
    {
      while (vm % 10 == 0)
      {
        vrIsTrailingZeros &= lastRemovedDigit == 0;
        lastRemovedDigit = (uint8)(vr % 10);
        vr /= 10;
        vp /= 10;
        vm /= 10;
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
       
                                                                       
                         
       
                                                                     
                                            
       
    while (vp / 10 > vm / 10)
    {
      lastRemovedDigit = (uint8)(vr % 10);
      vr /= 10;
      vp /= 10;
      vm /= 10;
      ++removed;
    }

       
                                                                          
           
       
    output = vr + (vr == vm || lastRemovedDigit >= 5);
  }

  const int32 exp = e10 + removed;

  floating_decimal_32 fd;

  fd.exponent = exp;
  fd.mantissa = output;
  return fd;
}

static inline int
to_chars_f(const floating_decimal_32 v, const uint32 olength, char *const result)
{
                                                 
  int index = 0;

  uint32 output = v.mantissa;
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
       
                                                                           
                                                                         
                                                                      
                                              
       
    Assert(exp < 6 && exp + olength <= 6);
    memset(result, '0', 8);
  }

  while (output >= 10000)
  {
    const uint32 c = output - 10000 * (output / 10000);
    const uint32 c0 = (c % 100) << 1;
    const uint32 c1 = (c / 100) << 1;

    output /= 10000;

    memcpy(result + index + olength - i - 2, DIGIT_TABLE + c0, 2);
    memcpy(result + index + olength - i - 4, DIGIT_TABLE + c1, 2);
    i += 4;
  }
  if (output >= 100)
  {
    const uint32 c = (output % 100) << 1;

    output /= 100;
    memcpy(result + index + olength - i - 2, DIGIT_TABLE + c, 2);
    i += 2;
  }
  if (output >= 10)
  {
    const uint32 c = output << 1;

    memcpy(result + index + olength - i - 2, DIGIT_TABLE + c, 2);
  }
  else
  {
    result[index] = (char)('0' + output);
  }

  if (index == 1)
  {
       
                                                                       
                                                                 
                                                                
       
    Assert(nexp < 7);
                                                                  
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
to_chars(const floating_decimal_32 v, const bool sign, char *const result)
{
                                                 
  int index = 0;

  uint32 output = v.mantissa;
  uint32 olength = decimalLength(output);
  int32 exp = v.exponent + olength - 1;

  if (sign)
  {
    result[index++] = '-';
  }

     
                                                                      
                                                                        
                                                                   
     
  if (exp >= -4 && exp < 6)
  {
    return to_chars_f(v, olength, result + index) + sign;
  }

     
                                                                          
                                                                        
                                                                          
                                                                           
                                         
     
                                                                           
                                                                          
                                                            
     
                                                                        
                                                                          
                                                                      
                                                           
     
  if (v.exponent == 0)
  {
    while ((output & 1) == 0)
    {
      const uint32 q = output / 10;
      const uint32 r = output - 10 * q;

      if (r != 0)
      {
        break;
      }
      output = q;
      --olength;
    }
  }

         
                               
                                          
     
                                                
                                                   
                                                       
       
                                        
     
  uint32 i = 0;

  while (output >= 10000)
  {
    const uint32 c = output - 10000 * (output / 10000);
    const uint32 c0 = (c % 100) << 1;
    const uint32 c1 = (c / 100) << 1;

    output /= 10000;

    memcpy(result + index + olength - i - 1, DIGIT_TABLE + c0, 2);
    memcpy(result + index + olength - i - 3, DIGIT_TABLE + c1, 2);
    i += 4;
  }
  if (output >= 100)
  {
    const uint32 c = (output % 100) << 1;

    output /= 100;
    memcpy(result + index + olength - i - 1, DIGIT_TABLE + c, 2);
    i += 2;
  }
  if (output >= 10)
  {
    const uint32 c = output << 1;

       
                                                                        
               
       
    result[index + olength - i] = DIGIT_TABLE[c + 1];
    result[index] = DIGIT_TABLE[c];
  }
  else
  {
    result[index] = (char)('0' + output);
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

  memcpy(result + index, DIGIT_TABLE + 2 * exp, 2);
  index += 2;

  return index;
}

static inline bool
f2d_small_int(const uint32 ieeeMantissa, const uint32 ieeeExponent, floating_decimal_32 *v)
{
  const int32 e2 = (int32)ieeeExponent - FLOAT_BIAS - FLOAT_MANTISSA_BITS;

     
                                                                             
                                                                          
     

  if (e2 >= -FLOAT_MANTISSA_BITS && e2 <= 0)
  {
           
                                                   
                                     
       
                                                                         
                                                                          
                                                                         
                                                                   
                                                                 
       
    const uint32 mask = (1U << -e2) - 1;
    const uint32 fraction = ieeeMantissa & mask;

    if (fraction == 0)
    {
             
                                                 
                                                              
                                                             
                          
         
      const uint32 m2 = (1U << FLOAT_MANTISSA_BITS) | ieeeMantissa;

      v->mantissa = m2 >> -e2;
      v->exponent = 0;
      return true;
    }
  }

  return false;
}

   
                                                                      
                                                                               
                                             
   
                                       
   
int
float_to_shortest_decimal_bufn(float f, char *result)
{
     
                                                                        
                      
     
  const uint32 bits = float_to_bits(f);

                                                      
  const bool ieeeSign = ((bits >> (FLOAT_MANTISSA_BITS + FLOAT_EXPONENT_BITS)) & 1) != 0;
  const uint32 ieeeMantissa = bits & ((1u << FLOAT_MANTISSA_BITS) - 1);
  const uint32 ieeeExponent = (bits >> FLOAT_MANTISSA_BITS) & ((1u << FLOAT_EXPONENT_BITS) - 1);

                                                        
  if (ieeeExponent == ((1u << FLOAT_EXPONENT_BITS) - 1u) || (ieeeExponent == 0 && ieeeMantissa == 0))
  {
    return copy_special_str(result, ieeeSign, (ieeeExponent != 0), (ieeeMantissa != 0));
  }

  floating_decimal_32 v;
  const bool isSmallInt = f2d_small_int(ieeeMantissa, ieeeExponent, &v);

  if (!isSmallInt)
  {
    v = f2d(ieeeMantissa, ieeeExponent);
  }

  return to_chars(v, ieeeSign, result);
}

   
                                                                     
                                                                            
                                                 
   
                              
   
int
float_to_shortest_decimal_buf(float f, char *result)
{
  const int index = float_to_shortest_decimal_bufn(f, result);

                             
  Assert(index < FLOAT_SHORTEST_DECIMAL_LEN);
  result[index] = '\0';
  return index;
}

   
                                                                            
                                                        
   
                                                 
   
char *
float_to_shortest_decimal(float f)
{
  char *const result = (char *)palloc(FLOAT_SHORTEST_DECIMAL_LEN);

  float_to_shortest_decimal_buf(f, result);
  return result;
}
