                                                                            
   
                
                                                       
   
                                                                     
                                                                       
                                                                      
                                                                      
                                                                            
                                                                             
                                                                         
                                                                        
                                                                       
   
                                                                 
                                        
   
                                                                             
                                                                             
                                                                              
                                                                              
                
   
                                                                           
                                                                                
                                                                
   
                                                                             
                                                                             
                                                                             
                                                                           
                                                                           
                                                                               
                                                                               
                                                            
   
                                                                      
                                                                              
                                                                             
                                                                              
                                                                             
                                                                               
   
   
                                                                         
                                                                        
   
                  
                                         
   
                                                                            
   

#include "postgres.h"

#include <limits.h>

#include "access/htup_details.h"
#include "commands/tablespace.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "storage/buffile.h"
#include "utils/memutils.h"
#include "utils/resowner.h"

   
                                                                         
                                                 
   
typedef enum
{
  TSS_INMEM,                                     
  TSS_WRITEFILE,                           
  TSS_READFILE                               
} TupStoreStatus;

   
                                                                           
                                                                        
                                                                          
                                                                           
                                                                                
   
                                                                             
                                                                          
                                                                            
             
   
typedef struct
{
  int eflags;                             
  bool eof_reached;                           
  int current;                                    
  int file;                         
  off_t offset;                              
} TSReadPointer;

   
                                            
   
struct Tuplestorestate
{
  TupStoreStatus status;                                       
  int eflags;                                                           
  bool backward;                                                 
  bool interXact;                                              
  bool truncated;                                                  
  int64 availMem;                                                   
  int64 allowedMem;                                           
  int64 tuples;                                       
  BufFile *myfile;                                              
  MemoryContext context;                                         
  ResourceOwner resowner;                                      

     
                                                                            
                                                                            
                                                           
     
                                                                             
                                                                          
                                        
     
                                                                          
                                                                           
                                                                             
                                                    
     
  void *(*copytup)(Tuplestorestate *state, void *tup);

     
                                                                            
                                                                            
                                                                        
                                                                            
                       
     
  void (*writetup)(Tuplestorestate *state, void *tup);

     
                                                                          
                                                                       
                                                                         
                     
     
  void *(*readtup)(Tuplestorestate *state, unsigned int len);

     
                                                                             
                                                     
     
                                                                          
                                                                            
                                                                             
                                                                           
                                    
     
  void **memtuples;                                             
  int memtupdeleted;                                              
  int memtupcount;                                            
  int memtupsize;                                              
  bool growmemtuples;                                        

     
                                                                      
     
                                                                            
                                                                          
                                                                           
                                                                       
     
  TSReadPointer *readptrs;                             
  int activeptr;                                                 
  int readptrcount;                                                
  int readptrsize;                                                 

  int writepos_file;                                          
  off_t writepos_offset;                                       
};

#define COPYTUP(state, tup) ((*(state)->copytup)(state, tup))
#define WRITETUP(state, tup) ((*(state)->writetup)(state, tup))
#define READTUP(state, len) ((*(state)->readtup)(state, len))
#define LACKMEM(state) ((state)->availMem < 0)
#define USEMEM(state, amt) ((state)->availMem -= (amt))
#define FREEMEM(state, amt) ((state)->availMem += (amt))

                       
   
                                                 
   
                                                                              
                                                                 
                                     
                                                                      
                                                                          
   
                                                                 
                                                                              
                                                                            
                                                                         
                                                                          
                
   
                                                                        
                                                                        
                                                                        
                                      
   
                                                                      
                                                                           
                                                                             
                                                                        
                                                 
   
   
                                                
   
                                                                    
                                                             
                                                                       
                                                                    
   
                                                                          
                                                                       
                                                                          
                                                                         
                                                    
   
                       
   

static Tuplestorestate *
tuplestore_begin_common(int eflags, bool interXact, int maxKBytes);
static void
tuplestore_puttuple_common(Tuplestorestate *state, void *tuple);
static void
dumptuples(Tuplestorestate *state);
static unsigned int
getlen(Tuplestorestate *state, bool eofOK);
static void *
copytup_heap(Tuplestorestate *state, void *tup);
static void
writetup_heap(Tuplestorestate *state, void *tup);
static void *
readtup_heap(Tuplestorestate *state, unsigned int len);

   
                         
   
                                           
   
static Tuplestorestate *
tuplestore_begin_common(int eflags, bool interXact, int maxKBytes)
{
  Tuplestorestate *state;

  state = (Tuplestorestate *)palloc0(sizeof(Tuplestorestate));

  state->status = TSS_INMEM;
  state->eflags = eflags;
  state->interXact = interXact;
  state->truncated = false;
  state->allowedMem = maxKBytes * 1024L;
  state->availMem = state->allowedMem;
  state->myfile = NULL;
  state->context = CurrentMemoryContext;
  state->resowner = CurrentResourceOwner;

  state->memtupdeleted = 0;
  state->memtupcount = 0;
  state->tuples = 0;

     
                                                                          
                                       
     
  state->memtupsize = Max(16384 / sizeof(void *), ALLOCSET_SEPARATE_THRESHOLD / sizeof(void *) + 1);

  state->growmemtuples = true;
  state->memtuples = (void **)palloc(state->memtupsize * sizeof(void *));

  USEMEM(state, GetMemoryChunkSpace(state->memtuples));

  state->activeptr = 0;
  state->readptrcount = 1;
  state->readptrsize = 8;                
  state->readptrs = (TSReadPointer *)palloc(state->readptrsize * sizeof(TSReadPointer));

  state->readptrs[0].eflags = eflags;
  state->readptrs[0].eof_reached = false;
  state->readptrs[0].current = 0;

  return state;
}

   
                         
   
                                                                    
                                                                         
                
   
                                                                    
                            
   
                                                                             
                                                                              
                                                                             
                                                                               
                               
   
                                                                     
                                                           
   
Tuplestorestate *
tuplestore_begin_heap(bool randomAccess, bool interXact, int maxKBytes)
{
  Tuplestorestate *state;
  int eflags;

     
                                                                           
                                          
     
  eflags = randomAccess ? (EXEC_FLAG_BACKWARD | EXEC_FLAG_REWIND) : (EXEC_FLAG_REWIND);

  state = tuplestore_begin_common(eflags, interXact, maxKBytes);

  state->copytup = copytup_heap;
  state->writetup = writetup_heap;
  state->readtup = readtup_heap;

  return state;
}

   
                         
   
                                                                        
                                                                          
                                 
   
                                                                     
                                                                             
                                           
                                            
                                                                           
                                                               
   
                                                                               
                                                                             
                                                           
   
void
tuplestore_set_eflags(Tuplestorestate *state, int eflags)
{
  int i;

  if (state->status != TSS_INMEM || state->memtupcount != 0)
  {
    elog(ERROR, "too late to call tuplestore_set_eflags");
  }

  state->readptrs[0].eflags = eflags;
  for (i = 1; i < state->readptrcount; i++)
  {
    eflags |= state->readptrs[i].eflags;
  }
  state->eflags = eflags;
}

   
                                                                  
   
                                
   
                                                                    
                                                                      
                                                                  
                 
   
int
tuplestore_alloc_read_pointer(Tuplestorestate *state, int eflags)
{
                                                   
  if (state->status != TSS_INMEM || state->memtupcount != 0)
  {
    if ((state->eflags | eflags) != state->eflags)
    {
      elog(ERROR, "too late to require new tuplestore eflags");
    }
  }

                                                    
  if (state->readptrcount >= state->readptrsize)
  {
    int newcnt = state->readptrsize * 2;

    state->readptrs = (TSReadPointer *)repalloc(state->readptrs, newcnt * sizeof(TSReadPointer));
    state->readptrsize = newcnt;
  }

                     
  state->readptrs[state->readptrcount] = state->readptrs[0];
  state->readptrs[state->readptrcount].eflags = eflags;

  state->eflags |= eflags;

  return state->readptrcount++;
}

   
                    
   
                                                                        
                 
   
void
tuplestore_clear(Tuplestorestate *state)
{
  int i;
  TSReadPointer *readptr;

  if (state->myfile)
  {
    BufFileClose(state->myfile);
  }
  state->myfile = NULL;
  if (state->memtuples)
  {
    for (i = state->memtupdeleted; i < state->memtupcount; i++)
    {
      FREEMEM(state, GetMemoryChunkSpace(state->memtuples[i]));
      pfree(state->memtuples[i]);
    }
  }
  state->status = TSS_INMEM;
  state->truncated = false;
  state->memtupdeleted = 0;
  state->memtupcount = 0;
  state->tuples = 0;
  readptr = state->readptrs;
  for (i = 0; i < state->readptrcount; readptr++, i++)
  {
    readptr->eof_reached = false;
    readptr->current = 0;
  }
}

   
                  
   
                                   
   
void
tuplestore_end(Tuplestorestate *state)
{
  int i;

  if (state->myfile)
  {
    BufFileClose(state->myfile);
  }
  if (state->memtuples)
  {
    for (i = state->memtupdeleted; i < state->memtupcount; i++)
    {
      pfree(state->memtuples[i]);
    }
    pfree(state->memtuples);
  }
  pfree(state->readptrs);
  pfree(state);
}

   
                                                                           
   
void
tuplestore_select_read_pointer(Tuplestorestate *state, int ptr)
{
  TSReadPointer *readptr;
  TSReadPointer *oldptr;

  Assert(ptr >= 0 && ptr < state->readptrcount);

                                 
  if (ptr == state->activeptr)
  {
    return;
  }

  readptr = &state->readptrs[ptr];
  oldptr = &state->readptrs[state->activeptr];

  switch (state->status)
  {
  case TSS_INMEM:
  case TSS_WRITEFILE:
                 
    break;
  case TSS_READFILE:

       
                                                                     
                        
       
    if (!oldptr->eof_reached)
    {
      BufFileTell(state->myfile, &oldptr->file, &oldptr->offset);
    }

       
                                                                  
                                                                 
                                                                     
                       
       
    if (readptr->eof_reached)
    {
      if (BufFileSeek(state->myfile, state->writepos_file, state->writepos_offset, SEEK_SET) != 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek in tuplestore temporary file")));
      }
    }
    else
    {
      if (BufFileSeek(state->myfile, readptr->file, readptr->offset, SEEK_SET) != 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek in tuplestore temporary file")));
      }
    }
    break;
  default:
    elog(ERROR, "invalid tuplestore state");
    break;
  }

  state->activeptr = ptr;
}

   
                          
   
                                                                 
                       
   
int64
tuplestore_tuple_count(Tuplestorestate *state)
{
  return state->tuples;
}

   
                    
   
                                                        
   
bool
tuplestore_ateof(Tuplestorestate *state)
{
  return state->readptrs[state->activeptr].eof_reached;
}

   
                                                                             
                                                                          
                                                                           
   
                                                                            
                                                                              
                                                                              
                                                                              
                                                                           
                                                                              
                                                                        
                                                                  
                                    
   
static bool
grow_memtuples(Tuplestorestate *state)
{
  int newmemtupsize;
  int memtupsize = state->memtupsize;
  int64 memNowUsed = state->allowedMem - state->availMem;

                                                                         
  if (!state->growmemtuples)
  {
    return false;
  }

                                      
  if (memNowUsed <= state->availMem)
  {
       
                                                                     
                                   
       
    if (memtupsize < INT_MAX / 2)
    {
      newmemtupsize = memtupsize * 2;
    }
    else
    {
      newmemtupsize = INT_MAX;
      state->growmemtuples = false;
    }
  }
  else
  {
       
                                                                        
                                                               
       
                                                                       
                                                                        
                                                                       
                                                                         
                                                                       
                                                                      
                                                                      
                                                                         
                                                                       
                                
       
                                                                         
                                                                        
                                                                         
                                                                       
       
                                                                        
                                                                        
                                                                 
                                                                        
                                  
       
    double grow_ratio;

    grow_ratio = (double)state->allowedMem / (double)memNowUsed;
    if (memtupsize * grow_ratio < INT_MAX)
    {
      newmemtupsize = (int)(memtupsize * grow_ratio);
    }
    else
    {
      newmemtupsize = INT_MAX;
    }

                                                        
    state->growmemtuples = false;
  }

                                                                       
  if (newmemtupsize <= memtupsize)
  {
    goto noalloc;
  }

     
                                                                           
                                                                       
                                                                           
                                                                         
                                           
     
  if ((Size)newmemtupsize >= MaxAllocHugeSize / sizeof(void *))
  {
    newmemtupsize = (int)(MaxAllocHugeSize / sizeof(void *));
    state->growmemtuples = false;                          
  }

     
                                                                          
                                                                         
                                                                          
                                                                        
                                                                        
                                                                         
                                                                            
                                                                          
                                     
     
  if (state->availMem < (int64)((newmemtupsize - memtupsize) * sizeof(void *)))
  {
    goto noalloc;
  }

                 
  FREEMEM(state, GetMemoryChunkSpace(state->memtuples));
  state->memtupsize = newmemtupsize;
  state->memtuples = (void **)repalloc_huge(state->memtuples, state->memtupsize * sizeof(void *));
  USEMEM(state, GetMemoryChunkSpace(state->memtuples));
  if (LACKMEM(state))
  {
    elog(ERROR, "unexpected out-of-memory situation in tuplestore");
  }
  return true;

noalloc:
                                                                     
  state->growmemtuples = false;
  return false;
}

   
                                                     
   
                                                                            
   
                                                                             
                                                                            
                                                                           
                                                                         
                                                                              
                                                                               
          
   
                                                                           
                                                     
   
void
tuplestore_puttupleslot(Tuplestorestate *state, TupleTableSlot *slot)
{
  MinimalTuple tuple;
  MemoryContext oldcxt = MemoryContextSwitchTo(state->context);

     
                                           
     
  tuple = ExecCopySlotMinimalTuple(slot);
  USEMEM(state, GetMemoryChunkSpace(tuple));

  tuplestore_puttuple_common(state, (void *)tuple);

  MemoryContextSwitchTo(oldcxt);
}

   
                                                                            
                                                                              
   
void
tuplestore_puttuple(Tuplestorestate *state, HeapTuple tuple)
{
  MemoryContext oldcxt = MemoryContextSwitchTo(state->context);

     
                                                                       
                                                           
     
  tuple = COPYTUP(state, tuple);

  tuplestore_puttuple_common(state, (void *)tuple);

  MemoryContextSwitchTo(oldcxt);
}

   
                                                                          
                                                      
   
void
tuplestore_putvalues(Tuplestorestate *state, TupleDesc tdesc, Datum *values, bool *isnull)
{
  MinimalTuple tuple;
  MemoryContext oldcxt = MemoryContextSwitchTo(state->context);

  tuple = heap_form_minimal_tuple(tdesc, values, isnull);
  USEMEM(state, GetMemoryChunkSpace(tuple));

  tuplestore_puttuple_common(state, (void *)tuple);

  MemoryContextSwitchTo(oldcxt);
}

static void
tuplestore_puttuple_common(Tuplestorestate *state, void *tuple)
{
  TSReadPointer *readptr;
  int i;
  ResourceOwner oldowner;

  state->tuples++;

  switch (state->status)
  {
  case TSS_INMEM:

       
                                                           
       
    readptr = state->readptrs;
    for (i = 0; i < state->readptrcount; readptr++, i++)
    {
      if (readptr->eof_reached && i != state->activeptr)
      {
        readptr->eof_reached = false;
        readptr->current = state->memtupcount;
      }
    }

       
                                                                     
                                                                   
                                                                    
                                             
       
    if (state->memtupcount >= state->memtupsize - 1)
    {
      (void)grow_memtuples(state);
      Assert(state->memtupcount < state->memtupsize);
    }

                                                
    state->memtuples[state->memtupcount++] = tuple;

       
                                                                      
       
    if (state->memtupcount < state->memtupsize && !LACKMEM(state))
    {
      return;
    }

       
                                                                     
                                                                  
       
    PrepareTempTablespaces();

                                                            
    oldowner = CurrentResourceOwner;
    CurrentResourceOwner = state->resowner;

    state->myfile = BufFileCreateTemp(state->interXact);

    CurrentResourceOwner = oldowner;

       
                                                                       
                                                                     
                                                  
       
    state->backward = (state->eflags & EXEC_FLAG_BACKWARD) != 0;
    state->status = TSS_WRITEFILE;
    dumptuples(state);
    break;
  case TSS_WRITEFILE:

       
                                                                 
                                                                
                       
       
    readptr = state->readptrs;
    for (i = 0; i < state->readptrcount; readptr++, i++)
    {
      if (readptr->eof_reached && i != state->activeptr)
      {
        readptr->eof_reached = false;
        BufFileTell(state->myfile, &readptr->file, &readptr->offset);
      }
    }

    WRITETUP(state, tuple);
    break;
  case TSS_READFILE:

       
                                       
       
    if (!state->readptrs[state->activeptr].eof_reached)
    {
      BufFileTell(state->myfile, &state->readptrs[state->activeptr].file, &state->readptrs[state->activeptr].offset);
    }
    if (BufFileSeek(state->myfile, state->writepos_file, state->writepos_offset, SEEK_SET) != 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek in tuplestore temporary file")));
    }
    state->status = TSS_WRITEFILE;

       
                                                           
       
    readptr = state->readptrs;
    for (i = 0; i < state->readptrcount; readptr++, i++)
    {
      if (readptr->eof_reached && i != state->activeptr)
      {
        readptr->eof_reached = false;
        readptr->file = state->writepos_file;
        readptr->offset = state->writepos_offset;
      }
    }

    WRITETUP(state, tuple);
    break;
  default:
    elog(ERROR, "invalid tuplestore state");
    break;
  }
}

   
                                                             
                                                               
                                                           
   
                                                                 
                                                                
   
static void *
tuplestore_gettuple(Tuplestorestate *state, bool forward, bool *should_free)
{
  TSReadPointer *readptr = &state->readptrs[state->activeptr];
  unsigned int tuplen;
  void *tup;

  Assert(forward || (readptr->eflags & EXEC_FLAG_BACKWARD));

  switch (state->status)
  {
  case TSS_INMEM:
    *should_free = false;
    if (forward)
    {
      if (readptr->eof_reached)
      {
        return NULL;
      }
      if (readptr->current < state->memtupcount)
      {
                                                 
        return state->memtuples[readptr->current++];
      }
      readptr->eof_reached = true;
      return NULL;
    }
    else
    {
         
                                                               
                                                 
         
      if (readptr->eof_reached)
      {
        readptr->current = state->memtupcount;
        readptr->eof_reached = false;
      }
      else
      {
        if (readptr->current <= state->memtupdeleted)
        {
          Assert(!state->truncated);
          return NULL;
        }
        readptr->current--;                          
      }
      if (readptr->current <= state->memtupdeleted)
      {
        Assert(!state->truncated);
        return NULL;
      }
      return state->memtuples[readptr->current - 1];
    }
    break;

  case TSS_WRITEFILE:
                                                     
    if (readptr->eof_reached && forward)
    {
      return NULL;
    }

       
                                       
       
    BufFileTell(state->myfile, &state->writepos_file, &state->writepos_offset);
    if (!readptr->eof_reached)
    {
      if (BufFileSeek(state->myfile, readptr->file, readptr->offset, SEEK_SET) != 0)
      {
        ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek in tuplestore temporary file")));
      }
    }
    state->status = TSS_READFILE;
                     

  case TSS_READFILE:
    *should_free = true;
    if (forward)
    {
      if ((tuplen = getlen(state, true)) != 0)
      {
        tup = READTUP(state, tuplen);
        return tup;
      }
      else
      {
        readptr->eof_reached = true;
        return NULL;
      }
    }

       
                 
       
                                                                    
                                        
       
                                                                  
                                                            
       
    if (BufFileSeek(state->myfile, 0, -(long)sizeof(unsigned int), SEEK_CUR) != 0)
    {
                                                                   
      readptr->eof_reached = false;
      Assert(!state->truncated);
      return NULL;
    }
    tuplen = getlen(state, false);

    if (readptr->eof_reached)
    {
      readptr->eof_reached = false;
                                                                   
    }
    else
    {
         
                                                               
         
      if (BufFileSeek(state->myfile, 0, -(long)(tuplen + 2 * sizeof(unsigned int)), SEEK_CUR) != 0)
      {
           
                                                                 
                                                                 
                                                                  
                                      
           
        if (BufFileSeek(state->myfile, 0, -(long)(tuplen + sizeof(unsigned int)), SEEK_CUR) != 0)
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek in tuplestore temporary file")));
        }
        Assert(!state->truncated);
        return NULL;
      }
      tuplen = getlen(state, false);
    }

       
                                                                       
                                                                 
                                                           
       
    if (BufFileSeek(state->myfile, 0, -(long)tuplen, SEEK_CUR) != 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek in tuplestore temporary file")));
    }
    tup = READTUP(state, tuplen);
    return tup;

  default:
    elog(ERROR, "invalid tuplestore state");
    return NULL;                          
  }
}

   
                                                                       
   
                                                                          
                     
   
                                                                           
                                                                              
                                                                          
                                                                      
                                                                            
                                                                            
   
bool
tuplestore_gettupleslot(Tuplestorestate *state, bool forward, bool copy, TupleTableSlot *slot)
{
  MinimalTuple tuple;
  bool should_free;

  tuple = (MinimalTuple)tuplestore_gettuple(state, forward, &should_free);

  if (tuple)
  {
    if (copy && !should_free)
    {
      tuple = heap_copy_minimal_tuple(tuple);
      should_free = true;
    }
    ExecStoreMinimalTuple(tuple, slot, should_free);
    return true;
  }
  else
  {
    ExecClearTuple(slot);
    return false;
  }
}

   
                                                                              
   
                                                                           
                                      
   
bool
tuplestore_advance(Tuplestorestate *state, bool forward)
{
  void *tuple;
  bool should_free;

  tuple = tuplestore_gettuple(state, forward, &should_free);

  if (tuple)
  {
    if (should_free)
    {
      pfree(tuple);
    }
    return true;
  }
  else
  {
    return false;
  }
}

   
                                                              
                                                 
                                                           
   
bool
tuplestore_skiptuples(Tuplestorestate *state, int64 ntuples, bool forward)
{
  TSReadPointer *readptr = &state->readptrs[state->activeptr];

  Assert(forward || (readptr->eflags & EXEC_FLAG_BACKWARD));

  if (ntuples <= 0)
  {
    return true;
  }

  switch (state->status)
  {
  case TSS_INMEM:
    if (forward)
    {
      if (readptr->eof_reached)
      {
        return false;
      }
      if (state->memtupcount - readptr->current >= ntuples)
      {
        readptr->current += ntuples;
        return true;
      }
      readptr->current = state->memtupcount;
      readptr->eof_reached = true;
      return false;
    }
    else
    {
      if (readptr->eof_reached)
      {
        readptr->current = state->memtupcount;
        readptr->eof_reached = false;
        ntuples--;
      }
      if (readptr->current - state->memtupdeleted > ntuples)
      {
        readptr->current -= ntuples;
        return true;
      }
      Assert(!state->truncated);
      readptr->current = state->memtupdeleted;
      return false;
    }
    break;

  default:
                                                             
    while (ntuples-- > 0)
    {
      void *tuple;
      bool should_free;

      tuple = tuplestore_gettuple(state, forward, &should_free);

      if (tuple == NULL)
      {
        return false;
      }
      if (should_free)
      {
        pfree(tuple);
      }
      CHECK_FOR_INTERRUPTS();
    }
    return true;
  }
}

   
                                                            
   
                                                                       
                                                                    
                         
   
static void
dumptuples(Tuplestorestate *state)
{
  int i;

  for (i = state->memtupdeleted;; i++)
  {
    TSReadPointer *readptr = state->readptrs;
    int j;

    for (j = 0; j < state->readptrcount; readptr++, j++)
    {
      if (i == readptr->current && !readptr->eof_reached)
      {
        BufFileTell(state->myfile, &readptr->file, &readptr->offset);
      }
    }
    if (i >= state->memtupcount)
    {
      break;
    }
    WRITETUP(state, state->memtuples[i]);
  }
  state->memtupdeleted = 0;
  state->memtupcount = 0;
}

   
                                                                
   
void
tuplestore_rescan(Tuplestorestate *state)
{
  TSReadPointer *readptr = &state->readptrs[state->activeptr];

  Assert(readptr->eflags & EXEC_FLAG_REWIND);
  Assert(!state->truncated);

  switch (state->status)
  {
  case TSS_INMEM:
    readptr->eof_reached = false;
    readptr->current = 0;
    break;
  case TSS_WRITEFILE:
    readptr->eof_reached = false;
    readptr->file = 0;
    readptr->offset = 0L;
    break;
  case TSS_READFILE:
    readptr->eof_reached = false;
    if (BufFileSeek(state->myfile, 0, 0L, SEEK_SET) != 0)
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek in tuplestore temporary file")));
    }
    break;
  default:
    elog(ERROR, "invalid tuplestore state");
    break;
  }
}

   
                                                                         
   
void
tuplestore_copy_read_pointer(Tuplestorestate *state, int srcptr, int destptr)
{
  TSReadPointer *sptr = &state->readptrs[srcptr];
  TSReadPointer *dptr = &state->readptrs[destptr];

  Assert(srcptr >= 0 && srcptr < state->readptrcount);
  Assert(destptr >= 0 && destptr < state->readptrcount);

                                    
  if (srcptr == destptr)
  {
    return;
  }

  if (dptr->eflags != sptr->eflags)
  {
                                                                       
    int eflags;
    int i;

    *dptr = *sptr;
    eflags = state->readptrs[0].eflags;
    for (i = 1; i < state->readptrcount; i++)
    {
      eflags |= state->readptrs[i].eflags;
    }
    state->eflags = eflags;
  }
  else
  {
    *dptr = *sptr;
  }

  switch (state->status)
  {
  case TSS_INMEM:
  case TSS_WRITEFILE:
                 
    break;
  case TSS_READFILE:

       
                                                                 
                                                                  
                                                                
                                                              
                    
       
    if (destptr == state->activeptr)
    {
      if (dptr->eof_reached)
      {
        if (BufFileSeek(state->myfile, state->writepos_file, state->writepos_offset, SEEK_SET) != 0)
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek in tuplestore temporary file")));
        }
      }
      else
      {
        if (BufFileSeek(state->myfile, dptr->file, dptr->offset, SEEK_SET) != 0)
        {
          ereport(ERROR, (errcode_for_file_access(), errmsg("could not seek in tuplestore temporary file")));
        }
      }
    }
    else if (srcptr == state->activeptr)
    {
      if (!dptr->eof_reached)
      {
        BufFileTell(state->myfile, &dptr->file, &dptr->offset);
      }
    }
    break;
  default:
    elog(ERROR, "invalid tuplestore state");
    break;
  }
}

   
                                                        
   
                                                                        
                                                                             
                      
   
                                                                              
                                                                            
                                                                          
            
   
void
tuplestore_trim(Tuplestorestate *state)
{
  int oldest;
  int nremove;
  int i;

     
                                                                  
                 
     
  if (state->eflags & EXEC_FLAG_REWIND)
  {
    return;
  }

     
                                                                          
                                                                           
     
  if (state->status != TSS_INMEM)
  {
    return;
  }

                                    
  oldest = state->memtupcount;
  for (i = 0; i < state->readptrcount; i++)
  {
    if (!state->readptrs[i].eof_reached)
    {
      oldest = Min(oldest, state->readptrs[i].current);
    }
  }

     
                                                                            
                                                                           
                                                                          
                                                                         
                                                                       
                                                                             
                                                                        
                                                           
     
  nremove = oldest - 1;
  if (nremove <= 0)
  {
    return;                    
  }

  Assert(nremove >= state->memtupdeleted);
  Assert(nremove <= state->memtupcount);

                                       
  for (i = state->memtupdeleted; i < nremove; i++)
  {
    FREEMEM(state, GetMemoryChunkSpace(state->memtuples[i]));
    pfree(state->memtuples[i]);
    state->memtuples[i] = NULL;
  }
  state->memtupdeleted = nremove;

                                                                       
  state->truncated = true;

     
                                                                            
                                                                           
                                                                         
                                                                             
     
  if (nremove < state->memtupcount / 8)
  {
    return;
  }

     
                                                 
     
                                                                            
                                                              
     
  if (nremove + 1 == state->memtupcount)
  {
    state->memtuples[0] = state->memtuples[nremove];
  }
  else
  {
    memmove(state->memtuples, state->memtuples + nremove, (state->memtupcount - nremove) * sizeof(void *));
  }

  state->memtupdeleted = 0;
  state->memtupcount -= nremove;
  for (i = 0; i < state->readptrcount; i++)
  {
    if (!state->readptrs[i].eof_reached)
    {
      state->readptrs[i].current -= nremove;
    }
  }
}

   
                        
   
                                                           
   
                                                                            
   
bool
tuplestore_in_memory(Tuplestorestate *state)
{
  return (state->status == TSS_INMEM);
}

   
                           
   

static unsigned int
getlen(Tuplestorestate *state, bool eofOK)
{
  unsigned int len;
  size_t nbytes;

  nbytes = BufFileRead(state->myfile, (void *)&len, sizeof(len));
  if (nbytes == sizeof(len))
  {
    return len;
  }
  if (nbytes != 0 || !eofOK)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from tuplestore temporary file: read only %zu of %zu bytes", nbytes, sizeof(len))));
  }
  return 0;
}

   
                                           
   
                                                                          
                                                      
   
                                                                          
                             
   

static void *
copytup_heap(Tuplestorestate *state, void *tup)
{
  MinimalTuple tuple;

  tuple = minimal_tuple_from_heap_tuple((HeapTuple)tup);
  USEMEM(state, GetMemoryChunkSpace(tuple));
  return (void *)tuple;
}

static void
writetup_heap(Tuplestorestate *state, void *tup)
{
  MinimalTuple tuple = (MinimalTuple)tup;

                                                 
  char *tupbody = (char *)tuple + MINIMAL_TUPLE_DATA_OFFSET;
  unsigned int tupbodylen = tuple->t_len - MINIMAL_TUPLE_DATA_OFFSET;

                                
  unsigned int tuplen = tupbodylen + sizeof(int);

  BufFileWrite(state->myfile, (void *)&tuplen, sizeof(tuplen));
  BufFileWrite(state->myfile, (void *)tupbody, tupbodylen);
  if (state->backward)                                 
  {
    BufFileWrite(state->myfile, (void *)&tuplen, sizeof(tuplen));
  }

  FREEMEM(state, GetMemoryChunkSpace(tuple));
  heap_free_minimal_tuple(tuple);
}

static void *
readtup_heap(Tuplestorestate *state, unsigned int len)
{
  unsigned int tupbodylen = len - sizeof(int);
  unsigned int tuplen = tupbodylen + MINIMAL_TUPLE_DATA_OFFSET;
  MinimalTuple tuple = (MinimalTuple)palloc(tuplen);
  char *tupbody = (char *)tuple + MINIMAL_TUPLE_DATA_OFFSET;
  size_t nread;

  USEMEM(state, GetMemoryChunkSpace(tuple));
                                
  tuple->t_len = tuplen;
  nread = BufFileRead(state->myfile, (void *)tupbody, tupbodylen);
  if (nread != (size_t)tupbodylen)
  {
    ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from tuplestore temporary file: read only %zu of %zu bytes", nread, (size_t)tupbodylen)));
  }
  if (state->backward)                                 
  {
    nread = BufFileRead(state->myfile, (void *)&tuplen, sizeof(tuplen));
    if (nread != sizeof(tuplen))
    {
      ereport(ERROR, (errcode_for_file_access(), errmsg("could not read from tuplestore temporary file: read only %zu of %zu bytes", nread, sizeof(tuplen))));
    }
  }
  return (void *)tuple;
}
