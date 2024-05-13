/*-------------------------------------------------------------------------
 *
 * instrument.c
 *	 functions for instrumentation of plan execution
 *
 *
 * Copyright (c) 2001-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/executor/instrument.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <unistd.h>

#include "executor/instrument.h"

BufferUsage pgBufferUsage;
static BufferUsage save_pgBufferUsage;

static void
BufferUsageAdd(BufferUsage *dst, const BufferUsage *add);
static void
BufferUsageAccumDiff(BufferUsage *dst, const BufferUsage *add, const BufferUsage *sub);

/* Allocate new instrumentation structure(s) */
Instrumentation *
InstrAlloc(int n, int instrument_options)
{


















}

/* Initialize a pre-allocated instrumentation structure. */
void
InstrInit(Instrumentation *instr, int instrument_options)
{



}

/* Entry to a plan node */
void
InstrStartNode(Instrumentation *instr)
{










}

/* Exit from a plan node */
void
InstrStopNode(Instrumentation *instr, double nTuples)
{































}

/* Finish a run cycle for a plan node */
void
InstrEndLoop(Instrumentation *instr)
{



























}

/* aggregate instrumentation information */
void
InstrAggNode(Instrumentation *dst, Instrumentation *add)
{


























}

/* note current values during parallel executor startup */
void
InstrStartParallelQuery(void)
{

}

/* report usage after parallel executor shutdown */
void
InstrEndParallelQuery(BufferUsage *result)
{


}

/* accumulate work done by workers in leader's stats */
void
InstrAccumParallelQuery(BufferUsage *result)
{

}

/* dst += add */
static void
BufferUsageAdd(BufferUsage *dst, const BufferUsage *add)
{












}

/* dst += add - sub */
static void
BufferUsageAccumDiff(BufferUsage *dst, const BufferUsage *add, const BufferUsage *sub)
{












}