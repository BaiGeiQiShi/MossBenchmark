/*---------------------------------------------------------------------------
 *
 * Ryu floating-point output for double precision.
 *
 * Portions Copyright (c) 2018-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/common/d2s.c
 *
 * This is a modification of code taken from github.com/ulfjack/ryu under the
 * terms of the Boost license (not the Apache license). The original copyright
 * notice follows:
 *
 * Copyright 2018 Ulf Adams
 *
 * The contents of this file may be used under the terms of the Apache
 * License, Version 2.0.
 *
 *     (See accompanying file LICENSE-Apache or copy at
 *      http://www.apache.org/licenses/LICENSE-2.0)
 *
 * Alternatively, the contents of this file may be used under the terms of the
 * Boost Software License, Version 1.0.
 *
 *     (See accompanying file LICENSE-Boost or copy at
 *      https://www.boost.org/LICENSE_1_0.txt)
 *
 * Unless required by applicable law or agreed to in writing, this software is
 * distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.
 *
 *---------------------------------------------------------------------------
 */

/*
 *  Runtime compiler options:
 *
 *  -DRYU_ONLY_64_BIT_OPS Avoid using uint128 or 64-bit intrinsics. Slower,*      depending on your compiler.
 */

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "common/shortest_dec.h"

/*
 * For consistency, we use 128-bit types if and only if the rest of PG also
 * does, even though we could use them here without worrying about the
 * alignment concerns that apply elsewhere.
 */
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

















}

/*  Returns true if value is divisible by 5^p. */
static inline bool
multipleOfPowerOf5(const uint64 value, const uint32 p)
{





}

/*  Returns true if value is divisible by 2^p. */
static inline bool
multipleOfPowerOf2(const uint64 value, const uint32 p)
{


}

/*
 * We need a 64x128-bit multiplication and a subsequent 128-bit shift.
 *
 * Multiplication:
 *
 *    The 64-bit factor is variable and passed in, the 128-bit factor comes
 *    from a lookup table. We know that the 64-bit factor only has 55
 *    significant bits (i.e., the 9 topmost bits are zeros). The 128-bit
 *    factor only has 124 significant bits (i.e., the 4 topmost bits are
 *    zeros).
 *
 * Shift:
 *
 *    In principle, the multiplication result requires 55 + 124 = 179 bits to
 *    represent. However, we then shift this value to the right by j, which is
 *    at least j >= 115, so the result is guaranteed to fit into 179 - 115 =
 *    64 bits. This means that we only need the topmost 64 significant bits of
 *    the 64x128-bit multiplication.
 *
 * There are several ways to do this:
 *
 *  1. Best case: the compiler exposes a 128-bit type.
 *     We perform two 64x64-bit multiplications, add the higher 64 bits of the
 *     lower result to the higher result, and shift by j - 64 bits.
 *
 *     We explicitly cast from 64-bit to 128-bit, so the compiler can tell
 *     that these are only 64-bit inputs, and can map these to the best
 *     possible sequence of assembly instructions. x86-64 machines happen to
 *     have matching assembly instructions for 64x64-bit multiplications and
 *     128-bit shifts.
 *
 *  2. Second best case: the compiler exposes intrinsics for the x86-64
 *     assembly instructions mentioned in 1.
 *
 *  3. We only have 64x64 bit instructions that return the lower 64 bits of
 *     the result, i.e., we have to use plain C.
 *
 *     Our inputs are less than the full width, so we have three options:
 *     a. Ignore this fact and just implement the intrinsics manually.
 *     b. Split both into 31-bit pieces, which guarantees no internal
 *        overflow, but requires extra work upfront (unless we change the
 *        lookup table).
 *     c. Split only the first factor into 31-bit pieces, which also
 *        guarantees no internal overflow, but requires extra work since the
 *        intermediate results are not perfectly aligned.
 */
#if defined(HAVE_INT128)

/*  Best case: use 128-bit type. */
static inline uint64
mulShift(const uint64 m, const uint64 *const mul, const int32 j)
{




}

static inline uint64
mulShiftAll(const uint64 m, const uint64 *const mul, const int32 j, uint64 *const vp, uint64 *const vm, const uint32 mmShift)
{



}

#elif defined(HAS_64_BIT_INTRINSICS)

static inline uint64
mulShift(const uint64 m, const uint64 *const mul, const int32 j)
{
  /* m is maximum 55 bits */
  uint64 high1;

  /* 128 */
  const uint64 low1 = umul128(m, mul[1], &high1);

  /* 64 */
  uint64 high0;
  uint64 sum;

  /* 64 */
  umul128(m, mul[0], &high0);
  /* 0 */
  sum = high0 + low1;

  if (sum < high0)
  {
    ++high1;
    /* overflow into high1 */
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

#else /* // !defined(HAVE_INT128) &&                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
       * !defined(HAS_64_BIT_INTRINSICS) */

static inline uint64
mulShiftAll(uint64 m, const uint64 *const mul, const int32 j, uint64 *const vp, uint64 *const vm, const uint32 mmShift)
{
  m <<= 1; /* m is maximum 55 bits */

  uint64 tmp;
  const uint64 lo = umul128(m, mul[0], &tmp);
  uint64 hi;
  const uint64 mid = tmp + umul128(m, mul[1], &hi);

  hi += mid < tmp; /* overflow into hi */

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

#endif /* // HAS_64_BIT_INTRINSICS */

static inline uint32
decimalLength(const uint64 v)
{






































































}

/*  A floating decimal representing m * 10^e. */
typedef struct floating_decimal_64
{
  uint64 mantissa;
  int32 exponent;
} floating_decimal_64;

static inline floating_decimal_64
d2d(const uint64 ieeeMantissa, const uint32 ieeeExponent)
{



























































































































































































































































































}

static inline int
to_chars_df(const floating_decimal_64 v, const uint32 olength, char *const result)
{























































































































































}

static inline int
to_chars(floating_decimal_64 v, const bool sign, char *const result)
{
















































































































































































}

static inline bool
d2d_small_int(const uint64 ieeeMantissa, const uint32 ieeeExponent, floating_decimal_64 *v)
{







































}

/*
 * Store the shortest decimal representation of the given double as an
 * UNTERMINATED string in the caller's supplied buffer (which must be at least
 * DOUBLE_SHORTEST_DECIMAL_LEN-1 bytes long).
 *
 * Returns the number of bytes stored.
 */
int
double_to_shortest_decimal_bufn(double f, char *result)
{


























}

/*
 * Store the shortest decimal representation of the given double as a
 * null-terminated string in the caller's supplied buffer (which must be at
 * least DOUBLE_SHORTEST_DECIMAL_LEN bytes long).
 *
 * Returns the string length.
 */
int
double_to_shortest_decimal_buf(double f, char *result)
{






}

/*
 * Return the shortest decimal representation as a null-terminated palloc'd
 * string (outside the backend, uses malloc() instead).
 *
 * Caller is responsible for freeing the result.
 */
char *
double_to_shortest_decimal(double f)
{




}