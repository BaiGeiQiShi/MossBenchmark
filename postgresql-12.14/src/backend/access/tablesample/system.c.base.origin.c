/*-------------------------------------------------------------------------
 *
 * system.c
 *	  support routines for SYSTEM tablesample method
 *
 * To ensure repeatability of samples, it is necessary that selection of a
 * given tuple be history-independent; otherwise syncscanning would break
 * repeatability, to say nothing of logically-irrelevant maintenance such
 * as physical extension or shortening of the relation.
 *
 * To achieve that, we proceed by hashing each candidate block number together
 * with the active seed, and then selecting it if the hash is less than the
 * cutoff value computed from the selection probability by BeginSampleScan.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/tablesample/system.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <math.h>

#include "access/relscan.h"
#include "access/tsmapi.h"
#include "catalog/pg_type.h"
#include "optimizer/optimizer.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"

/* Private state */
typedef struct
{
  uint64 cutoff;         /* select blocks with hash less than this */
  uint32 seed;           /* random seed */
  BlockNumber nextblock; /* next block to consider sampling */
  OffsetNumber lt;       /* last tuple returned from current block */
} SystemSamplerData;

static void
system_samplescangetsamplesize(PlannerInfo *root, RelOptInfo *baserel, List *paramexprs, BlockNumber *pages, double *tuples);
static void
system_initsamplescan(SampleScanState *node, int eflags);
static void
system_beginsamplescan(SampleScanState *node, Datum *params, int nparams, uint32 seed);
static BlockNumber
system_nextsampleblock(SampleScanState *node, BlockNumber nblocks);
static OffsetNumber
system_nextsampletuple(SampleScanState *node, BlockNumber blockno, OffsetNumber maxoffset);

/*
 * Create a TsmRoutine descriptor for the SYSTEM method.
 */
Datum
tsm_system_handler(PG_FUNCTION_ARGS)
{













}

/*
 * Sample size estimation.
 */
static void
system_samplescangetsamplesize(PlannerInfo *root, RelOptInfo *baserel, List *paramexprs, BlockNumber *pages, double *tuples)
{































}

/*
 * Initialize during executor setup.
 */
static void
system_initsamplescan(SampleScanState *node, int eflags)
{

}

/*
 * Examine parameters and prepare for a sample scan.
 */
static void
system_beginsamplescan(SampleScanState *node, Datum *params, int nparams, uint32 seed)
{




























}

/*
 * Select next block to sample.
 */
static BlockNumber
system_nextsampleblock(SampleScanState *node, BlockNumber nblocks)
{










































}

/*
 * Select next sampled tuple in current block.
 *
 * In block sampling, we just want to sample all the tuples in each selected
 * block.
 *
 * It is OK here to return an offset without knowing if the tuple is visible
 * (or even exists); nodeSamplescan.c will deal with that.
 *
 * When we reach end of the block, return InvalidOffsetNumber which tells
 * SampleScan to go to next block.
 */
static OffsetNumber
system_nextsampletuple(SampleScanState *node, BlockNumber blockno, OffsetNumber maxoffset)
{






















}