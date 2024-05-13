/*-------------------------------------------------------------------------
 *
 * bernoulli.c
 *	  support routines for BERNOULLI tablesample method
 *
 * To ensure repeatability of samples, it is necessary that selection of a
 * given tuple be history-independent; otherwise syncscanning would break
 * repeatability, to say nothing of logically-irrelevant maintenance such
 * as physical extension or shortening of the relation.
 *
 * To achieve that, we proceed by hashing each candidate TID together with
 * the active seed, and then selecting it if the hash is less than the
 * cutoff value computed from the selection probability by BeginSampleScan.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/tablesample/bernoulli.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <math.h>

#include "access/tsmapi.h"
#include "catalog/pg_type.h"
#include "optimizer/optimizer.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"

/* Private state */
typedef struct
{
  uint64 cutoff;   /* select tuples with hash less than this */
  uint32 seed;     /* random seed */
  OffsetNumber lt; /* last tuple returned from current block */
} BernoulliSamplerData;

static void
bernoulli_samplescangetsamplesize(PlannerInfo *root, RelOptInfo *baserel, List *paramexprs, BlockNumber *pages, double *tuples);
static void
bernoulli_initsamplescan(SampleScanState *node, int eflags);
static void
bernoulli_beginsamplescan(SampleScanState *node, Datum *params, int nparams, uint32 seed);
static OffsetNumber
bernoulli_nextsampletuple(SampleScanState *node, BlockNumber blockno, OffsetNumber maxoffset);

/*
 * Create a TsmRoutine descriptor for the BERNOULLI method.
 */
Datum
tsm_bernoulli_handler(PG_FUNCTION_ARGS)
{













}

/*
 * Sample size estimation.
 */
static void
bernoulli_samplescangetsamplesize(PlannerInfo *root, RelOptInfo *baserel, List *paramexprs, BlockNumber *pages, double *tuples)
{






























}

/*
 * Initialize during executor setup.
 */
static void
bernoulli_initsamplescan(SampleScanState *node, int eflags)
{

}

/*
 * Examine parameters and prepare for a sample scan.
 */
static void
bernoulli_beginsamplescan(SampleScanState *node, Datum *params, int nparams, uint32 seed)
{


























}

/*
 * Select next sampled tuple in current block.
 *
 * It is OK here to return an offset without knowing if the tuple is visible
 * (or even exists).  The reason is that we do the coinflip for every tuple
 * offset in the table.  Since all tuples have the same probability of being
 * returned, it doesn't matter if we do extra coinflips for invisible tuples.
 *
 * When we reach end of the block, return InvalidOffsetNumber which tells
 * SampleScan to go to next block.
 */
static OffsetNumber
bernoulli_nextsampletuple(SampleScanState *node, BlockNumber blockno, OffsetNumber maxoffset)
{



















































}