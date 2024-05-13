/*-------------------------------------------------------------------------
 *
 * hyperloglog.c
 *	  HyperLogLog cardinality estimator
 *
 * Portions Copyright (c) 2014-2019, PostgreSQL Global Development Group
 *
 * Based on Hideaki Ohno's C++ implementation.  This is probably not ideally
 * suited to estimating the cardinality of very large sets;  in particular, we
 * have not attempted to further optimize the implementation as described in
 * the Heule, Nunkesser and Hall paper "HyperLogLog in Practice: Algorithmic
 * Engineering of a State of The Art Cardinality Estimation Algorithm".
 *
 * A sparse representation of HyperLogLog state is used, with fixed space
 * overhead.
 *
 * The copyright terms of Ohno's original version (the MIT license) follow.
 *
 * IDENTIFICATION
 *	  src/backend/lib/hyperloglog.c
 *
 *-------------------------------------------------------------------------
 */

/*
 * Copyright (c) 2013 Hideaki Ohno <hide.o.j55{at}gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the 'Software'), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "postgres.h"

#include <math.h>

#include "lib/hyperloglog.h"

#define POW_2_32 (4294967296.0)
#define NEG_POW_2_32 (-4294967296.0)

static inline uint8
rho(uint32 x, uint8 b);

/*
 * Initialize HyperLogLog track state, by bit width
 *
 * bwidth is bit width (so register size will be 2 to the power of bwidth).
 * Must be between 4 and 16 inclusive.
 */
void
initHyperLogLog(hyperLogLogState *cState, uint8 bwidth)
{











































}

/*
 * Initialize HyperLogLog track state, by error rate
 *
 * Instead of specifying bwidth (number of bits used for addressing the
 * register), this method allows sizing the counter for particular error
 * rate using a simple formula from the paper:
 *
 *	 e = 1.04 / sqrt(m)
 *
 * where 'm' is the number of registers, i.e. (2^bwidth). The method
 * finds the lowest bwidth with 'e' below the requested error rate, and
 * then uses it to initialize the counter.
 *
 * As bwidth has to be between 4 and 16, the worst possible error rate
 * is between ~25% (bwidth=4) and 0.4% (bwidth=16).
 */
void
initHyperLogLogError(hyperLogLogState *cState, double error)
{














}

/*
 * Free HyperLogLog track state
 *
 * Releases allocated resources, but not the state itself (in case it's not
 * allocated by palloc).
 */
void
freeHyperLogLog(hyperLogLogState *cState)
{


}

/*
 * Adds element to the estimator, from caller-supplied hash.
 *
 * It is critical that the hash value passed be an actual hash value, typically
 * generated using hash_any().  The algorithm relies on a specific bit-pattern
 * observable in conjunction with stochastic averaging.  There must be a
 * uniform distribution of bits in hash values for each distinct original value
 * observed.
 */
void
addHyperLogLog(hyperLogLogState *cState, uint32 hash)
{










}

/*
 * Estimates cardinality, based on elements added so far
 */
double
estimateHyperLogLog(hyperLogLogState *cState)
{





































}

/*
 * Worker for addHyperLogLog().
 *
 * Calculates the position of the first set bit in first b bits of x argument
 * starting from the first, reading from most significant to least significant
 * bits.
 *
 * Example (when considering fist 10 bits of x):
 *
 * rho(x = 0b1000000000)   returns 1
 * rho(x = 0b0010000000)   returns 3
 * rho(x = 0b0000000000)   returns b + 1
 *
 * "The binary address determined by the first b bits of x"
 *
 * Return value "j" used to index bit pattern to watch.
 */
static inline uint8
rho(uint32 x, uint8 b)
{









}