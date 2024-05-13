/*---------------------------------------------------------------------------
 *
 * Ryu floating-point output for single precision.
 *
 * Portions Copyright (c) 2018-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/common/f2s.c
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

/*
 * This table is generated (by the upstream) by PrintFloatLookupTable,* and modified (by us) to add UINT64CONST.
 */
#define FLOAT_POW5_INV_BITCOUNT 59
static const uint64 FLOAT_POW5_INV_SPLIT[31] = {UINT64CONST(576460752303423489), UINT64CONST(461168601842738791), UINT64CONST(368934881474191033), UINT64CONST(295147905179352826), UINT64CONST(472236648286964522), UINT64CONST(377789318629571618), UINT64CONST(302231454903657294), UINT64CONST(483570327845851670), UINT64CONST(386856262276681336), UINT64CONST(309485009821345069), UINT64CONST(495176015714152110), UINT64CONST(396140812571321688), UINT64CONST(316912650057057351), UINT64CONST(507060240091291761), UINT64CONST(405648192073033409), UINT64CONST(324518553658426727), UINT64CONST(519229685853482763), UINT64CONST(415383748682786211), UINT64CONST(332306998946228969), UINT64CONST(531691198313966350), UINT64CONST(425352958651173080), UINT64CONST(340282366920938464), UINT64CONST(544451787073501542), UINT64CONST(435561429658801234), UINT64CONST(348449143727040987), UINT64CONST(557518629963265579), UINT64CONST(446014903970612463), UINT64CONST(356811923176489971),
    UINT64CONST(570899077082383953), UINT64CONST(456719261665907162), UINT64CONST(365375409332725730)};
#define FLOAT_POW5_BITCOUNT 61
static const uint64 FLOAT_POW5_SPLIT[47] = {UINT64CONST(1152921504606846976), UINT64CONST(1441151880758558720), UINT64CONST(1801439850948198400), UINT64CONST(2251799813685248000), UINT64CONST(1407374883553280000), UINT64CONST(1759218604441600000), UINT64CONST(2199023255552000000), UINT64CONST(1374389534720000000), UINT64CONST(1717986918400000000), UINT64CONST(2147483648000000000), UINT64CONST(1342177280000000000), UINT64CONST(1677721600000000000), UINT64CONST(2097152000000000000), UINT64CONST(1310720000000000000), UINT64CONST(1638400000000000000), UINT64CONST(2048000000000000000), UINT64CONST(1280000000000000000), UINT64CONST(1600000000000000000), UINT64CONST(2000000000000000000), UINT64CONST(1250000000000000000), UINT64CONST(1562500000000000000), UINT64CONST(1953125000000000000), UINT64CONST(1220703125000000000), UINT64CONST(1525878906250000000), UINT64CONST(1907348632812500000), UINT64CONST(1192092895507812500), UINT64CONST(1490116119384765625), UINT64CONST(1862645149230957031),
    UINT64CONST(1164153218269348144), UINT64CONST(1455191522836685180), UINT64CONST(1818989403545856475), UINT64CONST(2273736754432320594), UINT64CONST(1421085471520200371), UINT64CONST(1776356839400250464), UINT64CONST(2220446049250313080), UINT64CONST(1387778780781445675), UINT64CONST(1734723475976807094), UINT64CONST(2168404344971008868), UINT64CONST(1355252715606880542), UINT64CONST(1694065894508600678), UINT64CONST(2117582368135750847), UINT64CONST(1323488980084844279), UINT64CONST(1654361225106055349), UINT64CONST(2067951531382569187), UINT64CONST(1292469707114105741), UINT64CONST(1615587133892632177), UINT64CONST(2019483917365790221)};

static inline uint32
pow5Factor(uint32 value)
{

















}

/*  Returns true if value is divisible by 5^p. */
static inline bool
multipleOfPowerOf5(const uint32 value, const uint32 p)
{

}

/*  Returns true if value is divisible by 2^p. */
static inline bool
multipleOfPowerOf2(const uint32 value, const uint32 p)
{


}

/*
 * It seems to be slightly faster to avoid uint128_t here, although the
 * generated code for uint128_t looks slightly nicer.
 */
static inline uint32
mulShift(const uint32 m, const uint64 factor, const int32 shift)
{





































}

static inline uint32
mulPow5InvDivPow2(const uint32 m, const uint32 q, const int32 j)
{

}

static inline uint32
mulPow5divPow2(const uint32 m, const uint32 i, const int32 j)
{

}

static inline uint32
decimalLength(const uint32 v)
{




































}

/*  A floating decimal representing m * 10^e. */
typedef struct floating_decimal_32
{
  uint32 mantissa;
  int32 exponent;
} floating_decimal_32;

static inline floating_decimal_32
f2d(const uint32 ieeeMantissa, const uint32 ieeeExponent)
{



















































































































































































































}

static inline int
to_chars_f(const floating_decimal_32 v, const uint32 olength, char *const result)
{






















































































































}

static inline int
to_chars(const floating_decimal_32 v, const bool sign, char *const result)
{

































































































































}

static inline bool
f2d_small_int(const uint32 ieeeMantissa, const uint32 ieeeExponent, floating_decimal_32 *v)
{







































}

/*
 * Store the shortest decimal representation of the given float as an
 * UNTERMINATED string in the caller's supplied buffer (which must be at least
 * FLOAT_SHORTEST_DECIMAL_LEN-1 bytes long).
 *
 * Returns the number of bytes stored.
 */
int
float_to_shortest_decimal_bufn(float f, char *result)
{


























}

/*
 * Store the shortest decimal representation of the given float as a
 * null-terminated string in the caller's supplied buffer (which must be at
 * least FLOAT_SHORTEST_DECIMAL_LEN bytes long).
 *
 * Returns the string length.
 */
int
float_to_shortest_decimal_buf(float f, char *result)
{






}

/*
 * Return the shortest decimal representation as a null-terminated palloc'd
 * string (outside the backend, uses malloc() instead).
 *
 * Caller is responsible for freeing the result.
 */
char *
float_to_shortest_decimal(float f)
{




}