                                                                            
   
               
                                         
   
                                                                       
                                                                     
                                                                         
                                                                      
                                                                         
              
   
                                                                          
                                                                           
                                                                           
                                                                       
                                                                           
                                                                          
                                                                          
                                                        
   
                                                                       
                                                                             
                                                                          
                                                                          
                                                                              
                                                                          
                                                                         
                                                                          
                                                                            
                                                                              
                                          
   
                                                                             
                                                                           
                                                                              
                                                                            
                                                                          
                                                                          
                                                                               
                                                                               
                                                                              
                                                                       
                                                                            
                                                                              
                                                                          
                                                                             
                                                                               
                                                                             
                                                                              
                                                                             
                                                                           
                                       
   
                                                                      
                                                                     
                                                                         
                                                                         
                                                                   
                                                                    
                                                                          
   
                                                                            
                                                                             
                                                                          
                                                                              
                                                                             
                                                                            
                                                                            
                                                                             
                                                                            
                                                                             
                                                                              
                                                                           
                                                           
   
                                                                               
                                                                               
                                                                  
                                                                           
                                                                     
                                                                             
                                                            
   
   
                                                                         
                                                                        
   
                  
                                        
   
                                                                            
   

#include "postgres.h"

#include <limits.h>

#include "access/hash.h"
#include "access/htup_details.h"
#include "access/nbtree.h"
#include "catalog/index.h"
#include "catalog/pg_am.h"
#include "commands/tablespace.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "pg_trace.h"
#include "utils/datum.h"
#include "utils/logtape.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_rusage.h"
#include "utils/rel.h"
#include "utils/sortsupport.h"
#include "utils/tuplesort.h"

                                            
#define HEAP_SORT 0
#define INDEX_SORT 1
#define DATUM_SORT 2
#define CLUSTER_SORT 3

                                                          
#define PARALLEL_SORT(state) ((state)->shared == NULL ? 0 : (state)->worker >= 0 ? 1 : 2)

                   
#ifdef TRACE_SORT
bool trace_sort = false;
#endif

#ifdef DEBUG_BOUNDED_SORT
bool optimize_bounded_sort = true;
#endif

   
                                                                      
                                                                          
                                                                           
                                                                        
                                                                          
                                                          
   
                                                                           
                                                                          
                                                                           
                                                                            
                                                                         
                                                                             
   
                                                                               
                                                                            
                                                                                
                                                                
   
                                                                         
                                                                                
                                                                               
                                                                                
                                                                         
                                                                          
                                                                  
   
                                                                             
                             
   
typedef struct
{
  void *tuple;                        
  Datum datum1;                                
  bool isnull1;                                
  int tupindex;                      
} SortTuple;

   
                                                                        
                                            
   
                                                                          
                                                                           
                                                 
   
                                                                              
                       
   
#define SLAB_SLOT_SIZE 1024

typedef union SlabSlot
{
  union SlabSlot *nextfree;
  char buffer[SLAB_SLOT_SIZE];
} SlabSlot;

   
                                                                        
                                                
   
typedef enum
{
  TSS_INITIAL,                                                     
  TSS_BOUNDED,                                                 
  TSS_BUILDRUNS,                                         
  TSS_SORTEDINMEM,                                         
  TSS_SORTEDONTAPE,                                           
  TSS_FINALMERGE                                           
} TupSortStatus;

   
                                                                            
                                
   
                                                                            
                                                                           
                                                                    
   
                                                                        
                                                                
   
#define MINORDER 6                            
#define MAXORDER 500                          
#define TAPE_BUFFER_OVERHEAD BLCKSZ
#define MERGE_BUFFER_SIZE (BLCKSZ * 32)

typedef int (*SortTupleComparator)(const SortTuple *a, const SortTuple *b, Tuplesortstate *state);

   
                                           
   
struct Tuplesortstate
{
  TupSortStatus status;                                            
  int nKeys;                                                     
  bool randomAccess;                                                 
  bool bounded;                                                         
                                                     
  bool boundUsed;                                                        
  int bound;                                                                
  bool tuples;                                                      
  int64 availMem;                                                       
  int64 allowedMem;                                               
  int maxTapes;                                                
  int tapeRange;                                          
  MemoryContext sortcontext;                                             
  MemoryContext tuplecontext;                                                
  LogicalTapeSet *tapeset;                                                   

     
                                                                            
                                                                           
                                                          
     
                                                                           
                                                               
                           
     
  SortTupleComparator comparetup;

     
                                                                            
                                                                         
                                                                           
                                                                   
     
  void (*copytup)(Tuplesortstate *state, SortTuple *stup, void *tup);

     
                                                                            
                                                                            
                                                                            
                                                                          
                                                                       
                                    
     
  void (*writetup)(Tuplesortstate *state, int tapenum, SortTuple *stup);

     
                                                                          
                                                                          
                                                                      
     
  void (*readtup)(Tuplesortstate *state, SortTuple *stup, int tapenum, unsigned int len);

     
                                                                         
                                                                        
                                                                            
                                                                            
                                                       
     
  SortTuple *memtuples;                                 
  int memtupcount;                                              
  int memtupsize;                                                
  bool growmemtuples;                                          

     
                                                                             
                                                                         
                                                                       
                                                                 
                                                                           
                         
     
                                                                            
                                                                         
                                                                           
                                                                           
                                    
     
                                                                             
                                                                           
                                                                          
              
     
                                                                             
                                                                    
                                                              
                               
     
                                                                      
                                        
     
  bool slabAllocatorUsed;

  char *slabMemoryBegin;                                      
  char *slabMemoryEnd;                                  
  SlabSlot *slabFreeHead;                        

                                                                 
  size_t read_buffer_size;

     
                                                                          
                                                                      
                                                                          
                                               
     
  void *lastReturnedTuple;

     
                                                                         
                                                           
     
  int currentRun;

     
                                                                         
                                                       
     

     
                                                                             
                                                                             
                             
     
  bool *mergeactive;                               

     
                                                                        
                                                                        
                                                   
     
  int Level;                      
  int destTape;                                                 
  int *tp_fib;                                            
  int *tp_runs;                                     
  int *tp_dummy;                                            
  int *tp_tapenum;                                   
  int activeTapes;                                            

     
                                                                           
                                                                           
                                       
     
  int result_tape;                                             
  int current;                                                  
  bool eof_reached;                                       

                                                              
  long markpos_block;                                              
  int markpos_offset;                                               
  bool markpos_eof;                            

     
                                                       
     
                                                                           
                                                                    
                                                      
     
                                                                        
                     
     
                                                                        
                                                                         
                                                                            
                                                                           
     
  int worker;
  Sharedsort *shared;
  int nParticipants;

     
                                                                           
                                                                          
                                                
     
  TupleDesc tupDesc;
  SortSupport sortKeys;                            

     
                                                                         
                                                                     
     
  SortSupport onlyKey;

     
                                                                          
                                                                            
                                                                       
             
     
  int64 abbrevNext;                                   
                                       

     
                                                                       
                              
     
  IndexInfo *indexInfo;                                                
  EState *estate;                                             

     
                                                                          
                                                                         
     
  Relation heapRel;                                         
  Relation indexRel;                        

                                                      
  bool enforceUnique;                                           

                                                     
  uint32 high_mask;                                           
  uint32 low_mask;
  uint32 max_buckets;

     
                                                                     
                                                                     
     
  Oid datumType;
                                                                
  int datumTypeLen;

     
                                               
     
#ifdef TRACE_SORT
  PGRUsage ru_start;
#endif
};

   
                                                                             
                     
   
struct Sharedsort
{
                                                
  slock_t mutex;

     
                                                                          
                                                           
     
                                                                             
                                                                           
                        
     
  int currentWorker;
  int workersFinished;

                            
  SharedFileSet fileset;

                                    
  int nTapes;

     
                                                                          
                                                                 
     
  TapeShare tapes[FLEXIBLE_ARRAY_MEMBER];
};

   
                                                            
   
#define IS_SLAB_SLOT(state, tuple) ((char *)(tuple) >= (state)->slabMemoryBegin && (char *)(tuple) < (state)->slabMemoryEnd)

   
                                                                   
                       
   
#define RELEASE_SLAB_SLOT(state, tuple)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    SlabSlot *buf = (SlabSlot *)tuple;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
    if (IS_SLAB_SLOT((state), buf))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      buf->nextfree = (state)->slabFreeHead;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
      (state)->slabFreeHead = buf;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     \
    }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
    else                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
      pfree(buf);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
  } while (0)

#define COMPARETUP(state, a, b) ((*(state)->comparetup)(a, b, state))
#define COPYTUP(state, stup, tup) ((*(state)->copytup)(state, stup, tup))
#define WRITETUP(state, tape, stup) ((*(state)->writetup)(state, tape, stup))
#define READTUP(state, stup, tape, len) ((*(state)->readtup)(state, stup, tape, len))
#define LACKMEM(state) ((state)->availMem < 0 && !(state)->slabAllocatorUsed)
#define USEMEM(state, amt) ((state)->availMem -= (amt))
#define FREEMEM(state, amt) ((state)->availMem += (amt))
#define SERIAL(state) ((state)->shared == NULL)
#define WORKER(state) ((state)->shared && (state)->worker != -1)
#define LEADER(state) ((state)->shared && (state)->worker == -1)

   
                                                 
   
                                                                              
                                                                            
                                                                             
                                                                      
                                                                          
   
                                                                         
                                                                          
                                                                            
                                                                         
                                                                        
                
   
                                                                        
                                                                        
                                                                        
                                      
   
                                                                      
                                                                           
                                                                             
                                                                        
                                                 
   
   
                                                
   
                                                                       
                                                                          
                                                            
   
                                                                          
                                                                       
                                                                          
                                                                         
                                                                  
                                                                        
                                                                         
                                                                          
                                                                        
                                                                    
                        
   

                                                               
#define LogicalTapeReadExact(tapeset, tapenum, ptr, len)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (LogicalTapeRead(tapeset, tapenum, ptr, len) != (size_t)(len))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  \
      elog(ERROR, "unexpected end of data");                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           \
  } while (0)

static Tuplesortstate *
tuplesort_begin_common(int workMem, SortCoordinate coordinate, bool randomAccess);
static void
puttuple_common(Tuplesortstate *state, SortTuple *tuple);
static bool
consider_abort_common(Tuplesortstate *state);
static void
inittapes(Tuplesortstate *state, bool mergeruns);
static void
inittapestate(Tuplesortstate *state, int maxTapes);
static void
selectnewtape(Tuplesortstate *state);
static void
init_slab_allocator(Tuplesortstate *state, int numSlots);
static void
mergeruns(Tuplesortstate *state);
static void
mergeonerun(Tuplesortstate *state);
static void
beginmerge(Tuplesortstate *state);
static bool
mergereadnext(Tuplesortstate *state, int srcTape, SortTuple *stup);
static void
dumptuples(Tuplesortstate *state, bool alltuples);
static void
make_bounded_heap(Tuplesortstate *state);
static void
sort_bounded_heap(Tuplesortstate *state);
static void
tuplesort_sort_memtuples(Tuplesortstate *state);
static void
tuplesort_heap_insert(Tuplesortstate *state, SortTuple *tuple);
static void
tuplesort_heap_replace_top(Tuplesortstate *state, SortTuple *tuple);
static void
tuplesort_heap_delete_top(Tuplesortstate *state);
static void
reversedirection(Tuplesortstate *state);
static unsigned int
getlen(Tuplesortstate *state, int tapenum, bool eofOK);
static void
markrunend(Tuplesortstate *state, int tapenum);
static void *
readtup_alloc(Tuplesortstate *state, Size tuplen);
static int
comparetup_heap(const SortTuple *a, const SortTuple *b, Tuplesortstate *state);
static void
copytup_heap(Tuplesortstate *state, SortTuple *stup, void *tup);
static void
writetup_heap(Tuplesortstate *state, int tapenum, SortTuple *stup);
static void
readtup_heap(Tuplesortstate *state, SortTuple *stup, int tapenum, unsigned int len);
static int
comparetup_cluster(const SortTuple *a, const SortTuple *b, Tuplesortstate *state);
static void
copytup_cluster(Tuplesortstate *state, SortTuple *stup, void *tup);
static void
writetup_cluster(Tuplesortstate *state, int tapenum, SortTuple *stup);
static void
readtup_cluster(Tuplesortstate *state, SortTuple *stup, int tapenum, unsigned int len);
static int
comparetup_index_btree(const SortTuple *a, const SortTuple *b, Tuplesortstate *state);
static int
comparetup_index_hash(const SortTuple *a, const SortTuple *b, Tuplesortstate *state);
static void
copytup_index(Tuplesortstate *state, SortTuple *stup, void *tup);
static void
writetup_index(Tuplesortstate *state, int tapenum, SortTuple *stup);
static void
readtup_index(Tuplesortstate *state, SortTuple *stup, int tapenum, unsigned int len);
static int
comparetup_datum(const SortTuple *a, const SortTuple *b, Tuplesortstate *state);
static void
copytup_datum(Tuplesortstate *state, SortTuple *stup, void *tup);
static void
writetup_datum(Tuplesortstate *state, int tapenum, SortTuple *stup);
static void
readtup_datum(Tuplesortstate *state, SortTuple *stup, int tapenum, unsigned int len);
static int
worker_get_identifier(Tuplesortstate *state);
static void
worker_freeze_result_tape(Tuplesortstate *state);
static void
worker_nomergeruns(Tuplesortstate *state);
static void
leader_takeover_tapes(Tuplesortstate *state);
static void
free_sort_tuple(Tuplesortstate *state, SortTuple *stup);

   
                                                                              
                                                                         
                                                                          
                                                                           
                    
   
#include "qsort_tuple.c"

   
                        
   
                                          
   
                                                                          
                                                                           
                                                                         
                                                                              
                                                                              
                                                                                
   
                                                                          
                                                                           
                                                                         
                                                                             
                                                                      
   

static Tuplesortstate *
tuplesort_begin_common(int workMem, SortCoordinate coordinate, bool randomAccess)
{
  Tuplesortstate *state;
  MemoryContext sortcontext;
  MemoryContext tuplecontext;
  MemoryContext oldcontext;

                                                                   
  if (coordinate && randomAccess)
  {
    elog(ERROR, "random access disallowed under parallel sort");
  }

     
                                                                       
                                                       
     
  sortcontext = AllocSetContextCreate(CurrentMemoryContext, "TupleSort main", ALLOCSET_DEFAULT_SIZES);

     
                                                    
     
                                                                         
                                                               
                                                                             
                                                                          
                           
     
  tuplecontext = AllocSetContextCreate(sortcontext, "Caller tuples", ALLOCSET_DEFAULT_SIZES);

     
                                                                        
                                                                 
     
  oldcontext = MemoryContextSwitchTo(sortcontext);

  state = (Tuplesortstate *)palloc0(sizeof(Tuplesortstate));

#ifdef TRACE_SORT
  if (trace_sort)
  {
    pg_rusage_init(&state->ru_start);
  }
#endif

  state->status = TSS_INITIAL;
  state->randomAccess = randomAccess;
  state->bounded = false;
  state->tuples = true;
  state->boundUsed = false;

     
                                                                            
                                                                            
                                                                         
                              
     
  state->allowedMem = Max(workMem, 64) * (int64)1024;
  state->availMem = state->allowedMem;
  state->sortcontext = sortcontext;
  state->tuplecontext = tuplecontext;
  state->tapeset = NULL;

  state->memtupcount = 0;

     
                                                                          
                                       
     
  state->memtupsize = Max(1024, ALLOCSET_SEPARATE_THRESHOLD / sizeof(SortTuple) + 1);

  state->growmemtuples = true;
  state->slabAllocatorUsed = false;
  state->memtuples = (SortTuple *)palloc(state->memtupsize * sizeof(SortTuple));

  USEMEM(state, GetMemoryChunkSpace(state->memtuples));

                                                                    
  if (LACKMEM(state))
  {
    elog(ERROR, "insufficient memory allowed for sort");
  }

  state->currentRun = 0;

     
                                                                           
                            
     

  state->result_tape = -1;                                                

     
                                                                         
                 
     
  if (!coordinate)
  {
                     
    state->shared = NULL;
    state->worker = -1;
    state->nParticipants = -1;
  }
  else if (coordinate->isWorker)
  {
                                                                       
    state->shared = coordinate->sharedsort;
    state->worker = worker_get_identifier(state);
    state->nParticipants = -1;
  }
  else
  {
                                                         
    state->shared = coordinate->sharedsort;
    state->worker = -1;
    state->nParticipants = coordinate->nParticipants;
    Assert(state->nParticipants >= 1);
  }

  MemoryContextSwitchTo(oldcontext);

  return state;
}

Tuplesortstate *
tuplesort_begin_heap(TupleDesc tupDesc, int nkeys, AttrNumber *attNums, Oid *sortOperators, Oid *sortCollations, bool *nullsFirstFlags, int workMem, SortCoordinate coordinate, bool randomAccess)
{
  Tuplesortstate *state = tuplesort_begin_common(workMem, coordinate, randomAccess);
  MemoryContext oldcontext;
  int i;

  oldcontext = MemoryContextSwitchTo(state->sortcontext);

  AssertArg(nkeys > 0);

#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG, "begin tuple sort: nkeys = %d, workMem = %d, randomAccess = %c", nkeys, workMem, randomAccess ? 't' : 'f');
  }
#endif

  state->nKeys = nkeys;

  TRACE_POSTGRESQL_SORT_START(HEAP_SORT, false,                      
      nkeys, workMem, randomAccess, PARALLEL_SORT(state));

  state->comparetup = comparetup_heap;
  state->copytup = copytup_heap;
  state->writetup = writetup_heap;
  state->readtup = readtup_heap;

  state->tupDesc = tupDesc;                                      
  state->abbrevNext = 10;

                                                
  state->sortKeys = (SortSupport)palloc0(nkeys * sizeof(SortSupportData));

  for (i = 0; i < nkeys; i++)
  {
    SortSupport sortKey = state->sortKeys + i;

    AssertArg(attNums[i] != 0);
    AssertArg(sortOperators[i] != 0);

    sortKey->ssup_cxt = CurrentMemoryContext;
    sortKey->ssup_collation = sortCollations[i];
    sortKey->ssup_nulls_first = nullsFirstFlags[i];
    sortKey->ssup_attno = attNums[i];
                                                                        
    sortKey->abbreviate = (i == 0);

    PrepareSortSupportFromOrderingOp(sortOperators[i], sortKey);
  }

     
                                                                            
                                                                           
                                                                         
                                                                  
     
  if (nkeys == 1 && !state->sortKeys->abbrev_converter)
  {
    state->onlyKey = state->sortKeys;
  }

  MemoryContextSwitchTo(oldcontext);

  return state;
}

Tuplesortstate *
tuplesort_begin_cluster(TupleDesc tupDesc, Relation indexRel, int workMem, SortCoordinate coordinate, bool randomAccess)
{
  Tuplesortstate *state = tuplesort_begin_common(workMem, coordinate, randomAccess);
  AttrNumber leading;
  BTScanInsert indexScanKey;
  MemoryContext oldcontext;
  int i;

  Assert(indexRel->rd_rel->relam == BTREE_AM_OID);

  oldcontext = MemoryContextSwitchTo(state->sortcontext);

#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG, "begin tuple sort: nkeys = %d, workMem = %d, randomAccess = %c", RelationGetNumberOfAttributes(indexRel), workMem, randomAccess ? 't' : 'f');
  }
#endif

  state->nKeys = IndexRelationGetNumberOfKeyAttributes(indexRel);

  TRACE_POSTGRESQL_SORT_START(CLUSTER_SORT, false,                      
      state->nKeys, workMem, randomAccess, PARALLEL_SORT(state));

  state->comparetup = comparetup_cluster;
  state->copytup = copytup_cluster;
  state->writetup = writetup_cluster;
  state->readtup = readtup_cluster;
  state->abbrevNext = 10;

  state->indexInfo = BuildIndexInfo(indexRel);
  leading = state->indexInfo->ii_IndexAttrNumbers[0];

  state->tupDesc = tupDesc;                                      

  indexScanKey = _bt_mkscankey(indexRel, NULL);

  if (state->indexInfo->ii_Expressions != NULL)
  {
    TupleTableSlot *slot;
    ExprContext *econtext;

       
                                                                
                                                                 
                                                                    
                                                 
       
    state->estate = CreateExecutorState();
    slot = MakeSingleTupleTableSlot(tupDesc, &TTSOpsHeapTuple);
    econtext = GetPerTupleExprContext(state->estate);
    econtext->ecxt_scantuple = slot;
  }

                                                
  state->sortKeys = (SortSupport)palloc0(state->nKeys * sizeof(SortSupportData));

  for (i = 0; i < state->nKeys; i++)
  {
    SortSupport sortKey = state->sortKeys + i;
    ScanKey scanKey = indexScanKey->scankeys + i;
    int16 strategy;

    sortKey->ssup_cxt = CurrentMemoryContext;
    sortKey->ssup_collation = scanKey->sk_collation;
    sortKey->ssup_nulls_first = (scanKey->sk_flags & SK_BT_NULLS_FIRST) != 0;
    sortKey->ssup_attno = scanKey->sk_attno;
                                                                        
    sortKey->abbreviate = (i == 0 && leading != 0);

    AssertState(sortKey->ssup_attno != 0);

    strategy = (scanKey->sk_flags & SK_BT_DESC) != 0 ? BTGreaterStrategyNumber : BTLessStrategyNumber;

    PrepareSortSupportFromIndexRel(indexRel, strategy, sortKey);
  }

  pfree(indexScanKey);

  MemoryContextSwitchTo(oldcontext);

  return state;
}

Tuplesortstate *
tuplesort_begin_index_btree(Relation heapRel, Relation indexRel, bool enforceUnique, int workMem, SortCoordinate coordinate, bool randomAccess)
{
  Tuplesortstate *state = tuplesort_begin_common(workMem, coordinate, randomAccess);
  BTScanInsert indexScanKey;
  MemoryContext oldcontext;
  int i;

  oldcontext = MemoryContextSwitchTo(state->sortcontext);

#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG, "begin index sort: unique = %c, workMem = %d, randomAccess = %c", enforceUnique ? 't' : 'f', workMem, randomAccess ? 't' : 'f');
  }
#endif

  state->nKeys = IndexRelationGetNumberOfKeyAttributes(indexRel);

  TRACE_POSTGRESQL_SORT_START(INDEX_SORT, enforceUnique, state->nKeys, workMem, randomAccess, PARALLEL_SORT(state));

  state->comparetup = comparetup_index_btree;
  state->copytup = copytup_index;
  state->writetup = writetup_index;
  state->readtup = readtup_index;
  state->abbrevNext = 10;

  state->heapRel = heapRel;
  state->indexRel = indexRel;
  state->enforceUnique = enforceUnique;

  indexScanKey = _bt_mkscankey(indexRel, NULL);

                                                
  state->sortKeys = (SortSupport)palloc0(state->nKeys * sizeof(SortSupportData));

  for (i = 0; i < state->nKeys; i++)
  {
    SortSupport sortKey = state->sortKeys + i;
    ScanKey scanKey = indexScanKey->scankeys + i;
    int16 strategy;

    sortKey->ssup_cxt = CurrentMemoryContext;
    sortKey->ssup_collation = scanKey->sk_collation;
    sortKey->ssup_nulls_first = (scanKey->sk_flags & SK_BT_NULLS_FIRST) != 0;
    sortKey->ssup_attno = scanKey->sk_attno;
                                                                        
    sortKey->abbreviate = (i == 0);

    AssertState(sortKey->ssup_attno != 0);

    strategy = (scanKey->sk_flags & SK_BT_DESC) != 0 ? BTGreaterStrategyNumber : BTLessStrategyNumber;

    PrepareSortSupportFromIndexRel(indexRel, strategy, sortKey);
  }

  pfree(indexScanKey);

  MemoryContextSwitchTo(oldcontext);

  return state;
}

Tuplesortstate *
tuplesort_begin_index_hash(Relation heapRel, Relation indexRel, uint32 high_mask, uint32 low_mask, uint32 max_buckets, int workMem, SortCoordinate coordinate, bool randomAccess)
{
  Tuplesortstate *state = tuplesort_begin_common(workMem, coordinate, randomAccess);
  MemoryContext oldcontext;

  oldcontext = MemoryContextSwitchTo(state->sortcontext);

#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG,
        "begin index sort: high_mask = 0x%x, low_mask = 0x%x, "
        "max_buckets = 0x%x, workMem = %d, randomAccess = %c",
        high_mask, low_mask, max_buckets, workMem, randomAccess ? 't' : 'f');
  }
#endif

  state->nKeys = 1;                                          

  state->comparetup = comparetup_index_hash;
  state->copytup = copytup_index;
  state->writetup = writetup_index;
  state->readtup = readtup_index;

  state->heapRel = heapRel;
  state->indexRel = indexRel;

  state->high_mask = high_mask;
  state->low_mask = low_mask;
  state->max_buckets = max_buckets;

  MemoryContextSwitchTo(oldcontext);

  return state;
}

Tuplesortstate *
tuplesort_begin_datum(Oid datumType, Oid sortOperator, Oid sortCollation, bool nullsFirstFlag, int workMem, SortCoordinate coordinate, bool randomAccess)
{
  Tuplesortstate *state = tuplesort_begin_common(workMem, coordinate, randomAccess);
  MemoryContext oldcontext;
  int16 typlen;
  bool typbyval;

  oldcontext = MemoryContextSwitchTo(state->sortcontext);

#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG, "begin datum sort: workMem = %d, randomAccess = %c", workMem, randomAccess ? 't' : 'f');
  }
#endif

  state->nKeys = 1;                               

  TRACE_POSTGRESQL_SORT_START(DATUM_SORT, false,                      
      1, workMem, randomAccess, PARALLEL_SORT(state));

  state->comparetup = comparetup_datum;
  state->copytup = copytup_datum;
  state->writetup = writetup_datum;
  state->readtup = readtup_datum;
  state->abbrevNext = 10;

  state->datumType = datumType;

                                                     
  get_typlenbyval(datumType, &typlen, &typbyval);
  state->datumTypeLen = typlen;
  state->tuples = !typbyval;

                                
  state->sortKeys = (SortSupport)palloc0(sizeof(SortSupportData));

  state->sortKeys->ssup_cxt = CurrentMemoryContext;
  state->sortKeys->ssup_collation = sortCollation;
  state->sortKeys->ssup_nulls_first = nullsFirstFlag;

     
                                                                            
                                                                             
                                                                         
                                                                           
                                                                             
                                              
     
  state->sortKeys->abbreviate = !typbyval;

  PrepareSortSupportFromOrderingOp(sortOperator, state->sortKeys);

     
                                                                            
                                                                           
                                                                         
                                                                  
     
  if (!state->sortKeys->abbrev_converter)
  {
    state->onlyKey = state->sortKeys;
  }

  MemoryContextSwitchTo(oldcontext);

  return state;
}

   
                       
   
                                                                         
   
                                                                             
                                                                           
                                 
   
                                                                        
                                                                       
   
void
tuplesort_set_bound(Tuplesortstate *state, int64 bound)
{
                                                     
  Assert(state->status == TSS_INITIAL);
  Assert(state->memtupcount == 0);
  Assert(!state->bounded);
  Assert(!WORKER(state));

#ifdef DEBUG_BOUNDED_SORT
                                                                      
  if (!optimize_bounded_sort)
  {
    return;
  }
#endif

                                    
  if (LEADER(state))
  {
    return;
  }

                                                                     
  if (bound > (int64)(INT_MAX / 2))
  {
    return;
  }

  state->bounded = true;
  state->bound = (int)bound;

     
                                                                   
                                                                      
                           
     
  state->sortKeys->abbrev_converter = NULL;
  if (state->sortKeys->abbrev_full_comparator)
  {
    state->sortKeys->comparator = state->sortKeys->abbrev_full_comparator;
  }

                                           
  state->sortKeys->abbrev_abort = NULL;
  state->sortKeys->abbrev_full_comparator = NULL;
}

   
                 
   
                                   
   
                                                                           
                                                                       
                        
   
void
tuplesort_end(Tuplesortstate *state)
{
                                                           
  MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);

#ifdef TRACE_SORT
  long spaceUsed;

  if (state->tapeset)
  {
    spaceUsed = LogicalTapeSetBlocks(state->tapeset);
  }
  else
  {
    spaceUsed = (state->allowedMem - state->availMem + 1023) / 1024;
  }
#endif

     
                                            
     
                                                                           
                                         
     
  if (state->tapeset)
  {
    LogicalTapeSetClose(state->tapeset);
  }

#ifdef TRACE_SORT
  if (trace_sort)
  {
    if (state->tapeset)
    {
      elog(LOG, "%s of worker %d ended, %ld disk blocks used: %s", SERIAL(state) ? "external sort" : "parallel external sort", state->worker, spaceUsed, pg_rusage_show(&state->ru_start));
    }
    else
    {
      elog(LOG, "%s of worker %d ended, %ld KB used: %s", SERIAL(state) ? "internal sort" : "unperformed parallel sort", state->worker, spaceUsed, pg_rusage_show(&state->ru_start));
    }
  }

  TRACE_POSTGRESQL_SORT_DONE(state->tapeset != NULL, spaceUsed);
#else

     
                                                                         
                                     
     
  TRACE_POSTGRESQL_SORT_DONE(state->tapeset != NULL, 0L);
#endif

                                                         
  if (state->estate != NULL)
  {
    ExprContext *econtext = GetPerTupleExprContext(state->estate);

    ExecDropSingleTupleTableSlot(econtext->ecxt_scantuple);
    FreeExecutorState(state->estate);
  }

  MemoryContextSwitchTo(oldcontext);

     
                                                                             
                                                 
     
  MemoryContextDelete(state->sortcontext);
}

   
                                                                             
                                                                          
                                                                           
   
                                                                            
                                                                              
                                                                              
                                                                              
                                                                           
                                                                              
                                                                        
                                                                  
                                    
   
static bool
grow_memtuples(Tuplesortstate *state)
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

     
                                                                           
                                                                       
                                                                           
                                                                         
                                           
     
  if ((Size)newmemtupsize >= MaxAllocHugeSize / sizeof(SortTuple))
  {
    newmemtupsize = (int)(MaxAllocHugeSize / sizeof(SortTuple));
    state->growmemtuples = false;                          
  }

     
                                                                          
                                                                         
                                                                          
                                                                        
                                                                        
                                                                         
                                                                            
                                                                          
                                     
     
  if (state->availMem < (int64)((newmemtupsize - memtupsize) * sizeof(SortTuple)))
  {
    goto noalloc;
  }

                 
  FREEMEM(state, GetMemoryChunkSpace(state->memtuples));
  state->memtupsize = newmemtupsize;
  state->memtuples = (SortTuple *)repalloc_huge(state->memtuples, state->memtupsize * sizeof(SortTuple));
  USEMEM(state, GetMemoryChunkSpace(state->memtuples));
  if (LACKMEM(state))
  {
    elog(ERROR, "unexpected out-of-memory situation in tuplesort");
  }
  return true;

noalloc:
                                                                     
  state->growmemtuples = false;
  return false;
}

   
                                                          
   
                                                                           
   
void
tuplesort_puttupleslot(Tuplesortstate *state, TupleTableSlot *slot)
{
  MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
  SortTuple stup;

     
                                                                         
                                
     
  COPYTUP(state, &stup, (void *)slot);

  puttuple_common(state, &stup);

  MemoryContextSwitchTo(oldcontext);
}

   
                                                          
   
                                                                           
   
void
tuplesort_putheaptuple(Tuplesortstate *state, HeapTuple tup)
{
  MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
  SortTuple stup;

     
                                                                         
                                
     
  COPYTUP(state, &stup, (void *)tup);

  puttuple_common(state, &stup);

  MemoryContextSwitchTo(oldcontext);
}

   
                                                                          
                                   
   
void
tuplesort_putindextuplevalues(Tuplesortstate *state, Relation rel, ItemPointer self, Datum *values, bool *isnull)
{
  MemoryContext oldcontext = MemoryContextSwitchTo(state->tuplecontext);
  SortTuple stup;
  Datum original;
  IndexTuple tuple;

  stup.tuple = index_form_tuple(RelationGetDescr(rel), values, isnull);
  tuple = ((IndexTuple)stup.tuple);
  tuple->t_tid = *self;
  USEMEM(state, GetMemoryChunkSpace(stup.tuple));
                                     
  original = index_getattr(tuple, 1, RelationGetDescr(state->indexRel), &stup.isnull1);

  MemoryContextSwitchTo(state->sortcontext);

  if (!state->sortKeys || !state->sortKeys->abbrev_converter || stup.isnull1)
  {
       
                                                                          
                                                                    
                                                                      
                                                                     
                                                                  
                          
       
    stup.datum1 = original;
  }
  else if (!consider_abort_common(state))
  {
                                              
    stup.datum1 = state->sortKeys->abbrev_converter(original, state->sortKeys);
  }
  else
  {
                            
    int i;

    stup.datum1 = original;

       
                                                                  
       
                                                                      
                                                                  
                                                                       
                                                                     
                                                                         
       
    for (i = 0; i < state->memtupcount; i++)
    {
      SortTuple *mtup = &state->memtuples[i];

      tuple = mtup->tuple;
      mtup->datum1 = index_getattr(tuple, 1, RelationGetDescr(state->indexRel), &mtup->isnull1);
    }
  }

  puttuple_common(state, &stup);

  MemoryContextSwitchTo(oldcontext);
}

   
                                                          
   
                                                               
   
void
tuplesort_putdatum(Tuplesortstate *state, Datum val, bool isNull)
{
  MemoryContext oldcontext = MemoryContextSwitchTo(state->tuplecontext);
  SortTuple stup;

     
                                                                    
                                                               
     
                                                                        
                                                                          
                                                                         
                                                                       
                                                                    
                              
     

  if (isNull || !state->tuples)
  {
       
                                                                        
                                                                         
       
    stup.datum1 = !isNull ? val : (Datum)0;
    stup.isnull1 = isNull;
    stup.tuple = NULL;                          
    MemoryContextSwitchTo(state->sortcontext);
  }
  else
  {
    Datum original = datumCopy(val, false, state->datumTypeLen);

    stup.isnull1 = false;
    stup.tuple = DatumGetPointer(original);
    USEMEM(state, GetMemoryChunkSpace(stup.tuple));
    MemoryContextSwitchTo(state->sortcontext);

    if (!state->sortKeys->abbrev_converter)
    {
      stup.datum1 = original;
    }
    else if (!consider_abort_common(state))
    {
                                                
      stup.datum1 = state->sortKeys->abbrev_converter(original, state->sortKeys);
    }
    else
    {
                              
      int i;

      stup.datum1 = original;

         
                                                                    
         
                                                                        
                                                                    
                                                                         
                                                                       
                                                                    
                
         
      for (i = 0; i < state->memtupcount; i++)
      {
        SortTuple *mtup = &state->memtuples[i];

        mtup->datum1 = PointerGetDatum(mtup->tuple);
      }
    }
  }

  puttuple_common(state, &stup);

  MemoryContextSwitchTo(oldcontext);
}

   
                                          
   
static void
puttuple_common(Tuplesortstate *state, SortTuple *tuple)
{
  Assert(!LEADER(state));

  switch (state->status)
  {
  case TSS_INITIAL:

       
                                                                      
                                                                    
                                                                       
                                                                  
                             
       
    if (state->memtupcount >= state->memtupsize - 1)
    {
      (void)grow_memtuples(state);
      Assert(state->memtupcount < state->memtupsize);
    }
    state->memtuples[state->memtupcount++] = *tuple;

       
                                                                      
                                                                   
                                                                     
                                                                   
                                        
       
                                                                       
                                                                      
                                                                   
                                     
       
    if (state->bounded && (state->memtupcount > state->bound * 2 || (state->memtupcount > state->bound && LACKMEM(state))))
    {
#ifdef TRACE_SORT
      if (trace_sort)
      {
        elog(LOG, "switching to bounded heapsort at %d tuples: %s", state->memtupcount, pg_rusage_show(&state->ru_start));
      }
#endif
      make_bounded_heap(state);
      return;
    }

       
                                                                      
       
    if (state->memtupcount < state->memtupsize && !LACKMEM(state))
    {
      return;
    }

       
                                                     
       
    inittapes(state, true);

       
                        
       
    dumptuples(state, false);
    break;

  case TSS_BOUNDED:

       
                                                                      
                                                                      
                                                                    
                                                                       
                                                                      
                                                                      
       
    if (COMPARETUP(state, tuple, &state->memtuples[0]) <= 0)
    {
                                                              
      free_sort_tuple(state, tuple);
      CHECK_FOR_INTERRUPTS();
    }
    else
    {
                                                                
      free_sort_tuple(state, &state->memtuples[0]);
      tuplesort_heap_replace_top(state, tuple);
    }
    break;

  case TSS_BUILDRUNS:

       
                                                                    
       
    state->memtuples[state->memtupcount++] = *tuple;

       
                                                         
       
    dumptuples(state, false);
    break;

  default:
    elog(ERROR, "invalid tuplesort state");
    break;
  }
}

static bool
consider_abort_common(Tuplesortstate *state)
{
  Assert(state->sortKeys[0].abbrev_converter != NULL);
  Assert(state->sortKeys[0].abbrev_abort != NULL);
  Assert(state->sortKeys[0].abbrev_full_comparator != NULL);

     
                                                                          
                                     
     
  if (state->status == TSS_INITIAL && state->memtupcount >= state->abbrevNext)
  {
    state->abbrevNext *= 2;

       
                                                                           
                                             
       
    if (!state->sortKeys->abbrev_abort(state->memtupcount, state->sortKeys))
    {
      return false;
    }

       
                                                                    
                                                                       
       
    state->sortKeys[0].comparator = state->sortKeys[0].abbrev_full_comparator;
    state->sortKeys[0].abbrev_converter = NULL;
                                             
    state->sortKeys[0].abbrev_abort = NULL;
    state->sortKeys[0].abbrev_full_comparator = NULL;

                                                                
    return true;
  }

  return false;
}

   
                                                   
   
void
tuplesort_performsort(Tuplesortstate *state)
{
  MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);

#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG, "performsort of worker %d starting: %s", state->worker, pg_rusage_show(&state->ru_start));
  }
#endif

  switch (state->status)
  {
  case TSS_INITIAL:

       
                                                                    
                                                             
       
    if (SERIAL(state))
    {
                                         
      tuplesort_sort_memtuples(state);
      state->status = TSS_SORTEDINMEM;
    }
    else if (WORKER(state))
    {
         
                                                                  
                                                                 
         
      inittapes(state, false);
      dumptuples(state, true);
      worker_nomergeruns(state);
      state->status = TSS_SORTEDONTAPE;
    }
    else
    {
         
                                                                   
                                                             
         
      leader_takeover_tapes(state);
      mergeruns(state);
    }
    state->current = 0;
    state->eof_reached = false;
    state->markpos_block = 0L;
    state->markpos_offset = 0;
    state->markpos_eof = false;
    break;

  case TSS_BOUNDED:

       
                                                                     
                                                                   
                                                              
       
    sort_bounded_heap(state);
    state->current = 0;
    state->eof_reached = false;
    state->markpos_offset = 0;
    state->markpos_eof = false;
    state->status = TSS_SORTEDINMEM;
    break;

  case TSS_BUILDRUNS:

       
                                                                     
                                                                       
                                                                   
                                                           
       
    dumptuples(state, true);
    mergeruns(state);
    state->eof_reached = false;
    state->markpos_block = 0L;
    state->markpos_offset = 0;
    state->markpos_eof = false;
    break;

  default:
    elog(ERROR, "invalid tuplesort state");
    break;
  }

#ifdef TRACE_SORT
  if (trace_sort)
  {
    if (state->status == TSS_FINALMERGE)
    {
      elog(LOG, "performsort of worker %d done (except %d-way final merge): %s", state->worker, state->activeTapes, pg_rusage_show(&state->ru_start));
    }
    else
    {
      elog(LOG, "performsort of worker %d done: %s", state->worker, pg_rusage_show(&state->ru_start));
    }
  }
#endif

  MemoryContextSwitchTo(oldcontext);
}

   
                                                                      
                                                           
                                                                             
                                                                       
                                 
   
static bool
tuplesort_gettuple_common(Tuplesortstate *state, bool forward, SortTuple *stup)
{
  unsigned int tuplen;
  size_t nmoved;

  Assert(!WORKER(state));

  switch (state->status)
  {
  case TSS_SORTEDINMEM:
    Assert(forward || state->randomAccess);
    Assert(!state->slabAllocatorUsed);
    if (forward)
    {
      if (state->current < state->memtupcount)
      {
        *stup = state->memtuples[state->current++];
        return true;
      }
      state->eof_reached = true;

         
                                                               
                                                                  
                                                      
         
      if (state->bounded && state->current >= state->bound)
      {
        elog(ERROR, "retrieved too many tuples in a bounded sort");
      }

      return false;
    }
    else
    {
      if (state->current <= 0)
      {
        return false;
      }

         
                                                               
                                                   
         
      if (state->eof_reached)
      {
        state->eof_reached = false;
      }
      else
      {
        state->current--;                          
        if (state->current <= 0)
        {
          return false;
        }
      }
      *stup = state->memtuples[state->current - 1];
      return true;
    }
    break;

  case TSS_SORTEDONTAPE:
    Assert(forward || state->randomAccess);
    Assert(state->slabAllocatorUsed);

       
                                                                 
                                        
       
    if (state->lastReturnedTuple)
    {
      RELEASE_SLAB_SLOT(state, state->lastReturnedTuple);
      state->lastReturnedTuple = NULL;
    }

    if (forward)
    {
      if (state->eof_reached)
      {
        return false;
      }

      if ((tuplen = getlen(state, state->result_tape, true)) != 0)
      {
        READTUP(state, stup, state->result_tape, tuplen);

           
                                                                
                                                               
                                 
           
        state->lastReturnedTuple = stup->tuple;

        return true;
      }
      else
      {
        state->eof_reached = true;
        return false;
      }
    }

       
                 
       
                                                                    
                                          
       
    if (state->eof_reached)
    {
         
                                                                    
                                                                  
                                                                    
         
      nmoved = LogicalTapeBackspace(state->tapeset, state->result_tape, 2 * sizeof(unsigned int));
      if (nmoved == 0)
      {
        return false;
      }
      else if (nmoved != 2 * sizeof(unsigned int))
      {
        elog(ERROR, "unexpected tape position");
      }
      state->eof_reached = false;
    }
    else
    {
         
                                                                     
                                                               
         
      nmoved = LogicalTapeBackspace(state->tapeset, state->result_tape, sizeof(unsigned int));
      if (nmoved == 0)
      {
        return false;
      }
      else if (nmoved != sizeof(unsigned int))
      {
        elog(ERROR, "unexpected tape position");
      }
      tuplen = getlen(state, state->result_tape, false);

         
                                                               
         
      nmoved = LogicalTapeBackspace(state->tapeset, state->result_tape, tuplen + 2 * sizeof(unsigned int));
      if (nmoved == tuplen + sizeof(unsigned int))
      {
           
                                                                  
                                                                   
                                                                
                                                                   
                                                  
           
        return false;
      }
      else if (nmoved != tuplen + 2 * sizeof(unsigned int))
      {
        elog(ERROR, "bogus tuple length in backward scan");
      }
    }

    tuplen = getlen(state, state->result_tape, false);

       
                                                                       
                                                                 
                                                           
       
    nmoved = LogicalTapeBackspace(state->tapeset, state->result_tape, tuplen);
    if (nmoved != tuplen)
    {
      elog(ERROR, "bogus tuple length in backward scan");
    }
    READTUP(state, stup, state->result_tape, tuplen);

       
                                                                       
                                                            
       
    state->lastReturnedTuple = stup->tuple;

    return true;

  case TSS_FINALMERGE:
    Assert(forward);
                                                                    
    Assert(state->slabAllocatorUsed);

       
                                                                    
                                        
       
    if (state->lastReturnedTuple)
    {
      RELEASE_SLAB_SLOT(state, state->lastReturnedTuple);
      state->lastReturnedTuple = NULL;
    }

       
                                                               
       
    if (state->memtupcount > 0)
    {
      int srcTape = state->memtuples[0].tupindex;
      SortTuple newtup;

      *stup = state->memtuples[0];

         
                                                                  
                                                                     
         
      state->lastReturnedTuple = stup->tuple;

         
                                                                   
                                     
         
      if (!mergereadnext(state, srcTape, &newtup))
      {
           
                                                                   
                                              
           
        tuplesort_heap_delete_top(state);

           
                                                                
                                                             
                         
           
        LogicalTapeRewindForWrite(state->tapeset, srcTape);
        return true;
      }
      newtup.tupindex = srcTape;
      tuplesort_heap_replace_top(state, &newtup);
      return true;
    }
    return false;

  default:
    elog(ERROR, "invalid tuplesort state");
    return false;                          
  }
}

   
                                                             
                                                                          
                     
   
                                                                          
                                                                         
                                                                               
                                                                            
                                                                        
                                                                             
   
                                                                          
                                                                            
                                                                            
                                                                            
                                                                               
                                                                        
                                               
   
bool
tuplesort_gettupleslot(Tuplesortstate *state, bool forward, bool copy, TupleTableSlot *slot, Datum *abbrev)
{
  MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
  SortTuple stup;

  if (!tuplesort_gettuple_common(state, forward, &stup))
  {
    stup.tuple = NULL;
  }

  MemoryContextSwitchTo(oldcontext);

  if (stup.tuple)
  {
                                           
    if (state->sortKeys->abbrev_converter && abbrev)
    {
      *abbrev = stup.datum1;
    }

    if (copy)
    {
      stup.tuple = heap_copy_minimal_tuple((MinimalTuple)stup.tuple);
    }

    ExecStoreMinimalTuple((MinimalTuple)stup.tuple, slot, copy);
    return true;
  }
  else
  {
    ExecClearTuple(slot);
    return false;
  }
}

   
                                                             
                                                                               
                                                                           
                                                                
   
HeapTuple
tuplesort_getheaptuple(Tuplesortstate *state, bool forward)
{
  MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
  SortTuple stup;

  if (!tuplesort_gettuple_common(state, forward, &stup))
  {
    stup.tuple = NULL;
  }

  MemoryContextSwitchTo(oldcontext);

  return stup.tuple;
}

   
                                                                   
                                                                               
                                                                           
                                                                
   
IndexTuple
tuplesort_getindextuple(Tuplesortstate *state, bool forward)
{
  MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
  SortTuple stup;

  if (!tuplesort_gettuple_common(state, forward, &stup))
  {
    stup.tuple = NULL;
  }

  MemoryContextSwitchTo(oldcontext);

  return (IndexTuple)stup.tuple;
}

   
                                                             
                                    
   
                                                                            
                                                                          
                                                    
   
                                                                          
                                                                         
                                                                               
                                                                            
                                                                                
                                                
   
bool
tuplesort_getdatum(Tuplesortstate *state, bool forward, Datum *val, bool *isNull, Datum *abbrev)
{
  MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);
  SortTuple stup;

  if (!tuplesort_gettuple_common(state, forward, &stup))
  {
    MemoryContextSwitchTo(oldcontext);
    return false;
  }

                                                   
  MemoryContextSwitchTo(oldcontext);

                                         
  if (state->sortKeys->abbrev_converter && abbrev)
  {
    *abbrev = stup.datum1;
  }

  if (stup.isnull1 || !state->tuples)
  {
    *val = stup.datum1;
    *isNull = stup.isnull1;
  }
  else
  {
                                                                   
    *val = datumCopy(PointerGetDatum(stup.tuple), false, state->datumTypeLen);
    *isNull = false;
  }

  return true;
}

   
                                                              
                                                 
                                                           
   
bool
tuplesort_skiptuples(Tuplesortstate *state, int64 ntuples, bool forward)
{
  MemoryContext oldcontext;

     
                                                                           
                                                               
     
  Assert(forward);
  Assert(ntuples >= 0);
  Assert(!WORKER(state));

  switch (state->status)
  {
  case TSS_SORTEDINMEM:
    if (state->memtupcount - state->current >= ntuples)
    {
      state->current += ntuples;
      return true;
    }
    state->current = state->memtupcount;
    state->eof_reached = true;

       
                                                             
                                                                
                                                    
       
    if (state->bounded && state->current >= state->bound)
    {
      elog(ERROR, "retrieved too many tuples in a bounded sort");
    }

    return false;

  case TSS_SORTEDONTAPE:
  case TSS_FINALMERGE:

       
                                                                       
                              
       
    oldcontext = MemoryContextSwitchTo(state->sortcontext);
    while (ntuples-- > 0)
    {
      SortTuple stup;

      if (!tuplesort_gettuple_common(state, forward, &stup))
      {
        MemoryContextSwitchTo(oldcontext);
        return false;
      }
      CHECK_FOR_INTERRUPTS();
    }
    MemoryContextSwitchTo(oldcontext);
    return true;

  default:
    elog(ERROR, "invalid tuplesort state");
    return false;                          
  }
}

   
                                                                         
                                                                            
   
                                                                     
   
int
tuplesort_merge_order(int64 allowedMem)
{
  int mOrder;

     
                                                                             
                                                                      
                                                                             
             
     
                                                                        
                                                                             
                                  
     
  mOrder = (allowedMem - TAPE_BUFFER_OVERHEAD) / (MERGE_BUFFER_SIZE + TAPE_BUFFER_OVERHEAD);

     
                                                                          
                                                                             
                                                                          
                                                                           
                                                                          
                                                                          
                                                                          
                                                                       
                                                      
     
  mOrder = Max(mOrder, MINORDER);
  mOrder = Min(mOrder, MAXORDER);

  return mOrder;
}

   
                                            
   
                                                                 
   
static void
inittapes(Tuplesortstate *state, bool mergeruns)
{
  int maxTapes, j;

  Assert(!LEADER(state));

  if (mergeruns)
  {
                                                            
    maxTapes = tuplesort_merge_order(state->allowedMem) + 1;
  }
  else
  {
                                                                        
    Assert(WORKER(state));
    maxTapes = MINORDER + 1;
  }

#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG, "worker %d switching to external sort with %d tapes: %s", state->worker, maxTapes, pg_rusage_show(&state->ru_start));
  }
#endif

                                                                 
  inittapestate(state, maxTapes);
  state->tapeset = LogicalTapeSetCreate(maxTapes, NULL, state->shared ? &state->shared->fileset : NULL, state->worker);

  state->currentRun = 0;

     
                                                    
     
  for (j = 0; j < maxTapes; j++)
  {
    state->tp_fib[j] = 1;
    state->tp_runs[j] = 0;
    state->tp_dummy[j] = 1;
    state->tp_tapenum[j] = j;
  }
  state->tp_fib[state->tapeRange] = 0;
  state->tp_dummy[state->tapeRange] = 0;

  state->Level = 1;
  state->destTape = 0;

  state->status = TSS_BUILDRUNS;
}

   
                                                            
   
static void
inittapestate(Tuplesortstate *state, int maxTapes)
{
  int64 tapeSpace;

     
                                                                         
                                                                           
                                                                          
                                                                           
                                                                          
                                                                  
                  
     
  tapeSpace = (int64)maxTapes * TAPE_BUFFER_OVERHEAD;

  if (tapeSpace + GetMemoryChunkSpace(state->memtuples) < state->allowedMem)
  {
    USEMEM(state, tapeSpace);
  }

     
                                                                            
                                                                           
                                                                          
     
  PrepareTempTablespaces();

  state->mergeactive = (bool *)palloc0(maxTapes * sizeof(bool));
  state->tp_fib = (int *)palloc0(maxTapes * sizeof(int));
  state->tp_runs = (int *)palloc0(maxTapes * sizeof(int));
  state->tp_dummy = (int *)palloc0(maxTapes * sizeof(int));
  state->tp_tapenum = (int *)palloc0(maxTapes * sizeof(int));

                                                          
  state->maxTapes = maxTapes;
                                                               
  state->tapeRange = maxTapes - 1;
}

   
                                                         
   
                                                                 
                                                                  
   
static void
selectnewtape(Tuplesortstate *state)
{
  int j;
  int a;

                                     
  if (state->tp_dummy[state->destTape] < state->tp_dummy[state->destTape + 1])
  {
    state->destTape++;
    return;
  }
  if (state->tp_dummy[state->destTape] != 0)
  {
    state->destTape = 0;
    return;
  }

                               
  state->Level++;
  a = state->tp_fib[0];
  for (j = 0; j < state->tapeRange; j++)
  {
    state->tp_dummy[j] = a + state->tp_fib[j + 1] - state->tp_fib[j];
    state->tp_fib[j] = a + state->tp_fib[j + 1];
  }
  state->destTape = 0;
}

   
                                                                        
   
static void
init_slab_allocator(Tuplesortstate *state, int numSlots)
{
  if (numSlots > 0)
  {
    char *p;
    int i;

    state->slabMemoryBegin = palloc(numSlots * SLAB_SLOT_SIZE);
    state->slabMemoryEnd = state->slabMemoryBegin + numSlots * SLAB_SLOT_SIZE;
    state->slabFreeHead = (SlabSlot *)state->slabMemoryBegin;
    USEMEM(state, numSlots * SLAB_SLOT_SIZE);

    p = state->slabMemoryBegin;
    for (i = 0; i < numSlots - 1; i++)
    {
      ((SlabSlot *)p)->nextfree = (SlabSlot *)(p + SLAB_SLOT_SIZE);
      p += SLAB_SLOT_SIZE;
    }
    ((SlabSlot *)p)->nextfree = NULL;
  }
  else
  {
    state->slabMemoryBegin = state->slabMemoryEnd = NULL;
    state->slabFreeHead = NULL;
  }
  state->slabAllocatorUsed = true;
}

   
                                                      
   
                                                                    
                                                                  
   
static void
mergeruns(Tuplesortstate *state)
{
  int tapenum, svTape, svRuns, svDummy;
  int numTapes;
  int numInputTapes;

  Assert(state->status == TSS_BUILDRUNS);
  Assert(state->memtupcount == 0);

  if (state->sortKeys != NULL && state->sortKeys->abbrev_converter != NULL)
  {
       
                                                                        
                                                                         
                                                                         
                 
       
    state->sortKeys->abbrev_converter = NULL;
    state->sortKeys->comparator = state->sortKeys->abbrev_full_comparator;

                                             
    state->sortKeys->abbrev_abort = NULL;
    state->sortKeys->abbrev_full_comparator = NULL;
  }

     
                                                                        
                                                             
     
  MemoryContextDelete(state->tuplecontext);
  state->tuplecontext = NULL;

     
                                                                             
                              
     
  FREEMEM(state, GetMemoryChunkSpace(state->memtuples));
  pfree(state->memtuples);
  state->memtuples = NULL;

     
                                                                            
                                                          
     
                                                                           
                                                                           
                                                                             
                                                      
     
  if (state->Level == 1)
  {
    numInputTapes = state->currentRun;
    numTapes = numInputTapes + 1;
    FREEMEM(state, (state->maxTapes - numTapes) * TAPE_BUFFER_OVERHEAD);
  }
  else
  {
    numInputTapes = state->tapeRange;
    numTapes = state->maxTapes;
  }

     
                                                                           
                                                                          
                                                                     
                                                      
     
                                                                           
                                                 
     
  if (state->tuples)
  {
    init_slab_allocator(state, numInputTapes + 1);
  }
  else
  {
    init_slab_allocator(state, 0);
  }

     
                                                                             
                           
     
  state->memtupsize = numInputTapes;
  state->memtuples = (SortTuple *)palloc(numInputTapes * sizeof(SortTuple));
  USEMEM(state, GetMemoryChunkSpace(state->memtuples));

     
                                                                           
                      
     
                                                                             
                                                                          
                                                                          
                                                                        
                                                                             
                                                                            
                                                                          
                                                                      
             
     
#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG, "worker %d using " INT64_FORMAT " KB of memory for read buffers among %d input tapes", state->worker, state->availMem / 1024, numInputTapes);
  }
#endif

  state->read_buffer_size = Max(state->availMem / numInputTapes, 0);
  USEMEM(state, state->read_buffer_size * numInputTapes);

                                                                      
  for (tapenum = 0; tapenum < state->tapeRange; tapenum++)
  {
    LogicalTapeRewindForRead(state->tapeset, tapenum, state->read_buffer_size);
  }

  for (;;)
  {
       
                                                                         
                                                                        
                                                                        
                                                                          
       
    if (!state->randomAccess && !WORKER(state))
    {
      bool allOneRun = true;

      Assert(state->tp_runs[state->tapeRange] == 0);
      for (tapenum = 0; tapenum < state->tapeRange; tapenum++)
      {
        if (state->tp_runs[tapenum] + state->tp_dummy[tapenum] != 1)
        {
          allOneRun = false;
          break;
        }
      }
      if (allOneRun)
      {
                                                        
        LogicalTapeSetForgetFreeSpace(state->tapeset);
                                                 
        beginmerge(state);
        state->status = TSS_FINALMERGE;
        return;
      }
    }

                                                                 
    while (state->tp_runs[state->tapeRange - 1] || state->tp_dummy[state->tapeRange - 1])
    {
      bool allDummy = true;

      for (tapenum = 0; tapenum < state->tapeRange; tapenum++)
      {
        if (state->tp_dummy[tapenum] == 0)
        {
          allDummy = false;
          break;
        }
      }

      if (allDummy)
      {
        state->tp_dummy[state->tapeRange]++;
        for (tapenum = 0; tapenum < state->tapeRange; tapenum++)
        {
          state->tp_dummy[tapenum]--;
        }
      }
      else
      {
        mergeonerun(state);
      }
    }

                                 
    if (--state->Level == 0)
    {
      break;
    }
                                                  
    LogicalTapeRewindForRead(state->tapeset, state->tp_tapenum[state->tapeRange], state->read_buffer_size);
                                                                    
    LogicalTapeRewindForWrite(state->tapeset, state->tp_tapenum[state->tapeRange - 1]);
    state->tp_runs[state->tapeRange - 1] = 0;

       
                                                                         
       
    svTape = state->tp_tapenum[state->tapeRange];
    svDummy = state->tp_dummy[state->tapeRange];
    svRuns = state->tp_runs[state->tapeRange];
    for (tapenum = state->tapeRange; tapenum > 0; tapenum--)
    {
      state->tp_tapenum[tapenum] = state->tp_tapenum[tapenum - 1];
      state->tp_dummy[tapenum] = state->tp_dummy[tapenum - 1];
      state->tp_runs[tapenum] = state->tp_runs[tapenum - 1];
    }
    state->tp_tapenum[0] = svTape;
    state->tp_dummy[0] = svDummy;
    state->tp_runs[0] = svRuns;
  }

     
                                                                          
                                                                            
                                                                         
                                                                         
                                                                             
                                 
     
  state->result_tape = state->tp_tapenum[state->tapeRange];
  if (!WORKER(state))
  {
    LogicalTapeFreeze(state->tapeset, state->result_tape, NULL);
  }
  else
  {
    worker_freeze_result_tape(state);
  }
  state->status = TSS_SORTEDONTAPE;

                                                                           
  for (tapenum = 0; tapenum < state->maxTapes; tapenum++)
  {
    if (tapenum != state->result_tape)
    {
      LogicalTapeRewindForWrite(state->tapeset, tapenum);
    }
  }
}

   
                                                                    
   
                                                                    
                           
   
static void
mergeonerun(Tuplesortstate *state)
{
  int destTape = state->tp_tapenum[state->tapeRange];
  int srcTape;

     
                                                                            
                                                                     
     
  beginmerge(state);

     
                                                                             
                                                                       
                   
     
  while (state->memtupcount > 0)
  {
    SortTuple stup;

                                     
    srcTape = state->memtuples[0].tupindex;
    WRITETUP(state, destTape, &state->memtuples[0]);

                                                                            
    if (state->memtuples[0].tuple)
    {
      RELEASE_SLAB_SLOT(state, state->memtuples[0].tuple);
    }

       
                                                                           
                         
       
    if (mergereadnext(state, srcTape, &stup))
    {
      stup.tupindex = srcTape;
      tuplesort_heap_replace_top(state, &stup);
    }
    else
    {
      tuplesort_heap_delete_top(state);
    }
  }

     
                                                                           
                                                        
     
  markrunend(state, destTape);
  state->tp_runs[state->tapeRange]++;

#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG, "worker %d finished %d-way merge step: %s", state->worker, state->activeTapes, pg_rusage_show(&state->ru_start));
  }
#endif
}

   
                                            
   
                                                                         
                                                                           
                                                          
   
static void
beginmerge(Tuplesortstate *state)
{
  int activeTapes;
  int tapenum;
  int srcTape;

                                 
  Assert(state->memtupcount == 0);

                                                   
  memset(state->mergeactive, 0, state->maxTapes * sizeof(*state->mergeactive));
  activeTapes = 0;
  for (tapenum = 0; tapenum < state->tapeRange; tapenum++)
  {
    if (state->tp_dummy[tapenum] > 0)
    {
      state->tp_dummy[tapenum]--;
    }
    else
    {
      Assert(state->tp_runs[tapenum] > 0);
      state->tp_runs[tapenum]--;
      srcTape = state->tp_tapenum[tapenum];
      state->mergeactive[srcTape] = true;
      activeTapes++;
    }
  }
  Assert(activeTapes > 0);
  state->activeTapes = activeTapes;

                                                                     
  for (srcTape = 0; srcTape < state->maxTapes; srcTape++)
  {
    SortTuple tup;

    if (mergereadnext(state, srcTape, &tup))
    {
      tup.tupindex = srcTape;
      tuplesort_heap_insert(state, &tup);
    }
  }
}

   
                                                             
   
                         
   
static bool
mergereadnext(Tuplesortstate *state, int srcTape, SortTuple *stup)
{
  unsigned int tuplen;

  if (!state->mergeactive[srcTape])
  {
    return false;                                      
  }

                               
  if ((tuplen = getlen(state, srcTape, true)) == 0)
  {
    state->mergeactive[srcTape] = false;
    return false;
  }
  READTUP(state, stup, srcTape, tuplen);

  return true;
}

   
                                                                           
   
                                                                              
                                    
   
static void
dumptuples(Tuplesortstate *state, bool alltuples)
{
  int memtupwrite;
  int i;

     
                                                                             
                                                                  
     
  if (state->memtupcount < state->memtupsize && !LACKMEM(state) && !alltuples)
  {
    return;
  }

     
                                                                         
                                                                          
                                                                    
                
     
                                                                            
                                                                             
                                                                            
                                 
     
                                                                          
                                                                          
                                                                             
                                                                            
                                                                        
                                                                  
                                                                      
                                              
     
  Assert(state->status == TSS_BUILDRUNS);

     
                                                                          
             
     
  if (state->currentRun == INT_MAX)
  {
    ereport(ERROR, (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED), errmsg("cannot have more than %d runs for an external sort", INT_MAX)));
  }

  state->currentRun++;

#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG, "worker %d starting quicksort of run %d: %s", state->worker, state->currentRun, pg_rusage_show(&state->ru_start));
  }
#endif

     
                                                                         
                              
     
  tuplesort_sort_memtuples(state);

#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG, "worker %d finished quicksort of run %d: %s", state->worker, state->currentRun, pg_rusage_show(&state->ru_start));
  }
#endif

  memtupwrite = state->memtupcount;
  for (i = 0; i < memtupwrite; i++)
  {
    WRITETUP(state, state->tp_tapenum[state->destTape], &state->memtuples[i]);
    state->memtupcount--;
  }

     
                                                                           
                                                                             
                                                                   
                                                                         
                             
     
  MemoryContextReset(state->tuplecontext);

  markrunend(state, state->tp_tapenum[state->destTape]);
  state->tp_runs[state->destTape]++;
  state->tp_dummy[state->destTape]--;                        

#ifdef TRACE_SORT
  if (trace_sort)
  {
    elog(LOG, "worker %d finished writing run %d to tape %d: %s", state->worker, state->currentRun, state->destTape, pg_rusage_show(&state->ru_start));
  }
#endif

  if (!alltuples)
  {
    selectnewtape(state);
  }
}

   
                                                  
   
void
tuplesort_rescan(Tuplesortstate *state)
{
  MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);

  Assert(state->randomAccess);

  switch (state->status)
  {
  case TSS_SORTEDINMEM:
    state->current = 0;
    state->eof_reached = false;
    state->markpos_offset = 0;
    state->markpos_eof = false;
    break;
  case TSS_SORTEDONTAPE:
    LogicalTapeRewindForRead(state->tapeset, state->result_tape, 0);
    state->eof_reached = false;
    state->markpos_block = 0L;
    state->markpos_offset = 0;
    state->markpos_eof = false;
    break;
  default:
    elog(ERROR, "invalid tuplesort state");
    break;
  }

  MemoryContextSwitchTo(oldcontext);
}

   
                                                                      
   
void
tuplesort_markpos(Tuplesortstate *state)
{
  MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);

  Assert(state->randomAccess);

  switch (state->status)
  {
  case TSS_SORTEDINMEM:
    state->markpos_offset = state->current;
    state->markpos_eof = state->eof_reached;
    break;
  case TSS_SORTEDONTAPE:
    LogicalTapeTell(state->tapeset, state->result_tape, &state->markpos_block, &state->markpos_offset);
    state->markpos_eof = state->eof_reached;
    break;
  default:
    elog(ERROR, "invalid tuplesort state");
    break;
  }

  MemoryContextSwitchTo(oldcontext);
}

   
                                                                           
                              
   
void
tuplesort_restorepos(Tuplesortstate *state)
{
  MemoryContext oldcontext = MemoryContextSwitchTo(state->sortcontext);

  Assert(state->randomAccess);

  switch (state->status)
  {
  case TSS_SORTEDINMEM:
    state->current = state->markpos_offset;
    state->eof_reached = state->markpos_eof;
    break;
  case TSS_SORTEDONTAPE:
    LogicalTapeSeek(state->tapeset, state->result_tape, state->markpos_block, state->markpos_offset);
    state->eof_reached = state->markpos_eof;
    break;
  default:
    elog(ERROR, "invalid tuplesort state");
    break;
  }

  MemoryContextSwitchTo(oldcontext);
}

   
                                                    
   
                                                                       
                                                                   
   
void
tuplesort_get_stats(Tuplesortstate *state, TuplesortInstrumentation *stats)
{
     
                                                                            
                                                                            
                                                                            
                                                                           
                                                                             
                                                                         
                                                       
     
  if (state->tapeset)
  {
    stats->spaceType = SORT_SPACE_TYPE_DISK;
    stats->spaceUsed = LogicalTapeSetBlocks(state->tapeset) * (BLCKSZ / 1024);
  }
  else
  {
    stats->spaceType = SORT_SPACE_TYPE_MEMORY;
    stats->spaceUsed = (state->allowedMem - state->availMem + 1023) / 1024;
  }

  switch (state->status)
  {
  case TSS_SORTEDINMEM:
    if (state->boundUsed)
    {
      stats->sortMethod = SORT_TYPE_TOP_N_HEAPSORT;
    }
    else
    {
      stats->sortMethod = SORT_TYPE_QUICKSORT;
    }
    break;
  case TSS_SORTEDONTAPE:
    stats->sortMethod = SORT_TYPE_EXTERNAL_SORT;
    break;
  case TSS_FINALMERGE:
    stats->sortMethod = SORT_TYPE_EXTERNAL_MERGE;
    break;
  default:
    stats->sortMethod = SORT_TYPE_STILL_IN_PROGRESS;
    break;
  }
}

   
                                        
   
const char *
tuplesort_method_name(TuplesortMethod m)
{
  switch (m)
  {
  case SORT_TYPE_STILL_IN_PROGRESS:
    return "still in progress";
  case SORT_TYPE_TOP_N_HEAPSORT:
    return "top-N heapsort";
  case SORT_TYPE_QUICKSORT:
    return "quicksort";
  case SORT_TYPE_EXTERNAL_SORT:
    return "external sort";
  case SORT_TYPE_EXTERNAL_MERGE:
    return "external merge";
  }

  return "unknown";
}

   
                                           
   
const char *
tuplesort_space_type_name(TuplesortSpaceType t)
{
  Assert(t == SORT_SPACE_TYPE_DISK || t == SORT_SPACE_TYPE_MEMORY);
  return t == SORT_SPACE_TYPE_DISK ? "Disk" : "Memory";
}

   
                                                             
   

   
                                                                         
                                                          
   
                                                                       
                                                                            
                                                                    
                                                         
   
static void
make_bounded_heap(Tuplesortstate *state)
{
  int tupcount = state->memtupcount;
  int i;

  Assert(state->status == TSS_INITIAL);
  Assert(state->bounded);
  Assert(tupcount >= state->bound);
  Assert(SERIAL(state));

                                                               
  reversedirection(state);

  state->memtupcount = 0;                          
  for (i = 0; i < tupcount; i++)
  {
    if (state->memtupcount < state->bound)
    {
                                       
                                                              
      SortTuple stup = state->memtuples[i];

      tuplesort_heap_insert(state, &stup);
    }
    else
    {
         
                                                                   
                                                                         
                      
         
      if (COMPARETUP(state, &state->memtuples[i], &state->memtuples[0]) <= 0)
      {
        free_sort_tuple(state, &state->memtuples[i]);
        CHECK_FOR_INTERRUPTS();
      }
      else
      {
        tuplesort_heap_replace_top(state, &state->memtuples[i]);
      }
    }
  }

  Assert(state->memtupcount == state->bound);
  state->status = TSS_BOUNDED;
}

   
                                                       
   
static void
sort_bounded_heap(Tuplesortstate *state)
{
  int tupcount = state->memtupcount;

  Assert(state->status == TSS_BOUNDED);
  Assert(state->bounded);
  Assert(tupcount == state->bound);
  Assert(SERIAL(state));

     
                                                                            
                                                                           
                                                                   
     
  while (state->memtupcount > 1)
  {
    SortTuple stup = state->memtuples[0];

                                                                        
    tuplesort_heap_delete_top(state);
    state->memtuples[state->memtupcount] = stup;
  }
  state->memtupcount = tupcount;

     
                                                                     
                                                                 
     
  reversedirection(state);

  state->status = TSS_SORTEDINMEM;
  state->boundUsed = true;
}

   
                                                          
   
                                                                        
   
static void
tuplesort_sort_memtuples(Tuplesortstate *state)
{
  Assert(!LEADER(state));

  if (state->memtupcount > 1)
  {
                                                  
    if (state->onlyKey != NULL)
    {
      qsort_ssup(state->memtuples, state->memtupcount, state->onlyKey);
    }
    else
    {
      qsort_tuple(state->memtuples, state->memtupcount, state->comparetup, state);
    }
  }
}

   
                                                                      
                                                                     
   
                                                                         
                                                                           
                                                                           
                                                                  
   
static void
tuplesort_heap_insert(Tuplesortstate *state, SortTuple *tuple)
{
  SortTuple *memtuples;
  int j;

  memtuples = state->memtuples;
  Assert(state->memtupcount < state->memtupsize);

  CHECK_FOR_INTERRUPTS();

     
                                                                            
                                               
     
  j = state->memtupcount++;
  while (j > 0)
  {
    int i = (j - 1) >> 1;

    if (COMPARETUP(state, tuple, &memtuples[i]) >= 0)
    {
      break;
    }
    memtuples[j] = memtuples[i];
    j = i;
  }
  memtuples[j] = *tuple;
}

   
                                                                     
                                                            
   
                                                                   
                 
   
static void
tuplesort_heap_delete_top(Tuplesortstate *state)
{
  SortTuple *memtuples = state->memtuples;
  SortTuple *tuple;

  if (--state->memtupcount <= 0)
  {
    return;
  }

     
                                                                           
                               
     
  tuple = &memtuples[state->memtupcount];
  tuplesort_heap_replace_top(state, tuple);
}

   
                                                                          
                                
   
                                                                      
                           
   
static void
tuplesort_heap_replace_top(Tuplesortstate *state, SortTuple *tuple)
{
  SortTuple *memtuples = state->memtuples;
  unsigned int i, n;

  Assert(state->memtupcount >= 1);

  CHECK_FOR_INTERRUPTS();

     
                                                                         
                                                                             
                                                              
     
  n = state->memtupcount;
  i = 0;                               
  for (;;)
  {
    unsigned int j = 2 * i + 1;

    if (j >= n)
    {
      break;
    }
    if (j + 1 < n && COMPARETUP(state, &memtuples[j], &memtuples[j + 1]) > 0)
    {
      j++;
    }
    if (COMPARETUP(state, tuple, &memtuples[j]) <= 0)
    {
      break;
    }
    memtuples[i] = memtuples[j];
    i = j;
  }
  memtuples[i] = *tuple;
}

   
                                                                 
   
                                                               
   
static void
reversedirection(Tuplesortstate *state)
{
  SortSupport sortKey = state->sortKeys;
  int nkey;

  for (nkey = 0; nkey < state->nKeys; nkey++, sortKey++)
  {
    sortKey->ssup_reverse = !sortKey->ssup_reverse;
    sortKey->ssup_nulls_first = !sortKey->ssup_nulls_first;
  }
}

   
                           
   

static unsigned int
getlen(Tuplesortstate *state, int tapenum, bool eofOK)
{
  unsigned int len;

  if (LogicalTapeRead(state->tapeset, tapenum, &len, sizeof(len)) != sizeof(len))
  {
    elog(ERROR, "unexpected end of tape");
  }
  if (len == 0 && !eofOK)
  {
    elog(ERROR, "unexpected end of data");
  }
  return len;
}

static void
markrunend(Tuplesortstate *state, int tapenum)
{
  unsigned int len = 0;

  LogicalTapeWrite(state->tapeset, tapenum, (void *)&len, sizeof(len));
}

   
                                                       
   
                                                                           
                          
   
static void *
readtup_alloc(Tuplesortstate *state, Size tuplen)
{
  SlabSlot *buf;

     
                                                                             
          
     
  Assert(state->slabFreeHead);

  if (tuplen > SLAB_SLOT_SIZE || !state->slabFreeHead)
  {
    return MemoryContextAlloc(state->sortcontext, tuplen);
  }
  else
  {
    buf = state->slabFreeHead;
                         
    state->slabFreeHead = buf->nextfree;

    return buf;
  }
}

   
                                                                   
   

static int
comparetup_heap(const SortTuple *a, const SortTuple *b, Tuplesortstate *state)
{
  SortSupport sortKey = state->sortKeys;
  HeapTupleData ltup;
  HeapTupleData rtup;
  TupleDesc tupDesc;
  int nkey;
  int32 compare;
  AttrNumber attno;
  Datum datum1, datum2;
  bool isnull1, isnull2;

                                    
  compare = ApplySortComparator(a->datum1, a->isnull1, b->datum1, b->isnull1, sortKey);
  if (compare != 0)
  {
    return compare;
  }

                                    
  ltup.t_len = ((MinimalTuple)a->tuple)->t_len + MINIMAL_TUPLE_OFFSET;
  ltup.t_data = (HeapTupleHeader)((char *)a->tuple - MINIMAL_TUPLE_OFFSET);
  rtup.t_len = ((MinimalTuple)b->tuple)->t_len + MINIMAL_TUPLE_OFFSET;
  rtup.t_data = (HeapTupleHeader)((char *)b->tuple - MINIMAL_TUPLE_OFFSET);
  tupDesc = state->tupDesc;

  if (sortKey->abbrev_converter)
  {
    attno = sortKey->ssup_attno;

    datum1 = heap_getattr(&ltup, attno, tupDesc, &isnull1);
    datum2 = heap_getattr(&rtup, attno, tupDesc, &isnull2);

    compare = ApplySortAbbrevFullComparator(datum1, isnull1, datum2, isnull2, sortKey);
    if (compare != 0)
    {
      return compare;
    }
  }

  sortKey++;
  for (nkey = 1; nkey < state->nKeys; nkey++, sortKey++)
  {
    attno = sortKey->ssup_attno;

    datum1 = heap_getattr(&ltup, attno, tupDesc, &isnull1);
    datum2 = heap_getattr(&rtup, attno, tupDesc, &isnull2);

    compare = ApplySortComparator(datum1, isnull1, datum2, isnull2, sortKey);
    if (compare != 0)
    {
      return compare;
    }
  }

  return 0;
}

static void
copytup_heap(Tuplesortstate *state, SortTuple *stup, void *tup)
{
     
                                                                   
                                                         
     
  TupleTableSlot *slot = (TupleTableSlot *)tup;
  Datum original;
  MinimalTuple tuple;
  HeapTupleData htup;
  MemoryContext oldcontext = MemoryContextSwitchTo(state->tuplecontext);

                                        
  tuple = ExecCopySlotMinimalTuple(slot);
  stup->tuple = (void *)tuple;
  USEMEM(state, GetMemoryChunkSpace(tuple));
                                     
  htup.t_len = tuple->t_len + MINIMAL_TUPLE_OFFSET;
  htup.t_data = (HeapTupleHeader)((char *)tuple - MINIMAL_TUPLE_OFFSET);
  original = heap_getattr(&htup, state->sortKeys[0].ssup_attno, state->tupDesc, &stup->isnull1);

  MemoryContextSwitchTo(oldcontext);

  if (!state->sortKeys->abbrev_converter || stup->isnull1)
  {
       
                                                                          
                                                                    
                                                                      
                                                                     
                                                                  
                          
       
    stup->datum1 = original;
  }
  else if (!consider_abort_common(state))
  {
                                              
    stup->datum1 = state->sortKeys->abbrev_converter(original, state->sortKeys);
  }
  else
  {
                            
    int i;

    stup->datum1 = original;

       
                                                                  
       
                                                                      
                                                                  
                                                                       
                                                                     
                                                                         
       
    for (i = 0; i < state->memtupcount; i++)
    {
      SortTuple *mtup = &state->memtuples[i];

      htup.t_len = ((MinimalTuple)mtup->tuple)->t_len + MINIMAL_TUPLE_OFFSET;
      htup.t_data = (HeapTupleHeader)((char *)mtup->tuple - MINIMAL_TUPLE_OFFSET);

      mtup->datum1 = heap_getattr(&htup, state->sortKeys[0].ssup_attno, state->tupDesc, &mtup->isnull1);
    }
  }
}

static void
writetup_heap(Tuplesortstate *state, int tapenum, SortTuple *stup)
{
  MinimalTuple tuple = (MinimalTuple)stup->tuple;

                                                 
  char *tupbody = (char *)tuple + MINIMAL_TUPLE_DATA_OFFSET;
  unsigned int tupbodylen = tuple->t_len - MINIMAL_TUPLE_DATA_OFFSET;

                                
  unsigned int tuplen = tupbodylen + sizeof(int);

  LogicalTapeWrite(state->tapeset, tapenum, (void *)&tuplen, sizeof(tuplen));
  LogicalTapeWrite(state->tapeset, tapenum, (void *)tupbody, tupbodylen);
  if (state->randomAccess)                                 
  {
    LogicalTapeWrite(state->tapeset, tapenum, (void *)&tuplen, sizeof(tuplen));
  }

  if (!state->slabAllocatorUsed)
  {
    FREEMEM(state, GetMemoryChunkSpace(tuple));
    heap_free_minimal_tuple(tuple);
  }
}

static void
readtup_heap(Tuplesortstate *state, SortTuple *stup, int tapenum, unsigned int len)
{
  unsigned int tupbodylen = len - sizeof(int);
  unsigned int tuplen = tupbodylen + MINIMAL_TUPLE_DATA_OFFSET;
  MinimalTuple tuple = (MinimalTuple)readtup_alloc(state, tuplen);
  char *tupbody = (char *)tuple + MINIMAL_TUPLE_DATA_OFFSET;
  HeapTupleData htup;

                                
  tuple->t_len = tuplen;
  LogicalTapeReadExact(state->tapeset, tapenum, tupbody, tupbodylen);
  if (state->randomAccess)                                 
  {
    LogicalTapeReadExact(state->tapeset, tapenum, &tuplen, sizeof(tuplen));
  }
  stup->tuple = (void *)tuple;
                                     
  htup.t_len = tuple->t_len + MINIMAL_TUPLE_OFFSET;
  htup.t_data = (HeapTupleHeader)((char *)tuple - MINIMAL_TUPLE_OFFSET);
  stup->datum1 = heap_getattr(&htup, state->sortKeys[0].ssup_attno, state->tupDesc, &stup->isnull1);
}

   
                                                                   
                                             
   

static int
comparetup_cluster(const SortTuple *a, const SortTuple *b, Tuplesortstate *state)
{
  SortSupport sortKey = state->sortKeys;
  HeapTuple ltup;
  HeapTuple rtup;
  TupleDesc tupDesc;
  int nkey;
  int32 compare;
  Datum datum1, datum2;
  bool isnull1, isnull2;
  AttrNumber leading = state->indexInfo->ii_IndexAttrNumbers[0];

                                                   
  ltup = (HeapTuple)a->tuple;
  rtup = (HeapTuple)b->tuple;
  tupDesc = state->tupDesc;

                                                    
  if (leading != 0)
  {
    compare = ApplySortComparator(a->datum1, a->isnull1, b->datum1, b->isnull1, sortKey);
    if (compare != 0)
    {
      return compare;
    }

    if (sortKey->abbrev_converter)
    {
      datum1 = heap_getattr(ltup, leading, tupDesc, &isnull1);
      datum2 = heap_getattr(rtup, leading, tupDesc, &isnull2);

      compare = ApplySortAbbrevFullComparator(datum1, isnull1, datum2, isnull2, sortKey);
    }
    if (compare != 0 || state->nKeys == 1)
    {
      return compare;
    }
                                                 
    sortKey++;
    nkey = 1;
  }
  else
  {
                                            
    nkey = 0;
  }

  if (state->indexInfo->ii_Expressions == NULL)
  {
                                                                     

    for (; nkey < state->nKeys; nkey++, sortKey++)
    {
      AttrNumber attno = state->indexInfo->ii_IndexAttrNumbers[nkey];

      datum1 = heap_getattr(ltup, attno, tupDesc, &isnull1);
      datum2 = heap_getattr(rtup, attno, tupDesc, &isnull2);

      compare = ApplySortComparator(datum1, isnull1, datum2, isnull2, sortKey);
      if (compare != 0)
      {
        return compare;
      }
    }
  }
  else
  {
       
                                                                       
                                                                           
                                                                  
                                                    
       
    Datum l_index_values[INDEX_MAX_KEYS];
    bool l_index_isnull[INDEX_MAX_KEYS];
    Datum r_index_values[INDEX_MAX_KEYS];
    bool r_index_isnull[INDEX_MAX_KEYS];
    TupleTableSlot *ecxt_scantuple;

                                                           
    ResetPerTupleExprContext(state->estate);

    ecxt_scantuple = GetPerTupleExprContext(state->estate)->ecxt_scantuple;

    ExecStoreHeapTuple(ltup, ecxt_scantuple, false);
    FormIndexDatum(state->indexInfo, ecxt_scantuple, state->estate, l_index_values, l_index_isnull);

    ExecStoreHeapTuple(rtup, ecxt_scantuple, false);
    FormIndexDatum(state->indexInfo, ecxt_scantuple, state->estate, r_index_values, r_index_isnull);

    for (; nkey < state->nKeys; nkey++, sortKey++)
    {
      compare = ApplySortComparator(l_index_values[nkey], l_index_isnull[nkey], r_index_values[nkey], r_index_isnull[nkey], sortKey);
      if (compare != 0)
      {
        return compare;
      }
    }
  }

  return 0;
}

static void
copytup_cluster(Tuplesortstate *state, SortTuple *stup, void *tup)
{
  HeapTuple tuple = (HeapTuple)tup;
  Datum original;
  MemoryContext oldcontext = MemoryContextSwitchTo(state->tuplecontext);

                                        
  tuple = heap_copytuple(tuple);
  stup->tuple = (void *)tuple;
  USEMEM(state, GetMemoryChunkSpace(tuple));

  MemoryContextSwitchTo(oldcontext);

     
                                                                          
                   
     
  if (state->indexInfo->ii_IndexAttrNumbers[0] == 0)
  {
    return;
  }

  original = heap_getattr(tuple, state->indexInfo->ii_IndexAttrNumbers[0], state->tupDesc, &stup->isnull1);

  if (!state->sortKeys->abbrev_converter || stup->isnull1)
  {
       
                                                                          
                                                                    
                                                                      
                                                                     
                                                                  
                          
       
    stup->datum1 = original;
  }
  else if (!consider_abort_common(state))
  {
                                              
    stup->datum1 = state->sortKeys->abbrev_converter(original, state->sortKeys);
  }
  else
  {
                            
    int i;

    stup->datum1 = original;

       
                                                                  
       
                                                                      
                                                                  
                                                                       
                                                                     
                                                                         
       
    for (i = 0; i < state->memtupcount; i++)
    {
      SortTuple *mtup = &state->memtuples[i];

      tuple = (HeapTuple)mtup->tuple;
      mtup->datum1 = heap_getattr(tuple, state->indexInfo->ii_IndexAttrNumbers[0], state->tupDesc, &mtup->isnull1);
    }
  }
}

static void
writetup_cluster(Tuplesortstate *state, int tapenum, SortTuple *stup)
{
  HeapTuple tuple = (HeapTuple)stup->tuple;
  unsigned int tuplen = tuple->t_len + sizeof(ItemPointerData) + sizeof(int);

                                                                      
  LogicalTapeWrite(state->tapeset, tapenum, &tuplen, sizeof(tuplen));
  LogicalTapeWrite(state->tapeset, tapenum, &tuple->t_self, sizeof(ItemPointerData));
  LogicalTapeWrite(state->tapeset, tapenum, tuple->t_data, tuple->t_len);
  if (state->randomAccess)                                 
  {
    LogicalTapeWrite(state->tapeset, tapenum, &tuplen, sizeof(tuplen));
  }

  if (!state->slabAllocatorUsed)
  {
    FREEMEM(state, GetMemoryChunkSpace(tuple));
    heap_freetuple(tuple);
  }
}

static void
readtup_cluster(Tuplesortstate *state, SortTuple *stup, int tapenum, unsigned int tuplen)
{
  unsigned int t_len = tuplen - sizeof(ItemPointerData) - sizeof(int);
  HeapTuple tuple = (HeapTuple)readtup_alloc(state, t_len + HEAPTUPLESIZE);

                                            
  tuple->t_data = (HeapTupleHeader)((char *)tuple + HEAPTUPLESIZE);
  tuple->t_len = t_len;
  LogicalTapeReadExact(state->tapeset, tapenum, &tuple->t_self, sizeof(ItemPointerData));
                                                           
  tuple->t_tableOid = InvalidOid;
                              
  LogicalTapeReadExact(state->tapeset, tapenum, tuple->t_data, tuple->t_len);
  if (state->randomAccess)                                 
  {
    LogicalTapeReadExact(state->tapeset, tapenum, &tuplen, sizeof(tuplen));
  }
  stup->tuple = (void *)tuple;
                                                              
  if (state->indexInfo->ii_IndexAttrNumbers[0] != 0)
  {
    stup->datum1 = heap_getattr(tuple, state->indexInfo->ii_IndexAttrNumbers[0], state->tupDesc, &stup->isnull1);
  }
}

   
                                            
   
                                                                           
                                                                        
                            
   

static int
comparetup_index_btree(const SortTuple *a, const SortTuple *b, Tuplesortstate *state)
{
     
                                                                            
                                                                    
                                          
     
  SortSupport sortKey = state->sortKeys;
  IndexTuple tuple1;
  IndexTuple tuple2;
  int keysz;
  TupleDesc tupDes;
  bool equal_hasnull = false;
  int nkey;
  int32 compare;
  Datum datum1, datum2;
  bool isnull1, isnull2;

                                    
  compare = ApplySortComparator(a->datum1, a->isnull1, b->datum1, b->isnull1, sortKey);
  if (compare != 0)
  {
    return compare;
  }

                                    
  tuple1 = (IndexTuple)a->tuple;
  tuple2 = (IndexTuple)b->tuple;
  keysz = state->nKeys;
  tupDes = RelationGetDescr(state->indexRel);

  if (sortKey->abbrev_converter)
  {
    datum1 = index_getattr(tuple1, 1, tupDes, &isnull1);
    datum2 = index_getattr(tuple2, 1, tupDes, &isnull2);

    compare = ApplySortAbbrevFullComparator(datum1, isnull1, datum2, isnull2, sortKey);
    if (compare != 0)
    {
      return compare;
    }
  }

                                                                
  if (a->isnull1)
  {
    equal_hasnull = true;
  }

  sortKey++;
  for (nkey = 2; nkey <= keysz; nkey++, sortKey++)
  {
    datum1 = index_getattr(tuple1, nkey, tupDes, &isnull1);
    datum2 = index_getattr(tuple2, nkey, tupDes, &isnull2);

    compare = ApplySortComparator(datum1, isnull1, datum2, isnull2, sortKey);
    if (compare != 0)
    {
      return compare;                                           
    }

                                                                  
    if (isnull1)
    {
      equal_hasnull = true;
    }
  }

     
                                                                        
                                                                     
     
                                                                             
                                                                          
                                                                             
            
     
  if (state->enforceUnique && !equal_hasnull)
  {
    Datum values[INDEX_MAX_KEYS];
    bool isnull[INDEX_MAX_KEYS];
    char *key_desc;

       
                                                                           
                                                                      
                                                                        
                 
       
    Assert(tuple1 != tuple2);

    index_deform_tuple(tuple1, tupDes, values, isnull);

    key_desc = BuildIndexValueDescription(state->indexRel, values, isnull);

    ereport(ERROR, (errcode(ERRCODE_UNIQUE_VIOLATION), errmsg("could not create unique index \"%s\"", RelationGetRelationName(state->indexRel)), key_desc ? errdetail("Key %s is duplicated.", key_desc) : errdetail("Duplicate keys exist."), errtableconstraint(state->heapRel, RelationGetRelationName(state->indexRel))));
  }

     
                                                                            
                                                                      
                                                                            
             
     
  {
    BlockNumber blk1 = ItemPointerGetBlockNumber(&tuple1->t_tid);
    BlockNumber blk2 = ItemPointerGetBlockNumber(&tuple2->t_tid);

    if (blk1 != blk2)
    {
      return (blk1 < blk2) ? -1 : 1;
    }
  }
  {
    OffsetNumber pos1 = ItemPointerGetOffsetNumber(&tuple1->t_tid);
    OffsetNumber pos2 = ItemPointerGetOffsetNumber(&tuple2->t_tid);

    if (pos1 != pos2)
    {
      return (pos1 < pos2) ? -1 : 1;
    }
  }

                                                
  Assert(false);

  return 0;
}

static int
comparetup_index_hash(const SortTuple *a, const SortTuple *b, Tuplesortstate *state)
{
  Bucket bucket1;
  Bucket bucket2;
  IndexTuple tuple1;
  IndexTuple tuple2;

     
                                                                         
                                                               
     
  Assert(!a->isnull1);
  bucket1 = _hash_hashkey2bucket(DatumGetUInt32(a->datum1), state->max_buckets, state->high_mask, state->low_mask);
  Assert(!b->isnull1);
  bucket2 = _hash_hashkey2bucket(DatumGetUInt32(b->datum1), state->max_buckets, state->high_mask, state->low_mask);
  if (bucket1 > bucket2)
  {
    return 1;
  }
  else if (bucket1 < bucket2)
  {
    return -1;
  }

     
                                                                             
                                                                        
                              
     
  tuple1 = (IndexTuple)a->tuple;
  tuple2 = (IndexTuple)b->tuple;

  {
    BlockNumber blk1 = ItemPointerGetBlockNumber(&tuple1->t_tid);
    BlockNumber blk2 = ItemPointerGetBlockNumber(&tuple2->t_tid);

    if (blk1 != blk2)
    {
      return (blk1 < blk2) ? -1 : 1;
    }
  }
  {
    OffsetNumber pos1 = ItemPointerGetOffsetNumber(&tuple1->t_tid);
    OffsetNumber pos2 = ItemPointerGetOffsetNumber(&tuple2->t_tid);

    if (pos1 != pos2)
    {
      return (pos1 < pos2) ? -1 : 1;
    }
  }

                                                
  Assert(false);

  return 0;
}

static void
copytup_index(Tuplesortstate *state, SortTuple *stup, void *tup)
{
  IndexTuple tuple = (IndexTuple)tup;
  unsigned int tuplen = IndexTupleSize(tuple);
  IndexTuple newtuple;
  Datum original;

                                        
  newtuple = (IndexTuple)MemoryContextAlloc(state->tuplecontext, tuplen);
  memcpy(newtuple, tuple, tuplen);
  USEMEM(state, GetMemoryChunkSpace(newtuple));
  stup->tuple = (void *)newtuple;
                                     
  original = index_getattr(newtuple, 1, RelationGetDescr(state->indexRel), &stup->isnull1);

  if (!state->sortKeys->abbrev_converter || stup->isnull1)
  {
       
                                                                          
                                                                    
                                                                      
                                                                     
                                                                  
                          
       
    stup->datum1 = original;
  }
  else if (!consider_abort_common(state))
  {
                                              
    stup->datum1 = state->sortKeys->abbrev_converter(original, state->sortKeys);
  }
  else
  {
                            
    int i;

    stup->datum1 = original;

       
                                                                  
       
                                                                      
                                                                  
                                                                       
                                                                     
                                                                         
       
    for (i = 0; i < state->memtupcount; i++)
    {
      SortTuple *mtup = &state->memtuples[i];

      tuple = (IndexTuple)mtup->tuple;
      mtup->datum1 = index_getattr(tuple, 1, RelationGetDescr(state->indexRel), &mtup->isnull1);
    }
  }
}

static void
writetup_index(Tuplesortstate *state, int tapenum, SortTuple *stup)
{
  IndexTuple tuple = (IndexTuple)stup->tuple;
  unsigned int tuplen;

  tuplen = IndexTupleSize(tuple) + sizeof(tuplen);
  LogicalTapeWrite(state->tapeset, tapenum, (void *)&tuplen, sizeof(tuplen));
  LogicalTapeWrite(state->tapeset, tapenum, (void *)tuple, IndexTupleSize(tuple));
  if (state->randomAccess)                                 
  {
    LogicalTapeWrite(state->tapeset, tapenum, (void *)&tuplen, sizeof(tuplen));
  }

  if (!state->slabAllocatorUsed)
  {
    FREEMEM(state, GetMemoryChunkSpace(tuple));
    pfree(tuple);
  }
}

static void
readtup_index(Tuplesortstate *state, SortTuple *stup, int tapenum, unsigned int len)
{
  unsigned int tuplen = len - sizeof(unsigned int);
  IndexTuple tuple = (IndexTuple)readtup_alloc(state, tuplen);

  LogicalTapeReadExact(state->tapeset, tapenum, tuple, tuplen);
  if (state->randomAccess)                                 
  {
    LogicalTapeReadExact(state->tapeset, tapenum, &tuplen, sizeof(tuplen));
  }
  stup->tuple = (void *)tuple;
                                     
  stup->datum1 = index_getattr(tuple, 1, RelationGetDescr(state->indexRel), &stup->isnull1);
}

   
                                            
   

static int
comparetup_datum(const SortTuple *a, const SortTuple *b, Tuplesortstate *state)
{
  int compare;

  compare = ApplySortComparator(a->datum1, a->isnull1, b->datum1, b->isnull1, state->sortKeys);
  if (compare != 0)
  {
    return compare;
  }

                                                                     

  if (state->sortKeys->abbrev_converter)
  {
    compare = ApplySortAbbrevFullComparator(PointerGetDatum(a->tuple), a->isnull1, PointerGetDatum(b->tuple), b->isnull1, state->sortKeys);
  }

  return compare;
}

static void
copytup_datum(Tuplesortstate *state, SortTuple *stup, void *tup)
{
                            
  elog(ERROR, "copytup_datum() should not be called");
}

static void
writetup_datum(Tuplesortstate *state, int tapenum, SortTuple *stup)
{
  void *waddr;
  unsigned int tuplen;
  unsigned int writtenlen;

  if (stup->isnull1)
  {
    waddr = NULL;
    tuplen = 0;
  }
  else if (!state->tuples)
  {
    waddr = &stup->datum1;
    tuplen = sizeof(Datum);
  }
  else
  {
    waddr = stup->tuple;
    tuplen = datumGetSize(PointerGetDatum(stup->tuple), false, state->datumTypeLen);
    Assert(tuplen != 0);
  }

  writtenlen = tuplen + sizeof(unsigned int);

  LogicalTapeWrite(state->tapeset, tapenum, (void *)&writtenlen, sizeof(writtenlen));
  LogicalTapeWrite(state->tapeset, tapenum, waddr, tuplen);
  if (state->randomAccess)                                 
  {
    LogicalTapeWrite(state->tapeset, tapenum, (void *)&writtenlen, sizeof(writtenlen));
  }

  if (!state->slabAllocatorUsed && stup->tuple)
  {
    FREEMEM(state, GetMemoryChunkSpace(stup->tuple));
    pfree(stup->tuple);
  }
}

static void
readtup_datum(Tuplesortstate *state, SortTuple *stup, int tapenum, unsigned int len)
{
  unsigned int tuplen = len - sizeof(unsigned int);

  if (tuplen == 0)
  {
                   
    stup->datum1 = (Datum)0;
    stup->isnull1 = true;
    stup->tuple = NULL;
  }
  else if (!state->tuples)
  {
    Assert(tuplen == sizeof(Datum));
    LogicalTapeReadExact(state->tapeset, tapenum, &stup->datum1, tuplen);
    stup->isnull1 = false;
    stup->tuple = NULL;
  }
  else
  {
    void *raddr = readtup_alloc(state, tuplen);

    LogicalTapeReadExact(state->tapeset, tapenum, raddr, tuplen);
    stup->datum1 = PointerGetDatum(raddr);
    stup->isnull1 = false;
    stup->tuple = raddr;
  }

  if (state->randomAccess)                                 
  {
    LogicalTapeReadExact(state->tapeset, tapenum, &tuplen, sizeof(tuplen));
  }
}

   
                          
   

   
                                                                          
   
                                                                          
                       
   
Size
tuplesort_estimate_shared(int nWorkers)
{
  Size tapesSize;

  Assert(nWorkers > 0);

                                                         
  tapesSize = mul_size(sizeof(TapeShare), nWorkers);
  tapesSize = MAXALIGN(add_size(tapesSize, offsetof(Sharedsort, tapes)));

  return tapesSize;
}

   
                                                                   
   
                                                                      
                                                                         
                                                                    
   
void
tuplesort_initialize_shared(Sharedsort *shared, int nWorkers, dsm_segment *seg)
{
  int i;

  Assert(nWorkers > 0);

  SpinLockInit(&shared->mutex);
  shared->currentWorker = 0;
  shared->workersFinished = 0;
  SharedFileSetInit(&shared->fileset, seg);
  shared->nTapes = nWorkers;
  for (i = 0; i < nWorkers; i++)
  {
    shared->tapes[i].firstblocknumber = 0L;
  }
}

   
                                                              
   
                                           
   
void
tuplesort_attach_shared(Sharedsort *shared, dsm_segment *seg)
{
                               
  SharedFileSetAttach(&shared->fileset, seg);
}

   
                                                                           
   
                                                                             
                                                                         
                                                   
   
                                                                    
                                                                     
                                                                          
                                                                        
                                                               
   
static int
worker_get_identifier(Tuplesortstate *state)
{
  Sharedsort *shared = state->shared;
  int worker;

  Assert(WORKER(state));

  SpinLockAcquire(&shared->mutex);
  worker = shared->currentWorker++;
  SpinLockRelease(&shared->mutex);

  return worker;
}

   
                                                                      
   
                                                                             
                                                                        
                                                              
                                                                            
                                                                        
                                                              
   
                                                                             
                                                         
   
static void
worker_freeze_result_tape(Tuplesortstate *state)
{
  Sharedsort *shared = state->shared;
  TapeShare output;

  Assert(WORKER(state));
  Assert(state->result_tape != -1);
  Assert(state->memtupcount == 0);

     
                                                                            
                                                                      
     
  pfree(state->memtuples);
               
  state->memtuples = NULL;
  state->memtupsize = 0;

     
                                                                             
                              
     
  LogicalTapeFreeze(state->tapeset, state->result_tape, &output);

                                                                         
  SpinLockAcquire(&shared->mutex);
  shared->tapes[state->worker] = output;
  shared->workersFinished++;
  SpinLockRelease(&shared->mutex);
}

   
                                                                  
   
                                                                      
                        
   
static void
worker_nomergeruns(Tuplesortstate *state)
{
  Assert(WORKER(state));
  Assert(state->result_tape == -1);

  state->result_tape = state->tp_tapenum[state->destTape];
  worker_freeze_result_tape(state);
}

   
                                                                       
   
                                                                               
                                                                            
                                 
   
                                                                          
                                                                             
               
   
static void
leader_takeover_tapes(Tuplesortstate *state)
{
  Sharedsort *shared = state->shared;
  int nParticipants = state->nParticipants;
  int workersFinished;
  int j;

  Assert(LEADER(state));
  Assert(nParticipants >= 1);

  SpinLockAcquire(&shared->mutex);
  workersFinished = shared->workersFinished;
  SpinLockRelease(&shared->mutex);

  if (nParticipants != workersFinished)
  {
    elog(ERROR, "cannot take over tapes before all workers finish");
  }

     
                                                                            
                                                                           
                                                                      
     
                                                                          
                                                                      
                                                                 
                                                    
     
  inittapestate(state, nParticipants + 1);
  state->tapeset = LogicalTapeSetCreate(nParticipants + 1, shared->tapes, &shared->fileset, state->worker);

                                                                          
  state->currentRun = nParticipants;

     
                                                                         
                                                  
     
                                                                          
                                                                          
                                                     
     
  for (j = 0; j < state->maxTapes; j++)
  {
                                                      
    state->tp_fib[j] = 1;
    state->tp_runs[j] = 1;
    state->tp_dummy[j] = 0;
    state->tp_tapenum[j] = j;
  }
                                                        
  state->tp_fib[state->tapeRange] = 0;
  state->tp_runs[state->tapeRange] = 0;
  state->tp_dummy[state->tapeRange] = 1;

  state->Level = 1;
  state->destTape = 0;

  state->status = TSS_BUILDRUNS;
}

   
                                                                          
   
static void
free_sort_tuple(Tuplesortstate *state, SortTuple *stup)
{
  if (stup->tuple)
  {
    FREEMEM(state, GetMemoryChunkSpace(stup->tuple));
    pfree(stup->tuple);
    stup->tuple = NULL;
  }
}
