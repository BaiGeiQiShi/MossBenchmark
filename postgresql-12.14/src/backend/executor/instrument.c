                                                                            
   
                
                                                    
   
   
                                                                
   
                  
                                       
   
                                                                            
   
#include "postgres.h"

#include <unistd.h>

#include "executor/instrument.h"

BufferUsage pgBufferUsage;
static BufferUsage save_pgBufferUsage;

static void
BufferUsageAdd(BufferUsage *dst, const BufferUsage *add);
static void
BufferUsageAccumDiff(BufferUsage *dst, const BufferUsage *add, const BufferUsage *sub);

                                               
Instrumentation *
InstrAlloc(int n, int instrument_options)
{
  Instrumentation *instr;

                                                              
  instr = palloc0(n * sizeof(Instrumentation));
  if (instrument_options & (INSTRUMENT_BUFFERS | INSTRUMENT_TIMER))
  {
    bool need_buffers = (instrument_options & INSTRUMENT_BUFFERS) != 0;
    bool need_timer = (instrument_options & INSTRUMENT_TIMER) != 0;
    int i;

    for (i = 0; i < n; i++)
    {
      instr[i].need_bufusage = need_buffers;
      instr[i].need_timer = need_timer;
    }
  }

  return instr;
}

                                                           
void
InstrInit(Instrumentation *instr, int instrument_options)
{
  memset(instr, 0, sizeof(Instrumentation));
  instr->need_bufusage = (instrument_options & INSTRUMENT_BUFFERS) != 0;
  instr->need_timer = (instrument_options & INSTRUMENT_TIMER) != 0;
}

                          
void
InstrStartNode(Instrumentation *instr)
{
  if (instr->need_timer && !INSTR_TIME_SET_CURRENT_LAZY(instr->starttime))
  {
    elog(ERROR, "InstrStartNode called twice in a row");
  }

                                                         
  if (instr->need_bufusage)
  {
    instr->bufusage_start = pgBufferUsage;
  }
}

                           
void
InstrStopNode(Instrumentation *instr, double nTuples)
{
  instr_time endtime;

                                 
  instr->tuplecount += nTuples;

                                                             
  if (instr->need_timer)
  {
    if (INSTR_TIME_IS_ZERO(instr->starttime))
    {
      elog(ERROR, "InstrStopNode called without start");
    }

    INSTR_TIME_SET_CURRENT(endtime);
    INSTR_TIME_ACCUM_DIFF(instr->counter, endtime, instr->starttime);

    INSTR_TIME_SET_ZERO(instr->starttime);
  }

                                                              
  if (instr->need_bufusage)
  {
    BufferUsageAccumDiff(&instr->bufusage, &pgBufferUsage, &instr->bufusage_start);
  }

                                              
  if (!instr->running)
  {
    instr->running = true;
    instr->firsttuple = INSTR_TIME_GET_DOUBLE(instr->counter);
  }
}

                                        
void
InstrEndLoop(Instrumentation *instr)
{
  double totaltime;

                                                          
  if (!instr->running)
  {
    return;
  }

  if (!INSTR_TIME_IS_ZERO(instr->starttime))
  {
    elog(ERROR, "InstrEndLoop called on running node");
  }

                                                   
  totaltime = INSTR_TIME_GET_DOUBLE(instr->counter);

  instr->startup += instr->firsttuple;
  instr->total += totaltime;
  instr->ntuples += instr->tuplecount;
  instr->nloops += 1;

                                     
  instr->running = false;
  INSTR_TIME_SET_ZERO(instr->starttime);
  INSTR_TIME_SET_ZERO(instr->counter);
  instr->firsttuple = 0;
  instr->tuplecount = 0;
}

                                           
void
InstrAggNode(Instrumentation *dst, Instrumentation *add)
{
  if (!dst->running && add->running)
  {
    dst->running = true;
    dst->firsttuple = add->firsttuple;
  }
  else if (dst->running && add->running && dst->firsttuple > add->firsttuple)
  {
    dst->firsttuple = add->firsttuple;
  }

  INSTR_TIME_ADD(dst->counter, add->counter);

  dst->tuplecount += add->tuplecount;
  dst->startup += add->startup;
  dst->total += add->total;
  dst->ntuples += add->ntuples;
  dst->ntuples2 += add->ntuples2;
  dst->nloops += add->nloops;
  dst->nfiltered1 += add->nfiltered1;
  dst->nfiltered2 += add->nfiltered2;

                                                              
  if (dst->need_bufusage)
  {
    BufferUsageAdd(&dst->bufusage, &add->bufusage);
  }
}

                                                          
void
InstrStartParallelQuery(void)
{
  save_pgBufferUsage = pgBufferUsage;
}

                                                   
void
InstrEndParallelQuery(BufferUsage *result)
{
  memset(result, 0, sizeof(BufferUsage));
  BufferUsageAccumDiff(result, &pgBufferUsage, &save_pgBufferUsage);
}

                                                       
void
InstrAccumParallelQuery(BufferUsage *result)
{
  BufferUsageAdd(&pgBufferUsage, result);
}

                
static void
BufferUsageAdd(BufferUsage *dst, const BufferUsage *add)
{
  dst->shared_blks_hit += add->shared_blks_hit;
  dst->shared_blks_read += add->shared_blks_read;
  dst->shared_blks_dirtied += add->shared_blks_dirtied;
  dst->shared_blks_written += add->shared_blks_written;
  dst->local_blks_hit += add->local_blks_hit;
  dst->local_blks_read += add->local_blks_read;
  dst->local_blks_dirtied += add->local_blks_dirtied;
  dst->local_blks_written += add->local_blks_written;
  dst->temp_blks_read += add->temp_blks_read;
  dst->temp_blks_written += add->temp_blks_written;
  INSTR_TIME_ADD(dst->blk_read_time, add->blk_read_time);
  INSTR_TIME_ADD(dst->blk_write_time, add->blk_write_time);
}

                      
static void
BufferUsageAccumDiff(BufferUsage *dst, const BufferUsage *add, const BufferUsage *sub)
{
  dst->shared_blks_hit += add->shared_blks_hit - sub->shared_blks_hit;
  dst->shared_blks_read += add->shared_blks_read - sub->shared_blks_read;
  dst->shared_blks_dirtied += add->shared_blks_dirtied - sub->shared_blks_dirtied;
  dst->shared_blks_written += add->shared_blks_written - sub->shared_blks_written;
  dst->local_blks_hit += add->local_blks_hit - sub->local_blks_hit;
  dst->local_blks_read += add->local_blks_read - sub->local_blks_read;
  dst->local_blks_dirtied += add->local_blks_dirtied - sub->local_blks_dirtied;
  dst->local_blks_written += add->local_blks_written - sub->local_blks_written;
  dst->temp_blks_read += add->temp_blks_read - sub->temp_blks_read;
  dst->temp_blks_written += add->temp_blks_written - sub->temp_blks_written;
  INSTR_TIME_ACCUM_DIFF(dst->blk_read_time, add->blk_read_time, sub->blk_read_time);
  INSTR_TIME_ACCUM_DIFF(dst->blk_write_time, add->blk_write_time, sub->blk_write_time);
}
