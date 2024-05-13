/*-------------------------------------------------------------------------
 *
 * sampling.c
 *	  Relation block sampling routines.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/misc/sampling.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <math.h>

#include "utils/sampling.h"

/*
 * BlockSampler_Init -- prepare for random sampling of blocknumbers
 *
 * BlockSampler provides algorithm for block level sampling of a relation
 * as discussed on pgsql-hackers 2004-04-02 (subject "Large DB")
 * It selects a random sample of samplesize blocks out of
 * the nblocks blocks in the table. If the table has less than
 * samplesize blocks, all blocks are selected.
 *
 * Since we know the total number of blocks in advance, we can use the
 * straightforward Algorithm S from Knuth 3.4.2, rather than Vitter's
 * algorithm.
 */
void
BlockSampler_Init(BlockSampler bs, BlockNumber nblocks, int samplesize, long randseed)
{











}

bool
BlockSampler_HasMore(BlockSampler bs)
{

}

BlockNumber
BlockSampler_Next(BlockSampler bs)
{


















































}

/*
 * These two routines embody Algorithm Z from "Random sampling with a
 * reservoir" by Jeffrey S. Vitter, in ACM Trans. Math. Softw. 11, 1
 * (Mar. 1985), Pages 37-57.  Vitter describes his algorithm in terms
 * of the count S of records to skip before processing another record.
 * It is computed primarily based on t, the number of records already read.
 * The only extra state needed between calls is W, a random state variable.
 *
 * reservoir_init_selection_state computes the initial W value.
 *
 * Given that we've already read t records (t >= n), reservoir_get_next_S
 * determines the number of records to skip before the next record is
 * processed.
 */
void
reservoir_init_selection_state(ReservoirState rs, int n)
{








}

double
reservoir_get_next_S(ReservoirState rs, double t, int n)
{







































































}

/*----------
 * Random number generator used by sampling
 *----------
 */
void
sampler_random_init_state(long seed, SamplerRandomState randstate)
{



}

/* Select a random value R uniformly distributed in (0 - 1) */
double
sampler_random_fract(SamplerRandomState randstate)
{








}

/*
 * Backwards-compatible API for block sampling
 *
 * This code is now deprecated, but since it's still in use by many FDWs,
 * we should keep it for awhile at least.  The functionality is the same as
 * sampler_random_fract/reservoir_init_selection_state/reservoir_get_next_S,
 * except that a common random state is used across all callers.
 */
static ReservoirStateData oldrs;

double
anl_random_fract(void)
{








}

double
anl_init_selection_state(int n)
{








}

double
anl_get_next_S(double t, int n, double *stateptr)
{






}