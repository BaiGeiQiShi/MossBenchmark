/*-------------------------------------------------------------------------
 *
 * tuplesort.c
 *	  Generalized tuple sorting routines.
 *
 * This module handles sorting of heap tuples, index tuples, or single
 * Datums (and could easily support other kinds of sortable objects,
 * if necessary).  It works efficiently for both small and large amounts
 * of data.  Small amounts are sorted in-memory using qsort().  Large
 * amounts are sorted using temporary files and a standard external sort
 * algorithm.
 *
 * See Knuth, volume 3, for more than you want to know about the external
 * sorting algorithm.  Historically, we divided the input into sorted runs
 * using replacement selection, in the form of a priority tree implemented
 * as a heap (essentially his Algorithm 5.2.3H), but now we always use
 * quicksort for run generation.  We merge the runs using polyphase merge,
 * Knuth's Algorithm 5.4.2D.  The logical "tapes" used by Algorithm D are
 * implemented by logtape.c, which avoids space wastage by recycling disk
 * space as soon as each block is read from its "tape".
 *
 * The approximate amount of memory allowed for any one sort operation
 * is specified in kilobytes by the caller (most pass work_mem).  Initially,
 * we absorb tuples and simply store them in an unsorted array as long as
 * we haven't exceeded workMem.  If we reach the end of the input without
 * exceeding workMem, we sort the array using qsort() and subsequently return
 * tuples just by scanning the tuple array sequentially.  If we do exceed
 * workMem, we begin to emit tuples into sorted runs in temporary tapes.
 * When tuples are dumped in batch after quicksorting, we begin a new run
 * with a new output tape (selected per Algorithm D).  After the end of the
 * input is reached, we dump out remaining tuples in memory into a final run,
 * then merge the runs using Algorithm D.
 *
 * When merging runs, we use a heap containing just the frontmost tuple from
 * each source run; we repeatedly output the smallest tuple and replace it
 * with the next tuple from its source tape (if any).  When the heap empties,
 * the merge is complete.  The basic merge algorithm thus needs very little
 * memory --- only M tuples for an M-way merge, and M is constrained to a
 * small number.  However, we can still make good use of our full workMem
 * allocation by pre-reading additional blocks from each source tape.  Without
 * prereading, our access pattern to the temporary file would be very erratic;
 * on average we'd read one block from each of M source tapes during the same
 * time that we're writing M blocks to the output tape, so there is no
 * sequentiality of access at all, defeating the read-ahead methods used by
 * most Unix kernels.  Worse, the output tape gets written into a very random
 * sequence of blocks of the temp file, ensuring that things will be even
 * worse when it comes time to read that tape.  A straightforward merge pass
 * thus ends up doing a lot of waiting for disk seeks.  We can improve matters
 * by prereading from each source tape sequentially, loading about workMem/M
 * bytes from each tape in turn, and making the sequential blocks immediately
 * available for reuse.  This approach helps to localize both read and write
 * accesses.  The pre-reading is handled by logtape.c, we just tell it how
 * much memory to use for the buffers.
 *
 * When the caller requests random access to the sort result, we form
 * the final sorted run on a logical tape which is then "frozen", so
 * that we can access it randomly.  When the caller does not need random
 * access, we return from tuplesort_performsort() as soon as we are down
 * to one run per logical tape.  The final merge is then performed
 * on-the-fly as the caller repeatedly calls tuplesort_getXXX; this
 * saves one cycle of writing all the data out to disk and reading it in.
 *
 * Before Postgres 8.2, we always used a seven-tape polyphase merge, on the
 * grounds that 7 is the "sweet spot" on the tapes-to-passes curve according
 * to Knuth's figure 70 (section 5.4.2).  However, Knuth is assuming that
 * tape drives are expensive beasts, and in particular that there will always
 * be many more runs than tape drives.  In our implementation a "tape drive"
 * doesn't cost much more than a few Kb of memory buffers, so we can afford
 * to have lots of them.  In particular, if we can have as many tape drives
 * as sorted runs, we can eliminate any repeated I/O at all.  In the current
 * code we determine the number of tapes M on the basis of workMem: we want
 * workMem/M to be large enough that we read a fair amount of data each time
 * we preread from a tape, so as to maintain the locality of access described
 * above.  Nonetheless, with large workMem we can have many tapes (but not
 * too many -- see the comments in tuplesort_merge_order).
 *
 * This module supports parallel sorting.  Parallel sorts involve coordination
 * among one or more worker processes, and a leader process, each with its own
 * tuplesort state.  The leader process (or, more accurately, the
 * Tuplesortstate associated with a leader process) creates a full tapeset
 * consisting of worker tapes with one run to merge; a run for every
 * worker process.  This is then merged.  Worker processes are guaranteed to
 * produce exactly one output run from their partial input.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/sort/tuplesort.c
 *
 *-------------------------------------------------------------------------
 */

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

/* sort-type codes for sort__start probes */
#define HEAP_SORT 0
#define INDEX_SORT 1
#define DATUM_SORT 2
#define CLUSTER_SORT 3

/* Sort parallel code from state for sort__start probes */
#define PARALLEL_SORT(state) ((state)->shared == NULL ? 0 : (state)->worker >= 0 ? 1 : 2)

/* GUC variables */
#ifdef TRACE_SORT
bool trace_sort = false;
#endif

#ifdef DEBUG_BOUNDED_SORT
bool optimize_bounded_sort = true;
#endif

/*
 * The objects we actually sort are SortTuple structs.  These contain
 * a pointer to the tuple proper (might be a MinimalTuple or IndexTuple),
 * which is a separate palloc chunk --- we assume it is just one chunk and
 * can be freed by a simple pfree() (except during merge, when we use a
 * simple slab allocator).  SortTuples also contain the tuple's first key
 * column in Datum/nullflag format, and an index integer.
 *
 * Storing the first key column lets us save heap_getattr or index_getattr
 * calls during tuple comparisons.  We could extract and save all the key
 * columns not just the first, but this would increase code complexity and
 * overhead, and wouldn't actually save any comparison cycles in the common
 * case where the first key determines the comparison result.  Note that
 * for a pass-by-reference datatype, datum1 points into the "tuple" storage.
 *
 * There is one special case: when the sort support infrastructure provides an
 * "abbreviated key" representation, where the key is (typically) a pass by
 * value proxy for a pass by reference type.  In this case, the abbreviated key
 * is stored in datum1 in place of the actual first key column.
 *
 * When sorting single Datums, the data value is represented directly by
 * datum1/isnull1 for pass by value types (or null values).  If the datatype is
 * pass-by-reference and isnull1 is false, then "tuple" points to a separately
 * palloc'd data value, otherwise "tuple" is NULL.  The value of datum1 is then
 * either the same pointer as "tuple", or is an abbreviated key value as
 * described above.  Accordingly, "tuple" is always used in preference to
 * datum1 as the authoritative value for pass-by-reference cases.
 *
 * tupindex holds the input tape number that each tuple in the heap was read
 * from during merge passes.
 */
typedef struct
{
  void *tuple;  /* the tuple itself */
  Datum datum1; /* value of first key column */
  bool isnull1; /* is first key column NULL? */
  int tupindex; /* see notes above */
} SortTuple;

/*
 * During merge, we use a pre-allocated set of fixed-size slots to hold
 * tuples.  To avoid palloc/pfree overhead.
 *
 * Merge doesn't require a lot of memory, so we can afford to waste some,
 * by using gratuitously-sized slots.  If a tuple is larger than 1 kB, the
 * palloc() overhead is not significant anymore.
 *
 * 'nextfree' is valid when this chunk is in the free list.  When in use, the
 * slot holds a tuple.
 */
#define SLAB_SLOT_SIZE 1024

typedef union SlabSlot
{
  union SlabSlot *nextfree;
  char buffer[SLAB_SLOT_SIZE];
} SlabSlot;

/*
 * Possible states of a Tuplesort object.  These denote the states that
 * persist between calls of Tuplesort routines.
 */
typedef enum
{
  TSS_INITIAL,      /* Loading tuples; still within memory limit */
  TSS_BOUNDED,      /* Loading tuples into bounded-size heap */
  TSS_BUILDRUNS,    /* Loading tuples; writing to tape */
  TSS_SORTEDINMEM,  /* Sort completed entirely in memory */
  TSS_SORTEDONTAPE, /* Sort completed, final run is on tape */
  TSS_FINALMERGE    /* Performing final merge on-the-fly */
} TupSortStatus;

/*
 * Parameters for calculation of number of tapes to use --- see inittapes()
 * and tuplesort_merge_order().
 *
 * In this calculation we assume that each tape will cost us about 1 blocks
 * worth of buffer space.  This ignores the overhead of all the other data
 * structures needed for each tape, but it's probably close enough.
 *
 * MERGE_BUFFER_SIZE is how much data we'd like to read from each input
 * tape during a preread cycle (see discussion at top of file).
 */
#define MINORDER 6   /* minimum merge order */
#define MAXORDER 500 /* maximum merge order */
#define TAPE_BUFFER_OVERHEAD BLCKSZ
#define MERGE_BUFFER_SIZE (BLCKSZ * 32)

typedef int (*SortTupleComparator)(const SortTuple *a, const SortTuple *b, Tuplesortstate *state);

/*
 * Private state of a Tuplesort operation.
 */
struct Tuplesortstate
{
  TupSortStatus status;       /* enumerated value as shown above */
  int nKeys;                  /* number of columns in sort key */
  bool randomAccess;          /* did caller request random access? */
  bool bounded;               /* did caller specify a maximum number of
                               * tuples to return? */
  bool boundUsed;             /* true if we made use of a bounded heap */
  int bound;                  /* if bounded, the maximum number of tuples */
  bool tuples;                /* Can SortTuple.tuple ever be set? */
  int64 availMem;             /* remaining memory available, in bytes */
  int64 allowedMem;           /* total memory allowed, in bytes */
  int maxTapes;               /* number of tapes (Knuth's T) */
  int tapeRange;              /* maxTapes-1 (Knuth's P) */
  MemoryContext sortcontext;  /* memory context holding most sort data */
  MemoryContext tuplecontext; /* sub-context of sortcontext for tuple data */
  LogicalTapeSet *tapeset;    /* logtape.c object for tapes in a temp file */

  /*
   * These function pointers decouple the routines that must know what kind
   * of tuple we are sorting from the routines that don't need to know it.
   * They are set up by the tuplesort_begin_xxx routines.
   *
   * Function to compare two tuples; result is per qsort() convention, ie:
   * <0, 0, >0 according as a<b, a=b, a>b.  The API must match
   * qsort_arg_comparator.
   */
  SortTupleComparator comparetup;

  /*
   * Function to copy a supplied input tuple into palloc'd space and set up
   * its SortTuple representation (ie, set tuple/datum1/isnull1).  Also,
   * state->availMem must be decreased by the amount of space used for the
   * tuple copy (note the SortTuple struct itself is not counted).
   */
  void (*copytup)(Tuplesortstate *state, SortTuple *stup, void *tup);

  /*
   * Function to write a stored tuple onto tape.  The representation of the
   * tuple on tape need not be the same as it is in memory; requirements on
   * the tape representation are given below.  Unless the slab allocator is
   * used, after writing the tuple, pfree() the out-of-line data (not the
   * SortTuple struct!), and increase state->availMem by the amount of
   * memory space thereby released.
   */
  void (*writetup)(Tuplesortstate *state, int tapenum, SortTuple *stup);

  /*
   * Function to read a stored tuple from tape back into memory. 'len' is
   * the already-read length of the stored tuple.  The tuple is allocated
   * from the slab memory arena, or is palloc'd, see readtup_alloc().
   */
  void (*readtup)(Tuplesortstate *state, SortTuple *stup, int tapenum, unsigned int len);

  /*
   * This array holds the tuples now in sort memory.  If we are in state
   * INITIAL, the tuples are in no particular order; if we are in state
   * SORTEDINMEM, the tuples are in final sorted order; in states BUILDRUNS
   * and FINALMERGE, the tuples are organized in "heap" order per Algorithm
   * H.  In state SORTEDONTAPE, the array is not used.
   */
  SortTuple *memtuples; /* array of SortTuple structs */
  int memtupcount;      /* number of tuples currently present */
  int memtupsize;       /* allocated length of memtuples array */
  bool growmemtuples;   /* memtuples' growth still underway? */

  /*
   * Memory for tuples is sometimes allocated using a simple slab allocator,
   * rather than with palloc().  Currently, we switch to slab allocation
   * when we start merging.  Merging only needs to keep a small, fixed
   * number of tuples in memory at any time, so we can avoid the
   * palloc/pfree overhead by recycling a fixed number of fixed-size slots
   * to hold the tuples.
   *
   * For the slab, we use one large allocation, divided into SLAB_SLOT_SIZE
   * slots.  The allocation is sized to have one slot per tape, plus one
   * additional slot.  We need that many slots to hold all the tuples kept
   * in the heap during merge, plus the one we have last returned from the
   * sort, with tuplesort_gettuple.
   *
   * Initially, all the slots are kept in a linked list of free slots.  When
   * a tuple is read from a tape, it is put to the next available slot, if
   * it fits.  If the tuple is larger than SLAB_SLOT_SIZE, it is palloc'd
   * instead.
   *
   * When we're done processing a tuple, we return the slot back to the free
   * list, or pfree() if it was palloc'd.  We know that a tuple was
   * allocated from the slab, if its pointer value is between
   * slabMemoryBegin and -End.
   *
   * When the slab allocator is used, the USEMEM/LACKMEM mechanism of
   * tracking memory usage is not used.
   */
  bool slabAllocatorUsed;

  char *slabMemoryBegin;  /* beginning of slab memory arena */
  char *slabMemoryEnd;    /* end of slab memory arena */
  SlabSlot *slabFreeHead; /* head of free list */

  /* Buffer size to use for reading input tapes, during merge. */
  size_t read_buffer_size;

  /*
   * When we return a tuple to the caller in tuplesort_gettuple_XXX, that
   * came from a tape (that is, in TSS_SORTEDONTAPE or TSS_FINALMERGE
   * modes), we remember the tuple in 'lastReturnedTuple', so that we can
   * recycle the memory on next gettuple call.
   */
  void *lastReturnedTuple;

  /*
   * While building initial runs, this is the current output run number.
   * Afterwards, it is the number of initial runs we made.
   */
  int currentRun;

  /*
   * Unless otherwise noted, all pointer variables below are pointers to
   * arrays of length maxTapes, holding per-tape data.
   */

  /*
   * This variable is only used during merge passes.  mergeactive[i] is true
   * if we are reading an input run from (actual) tape number i and have not
   * yet exhausted that run.
   */
  bool *mergeactive; /* active input run source? */

  /*
   * Variables for Algorithm D.  Note that destTape is a "logical" tape
   * number, ie, an index into the tp_xxx[] arrays.  Be careful to keep
   * "logical" and "actual" tape numbers straight!
   */
  int Level;       /* Knuth's l */
  int destTape;    /* current output tape (Knuth's j, less 1) */
  int *tp_fib;     /* Target Fibonacci run counts (A[]) */
  int *tp_runs;    /* # of real runs on each tape */
  int *tp_dummy;   /* # of dummy runs for each tape (D[]) */
  int *tp_tapenum; /* Actual tape numbers (TAPE[]) */
  int activeTapes; /* # of active input tapes in merge pass */

  /*
   * These variables are used after completion of sorting to keep track of
   * the next tuple to return.  (In the tape case, the tape's current read
   * position is also critical state.)
   */
  int result_tape;  /* actual tape number of finished output */
  int current;      /* array index (only used if SORTEDINMEM) */
  bool eof_reached; /* reached EOF (needed for cursors) */

  /* markpos_xxx holds marked position for mark and restore */
  long markpos_block; /* tape block# (only used if SORTEDONTAPE) */
  int markpos_offset; /* saved "current", or offset in tape block */
  bool markpos_eof;   /* saved "eof_reached" */

  /*
   * These variables are used during parallel sorting.
   *
   * worker is our worker identifier.  Follows the general convention that
   * -1 value relates to a leader tuplesort, and values >= 0 worker
   * tuplesorts. (-1 can also be a serial tuplesort.)
   *
   * shared is mutable shared memory state, which is used to coordinate
   * parallel sorts.
   *
   * nParticipants is the number of worker Tuplesortstates known by the
   * leader to have actually been launched, which implies that they must
   * finish a run leader can merge.  Typically includes a worker state held
   * by the leader process itself.  Set in the leader Tuplesortstate only.
   */
  int worker;
  Sharedsort *shared;
  int nParticipants;

  /*
   * The sortKeys variable is used by every case other than the hash index
   * case; it is set by tuplesort_begin_xxx.  tupDesc is only used by the
   * MinimalTuple and CLUSTER routines, though.
   */
  TupleDesc tupDesc;
  SortSupport sortKeys; /* array of length nKeys */

  /*
   * This variable is shared by the single-key MinimalTuple case and the
   * Datum case (which both use qsort_ssup()).  Otherwise it's NULL.
   */
  SortSupport onlyKey;

  /*
   * Additional state for managing "abbreviated key" sortsupport routines
   * (which currently may be used by all cases except the hash index case).
   * Tracks the intervals at which the optimization's effectiveness is
   * tested.
   */
  int64 abbrevNext; /* Tuple # at which to next check
                     * applicability */

  /*
   * These variables are specific to the CLUSTER case; they are set by
   * tuplesort_begin_cluster.
   */
  IndexInfo *indexInfo; /* info about index being used for reference */
  EState *estate;       /* for evaluating index expressions */

  /*
   * These variables are specific to the IndexTuple case; they are set by
   * tuplesort_begin_index_xxx and used only by the IndexTuple routines.
   */
  Relation heapRel;  /* table the index is being built on */
  Relation indexRel; /* index being built */

  /* These are specific to the index_btree subcase: */
  bool enforceUnique; /* complain if we find duplicate tuples */

  /* These are specific to the index_hash subcase: */
  uint32 high_mask; /* masks for sortable part of hash code */
  uint32 low_mask;
  uint32 max_buckets;

  /*
   * These variables are specific to the Datum case; they are set by
   * tuplesort_begin_datum and used only by the DatumTuple routines.
   */
  Oid datumType;
  /* we need typelen in order to know how to copy the Datums. */
  int datumTypeLen;

  /*
   * Resource snapshot for time of sort start.
   */
#ifdef TRACE_SORT
  PGRUsage ru_start;
#endif
};

/*
 * Private mutable state of tuplesort-parallel-operation.  This is allocated
 * in shared memory.
 */
struct Sharedsort
{
  /* mutex protects all fields prior to tapes */
  slock_t mutex;

  /*
   * currentWorker generates ordinal identifier numbers for parallel sort
   * workers.  These start from 0, and are always gapless.
   *
   * Workers increment workersFinished to indicate having finished.  If this
   * is equal to state.nParticipants within the leader, leader is ready to
   * merge worker runs.
   */
  int currentWorker;
  int workersFinished;

  /* Temporary file space */
  SharedFileSet fileset;

  /* Size of tapes flexible array */
  int nTapes;

  /*
   * Tapes array used by workers to report back information needed by the
   * leader to concatenate all worker tapes into one for merging
   */
  TapeShare tapes[FLEXIBLE_ARRAY_MEMBER];
};

/*
 * Is the given tuple allocated from the slab memory arena?
 */
#define IS_SLAB_SLOT(state, tuple) ((char *)(tuple) >= (state)->slabMemoryBegin && (char *)(tuple) < (state)->slabMemoryEnd)

/*
 * Return the given tuple to the slab memory free list, or free it
 * if it was palloc'd.
 */
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

/*
 * NOTES about on-tape representation of tuples:
 *
 * We require the first "unsigned int" of a stored tuple to be the total size
 * on-tape of the tuple, including itself (so it is never zero; an all-zero
 * unsigned int is used to delimit runs).  The remainder of the stored tuple
 * may or may not match the in-memory representation of the tuple ---
 * any conversion needed is the job of the writetup and readtup routines.
 *
 * If state->randomAccess is true, then the stored representation of the
 * tuple must be followed by another "unsigned int" that is a copy of the
 * length --- so the total tape space used is actually sizeof(unsigned int)
 * more than the stored length value.  This allows read-backwards.  When
 * randomAccess is not true, the write/read routines may omit the extra
 * length word.
 *
 * writetup is expected to write both length words as well as the tuple
 * data.  When readtup is called, the tape is positioned just after the
 * front length word; readtup must read the tuple data and advance past
 * the back length word (if present).
 *
 * The write/read routines can make use of the tuple description data
 * stored in the Tuplesortstate record, if needed.  They are also expected
 * to adjust state->availMem by the amount of memory space (not tape space!)
 * released or consumed.  There is no error return from either writetup
 * or readtup; they should ereport() on failure.
 *
 *
 * NOTES about memory consumption calculations:
 *
 * We count space allocated for tuples against the workMem limit, plus
 * the space used by the variable-size memtuples array.  Fixed-size space
 * is not counted; it's small enough to not be interesting.
 *
 * Note that we count actual space used (as shown by GetMemoryChunkSpace)
 * rather than the originally-requested size.  This is important since
 * palloc can add substantial overhead.  It's not a complete answer since
 * we won't count any wasted space in palloc allocation blocks, but it's
 * a lot better than what we were doing before 7.3.  As of 9.6, a
 * separate memory context is used for caller passed tuples.  Resetting
 * it at certain key increments significantly ameliorates fragmentation.
 * Note that this places a responsibility on readtup and copytup routines
 * to use the right memory context for these tuples (and to not use the
 * reset context for anything whose lifetime needs to span multiple
 * external sort runs).
 */

/* When using this macro, beware of double evaluation of len */
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

/*
 * Special versions of qsort just for SortTuple objects.  qsort_tuple() sorts
 * any variant of SortTuples, using the appropriate comparetup function.
 * qsort_ssup() is specialized for the case where the comparetup function
 * reduces to ApplySortComparator(), that is single-key MinimalTuple sorts
 * and Datum sorts.
 */
#include "qsort_tuple.c"

/*
 *		tuplesort_begin_xxx
 *
 * Initialize for a tuple sort operation.
 *
 * After calling tuplesort_begin, the caller should call tuplesort_putXXX
 * zero or more times, then call tuplesort_performsort when all the tuples
 * have been supplied.  After performsort, retrieve the tuples in sorted
 * order by calling tuplesort_getXXX until it returns false/NULL.  (If random
 * access was requested, rescan, markpos, and restorepos can also be called.)
 * Call tuplesort_end to terminate the operation and release memory/disk space.
 *
 * Each variant of tuplesort_begin has a workMem parameter specifying the
 * maximum number of kilobytes of RAM to use before spilling data to disk.
 * (The normal value of this parameter is work_mem, but some callers use
 * other values.)  Each variant also has a randomAccess parameter specifying
 * whether the caller needs non-sequential access to the sort result.
 */

static Tuplesortstate *
tuplesort_begin_common(int workMem, SortCoordinate coordinate, bool randomAccess)
{
























































































































}

Tuplesortstate *
tuplesort_begin_heap(TupleDesc tupDesc, int nkeys, AttrNumber *attNums, Oid *sortOperators, Oid *sortCollations, bool *nullsFirstFlags, int workMem, SortCoordinate coordinate, bool randomAccess)
{






























































}

Tuplesortstate *
tuplesort_begin_cluster(TupleDesc tupDesc, Relation indexRel, int workMem, SortCoordinate coordinate, bool randomAccess)
{
















































































}

Tuplesortstate *
tuplesort_begin_index_btree(Relation heapRel, Relation indexRel, bool enforceUnique, int workMem, SortCoordinate coordinate, bool randomAccess)
{


























































}

Tuplesortstate *
tuplesort_begin_index_hash(Relation heapRel, Relation indexRel, uint32 high_mask, uint32 low_mask, uint32 max_buckets, int workMem, SortCoordinate coordinate, bool randomAccess)
{





























}

Tuplesortstate *
tuplesort_begin_datum(Oid datumType, Oid sortOperator, Oid sortCollation, bool nullsFirstFlag, int workMem, SortCoordinate coordinate, bool randomAccess)
{

































































}

/*
 * tuplesort_set_bound
 *
 *	Advise tuplesort that at most the first N result tuples are required.
 *
 * Must be called before inserting any tuples.  (Actually, we could allow it
 * as long as the sort hasn't spilled to disk, but there seems no need for
 * delayed calls at the moment.)
 *
 * This is a hint only. The tuplesort may still return more tuples than
 * requested.  Parallel leader tuplesorts will always ignore the hint.
 */
void
tuplesort_set_bound(Tuplesortstate *state, int64 bound)
{











































}

/*
 * tuplesort_end
 *
 *	Release resources and clean up.
 *
 * NOTE: after calling this, any pointers returned by tuplesort_getXXX are
 * pointing to garbage.  Be careful not to attempt to use or free such
 * pointers afterwards!
 */
void
tuplesort_end(Tuplesortstate *state)
{


































































}

/*
 * Grow the memtuples[] array, if possible within our memory constraint.  We
 * must not exceed INT_MAX tuples in memory or the caller-provided memory
 * limit.  Return true if we were able to enlarge the array, false if not.
 *
 * Normally, at each increment we double the size of the array.  When doing
 * that would exceed a limit, we attempt one last, smaller increase (and then
 * clear the growmemtuples flag so we don't try any more).  That allows us to
 * use memory as fully as permitted; sticking to the pure doubling rule could
 * result in almost half going unused.  Because availMem moves around with
 * tuple addition/removal, we need some rule to prevent making repeated small
 * increases in memtupsize, which would just be useless thrashing.  The
 * growmemtuples flag accomplishes that and also prevents useless
 * recalculations in this function.
 */
static bool
grow_memtuples(Tuplesortstate *state)
{

























































































































}

/*
 * Accept one tuple while collecting input data for sort.
 *
 * Note that the input data is always copied; the caller need not save it.
 */
void
tuplesort_puttupleslot(Tuplesortstate *state, TupleTableSlot *slot)
{












}

/*
 * Accept one tuple while collecting input data for sort.
 *
 * Note that the input data is always copied; the caller need not save it.
 */
void
tuplesort_putheaptuple(Tuplesortstate *state, HeapTuple tup)
{












}

/*
 * Collect one index tuple while collecting input data for sort, building
 * it from caller-supplied values.
 */
void
tuplesort_putindextuplevalues(Tuplesortstate *state, Relation rel, ItemPointer self, Datum *values, bool *isnull)
{



























































}

/*
 * Accept one Datum while collecting input data for sort.
 *
 * If the Datum is pass-by-ref type, the value will be copied.
 */
void
tuplesort_putdatum(Tuplesortstate *state, Datum val, bool isNull)
{









































































}

/*
 * Shared code for tuple and datum cases.
 */
static void
puttuple_common(Tuplesortstate *state, SortTuple *tuple)
{








































































































}

static bool
consider_abort_common(Tuplesortstate *state)
{




































}

/*
 * All tuples have been provided; finish the sort.
 */
void
tuplesort_performsort(Tuplesortstate *state)
{





































































































}

/*
 * Internal routine to fetch the next tuple in either forward or back
 * direction into *stup.  Returns false if no more tuples.
 * Returned tuple belongs to tuplesort memory context, and must not be freed
 * by caller.  Note that fetched tuple is stored in memory that may be
 * recycled by any future fetch.
 */
static bool
tuplesort_gettuple_common(Tuplesortstate *state, bool forward, SortTuple *stup)
{





















































































































































































































































}

/*
 * Fetch the next tuple in either forward or back direction.
 * If successful, put tuple in slot and return true; else, clear the slot
 * and return false.
 *
 * Caller may optionally be passed back abbreviated value (on true return
 * value) when abbreviation was used, which can be used to cheaply avoid
 * equality checks that might otherwise be required.  Caller can safely make a
 * determination of "non-equal tuple" based on simple binary inequality.  A
 * NULL value in leading attribute will set abbreviated value to zeroed
 * representation, which caller may rely on in abbreviated inequality check.
 *
 * If copy is true, the slot receives a tuple that's been copied into the
 * caller's memory context, so that it will stay valid regardless of future
 * manipulations of the tuplesort's state (up to and including deleting the
 * tuplesort).  If copy is false, the slot will just receive a pointer to a
 * tuple held within the tuplesort, which is more efficient, but only safe for
 * callers that are prepared to have any subsequent manipulation of the
 * tuplesort's state invalidate slot contents.
 */
bool
tuplesort_gettupleslot(Tuplesortstate *state, bool forward, bool copy, TupleTableSlot *slot, Datum *abbrev)
{































}

/*
 * Fetch the next tuple in either forward or back direction.
 * Returns NULL if no more tuples.  Returned tuple belongs to tuplesort memory
 * context, and must not be freed by caller.  Caller may not rely on tuple
 * remaining valid after any further manipulation of tuplesort.
 */
HeapTuple
tuplesort_getheaptuple(Tuplesortstate *state, bool forward)
{











}

/*
 * Fetch the next index tuple in either forward or back direction.
 * Returns NULL if no more tuples.  Returned tuple belongs to tuplesort memory
 * context, and must not be freed by caller.  Caller may not rely on tuple
 * remaining valid after any further manipulation of tuplesort.
 */
IndexTuple
tuplesort_getindextuple(Tuplesortstate *state, bool forward)
{











}

/*
 * Fetch the next Datum in either forward or back direction.
 * Returns false if no more datums.
 *
 * If the Datum is pass-by-ref type, the returned value is freshly palloc'd
 * in caller's context, and is now owned by the caller (this differs from
 * similar routines for other types of tuplesorts).
 *
 * Caller may optionally be passed back abbreviated value (on true return
 * value) when abbreviation was used, which can be used to cheaply avoid
 * equality checks that might otherwise be required.  Caller can safely make a
 * determination of "non-equal tuple" based on simple binary inequality.  A
 * NULL value will have a zeroed abbreviated value representation, which caller
 * may rely on in abbreviated inequality check.
 */
bool
tuplesort_getdatum(Tuplesortstate *state, bool forward, Datum *val, bool *isNull, Datum *abbrev)
{































}

/*
 * Advance over N tuples in either forward or back direction,
 * without returning any data.  N==0 is a no-op.
 * Returns true if successful, false if ran out of tuples.
 */
bool
tuplesort_skiptuples(Tuplesortstate *state, int64 ntuples, bool forward)
{



























































}

/*
 * tuplesort_merge_order - report merge order we'll use for given memory
 * (note: "merge order" just means the number of input tapes in the merge).
 *
 * This is exported for use by the planner.  allowedMem is in bytes.
 */
int
tuplesort_merge_order(int64 allowedMem)
{





























}

/*
 * inittapes - initialize for tape sorting.
 *
 * This is called only if we have found we won't sort in memory.
 */
static void
inittapes(Tuplesortstate *state, bool mergeruns)
{














































}

/*
 * inittapestate - initialize generic tape management state
 */
static void
inittapestate(Tuplesortstate *state, int maxTapes)
{



































}

/*
 * selectnewtape -- select new tape for new initial run.
 *
 * This is called after finishing a run when we know another run
 * must be started.  This implements steps D3, D4 of Algorithm D.
 */
static void
selectnewtape(Tuplesortstate *state)
{
























}

/*
 * Initialize the slab allocation arena, for the given number of slots.
 */
static void
init_slab_allocator(Tuplesortstate *state, int numSlots)
{
























}

/*
 * mergeruns -- merge all the completed initial runs.
 *
 * This implements steps D5, D6 of Algorithm D.  All input data has
 * already been written to initial runs on tape (see dumptuples).
 */
static void
mergeruns(Tuplesortstate *state)
{






































































































































































































































}

/*
 * Merge one run from each input tape, except ones with dummy runs.
 *
 * This is the inner loop of Algorithm D step D5.  We know that the
 * output tape is TAPE[T].
 */
static void
mergeonerun(Tuplesortstate *state)
{
























































}

/*
 * beginmerge - initialize for a merge pass
 *
 * We decrease the counts of real and dummy runs for each tape, and mark
 * which tapes contain active input runs in mergeactive[].  Then, fill the
 * merge heap with the first tuple from each active tape.
 */
static void
beginmerge(Tuplesortstate *state)
{







































}

/*
 * mergereadnext - read next tuple from one merge input tape
 *
 * Returns false on EOF.
 */
static bool
mergereadnext(Tuplesortstate *state, int srcTape, SortTuple *stup)
{
















}

/*
 * dumptuples - remove tuples from memtuples and write initial run to tape
 *
 * When alltuples = true, dump everything currently in memory.  (This case is
 * only used at end of input data.)
 */
static void
dumptuples(Tuplesortstate *state, bool alltuples)
{
































































































}

/*
 * tuplesort_rescan		- rewind and replay the scan
 */
void
tuplesort_rescan(Tuplesortstate *state)
{

























}

/*
 * tuplesort_markpos	- saves current position in the merged sort file
 */
void
tuplesort_markpos(Tuplesortstate *state)
{




















}

/*
 * tuplesort_restorepos - restores current position in merged sort file to
 *						  last saved position
 */
void
tuplesort_restorepos(Tuplesortstate *state)
{




















}

/*
 * tuplesort_get_stats - extract summary statistics
 *
 * This can be called after tuplesort_performsort() finishes to obtain
 * printable summary information about how the sort was performed.
 */
void
tuplesort_get_stats(Tuplesortstate *state, TuplesortInstrumentation *stats)
{










































}

/*
 * Convert TuplesortMethod to a string.
 */
const char *
tuplesort_method_name(TuplesortMethod m)
{















}

/*
 * Convert TuplesortSpaceType to a string.
 */
const char *
tuplesort_space_type_name(TuplesortSpaceType t)
{


}

/*
 * Heap manipulation routines, per Knuth's Algorithm 5.2.3H.
 */

/*
 * Convert the existing unordered array of SortTuples to a bounded heap,
 * discarding all but the smallest "state->bound" tuples.
 *
 * When working with a bounded heap, we want to keep the largest entry
 * at the root (array entry zero), instead of the smallest as in the normal
 * sort case.  This allows us to discard the largest entry cheaply.
 * Therefore, we temporarily reverse the sort direction.
 */
static void
make_bounded_heap(Tuplesortstate *state)
{











































}

/*
 * Convert the bounded heap to a properly-sorted array
 */
static void
sort_bounded_heap(Tuplesortstate *state)
{






























}

/*
 * Sort all memtuples using specialized qsort() routines.
 *
 * Quicksort is used for small in-memory sorts, and external sort runs.
 */
static void
tuplesort_sort_memtuples(Tuplesortstate *state)
{














}

/*
 * Insert a new tuple into an empty or existing heap, maintaining the
 * heap invariant.  Caller is responsible for ensuring there's room.
 *
 * Note: For some callers, tuple points to a memtuples[] entry above the
 * end of the heap.  This is safe as long as it's not immediately adjacent
 * to the end of the heap (ie, in the [memtupcount] array entry) --- if it
 * is, it might get overwritten before being moved into the heap!
 */
static void
tuplesort_heap_insert(Tuplesortstate *state, SortTuple *tuple)
{

























}

/*
 * Remove the tuple at state->memtuples[0] from the heap.  Decrement
 * memtupcount, and sift up to maintain the heap invariant.
 *
 * The caller has already free'd the tuple the top node points to,
 * if necessary.
 */
static void
tuplesort_heap_delete_top(Tuplesortstate *state)
{














}

/*
 * Replace the tuple at state->memtuples[0] with a new tuple.  Sift up to
 * maintain the heap invariant.
 *
 * This corresponds to Knuth's "sift-up" algorithm (Algorithm 5.2.3H,
 * Heapsort, steps H3-H8).
 */
static void
tuplesort_heap_replace_top(Tuplesortstate *state, SortTuple *tuple)
{


































}

/*
 * Function to reverse the sort direction from its current state
 *
 * It is not safe to call this when performing hash tuplesorts
 */
static void
reversedirection(Tuplesortstate *state)
{








}

/*
 * Tape interface routines
 */

static unsigned int
getlen(Tuplesortstate *state, int tapenum, bool eofOK)
{











}

static void
markrunend(Tuplesortstate *state, int tapenum)
{



}

/*
 * Get memory for tuple from within READTUP() routine.
 *
 * We use next free slot from the slab allocator, or palloc() if the tuple
 * is too large for that.
 */
static void *
readtup_alloc(Tuplesortstate *state, Size tuplen)
{




















}

/*
 * Routines specialized for HeapTuple (actually MinimalTuple) case
 */

static int
comparetup_heap(const SortTuple *a, const SortTuple *b, Tuplesortstate *state)
{






















































}

static void
copytup_heap(Tuplesortstate *state, SortTuple *stup, void *tup)
{
































































}

static void
writetup_heap(Tuplesortstate *state, int tapenum, SortTuple *stup)
{





















}

static void
readtup_heap(Tuplesortstate *state, SortTuple *stup, int tapenum, unsigned int len)
{


















}

/*
 * Routines specialized for the CLUSTER case (HeapTuple data, with
 * comparisons per a btree index definition)
 */

static int
comparetup_cluster(const SortTuple *a, const SortTuple *b, Tuplesortstate *state)
{



































































































}

static void
copytup_cluster(Tuplesortstate *state, SortTuple *stup, void *tup)
{































































}

static void
writetup_cluster(Tuplesortstate *state, int tapenum, SortTuple *stup)
{

















}

static void
readtup_cluster(Tuplesortstate *state, SortTuple *stup, int tapenum, unsigned int tuplen)
{





















}

/*
 * Routines specialized for IndexTuple case
 *
 * The btree and hash cases require separate comparison functions, but the
 * IndexTuple representation is the same so the copy/write/read support
 * functions can be shared.
 */

static int
comparetup_index_btree(const SortTuple *a, const SortTuple *b, Tuplesortstate *state)
{





























































































































}

static int
comparetup_index_hash(const SortTuple *a, const SortTuple *b, Tuplesortstate *state)
{





















































}

static void
copytup_index(Tuplesortstate *state, SortTuple *stup, void *tup)
{






















































}

static void
writetup_index(Tuplesortstate *state, int tapenum, SortTuple *stup)
{
















}

static void
readtup_index(Tuplesortstate *state, SortTuple *stup, int tapenum, unsigned int len)
{











}

/*
 * Routines specialized for DatumTuple case
 */

static int
comparetup_datum(const SortTuple *a, const SortTuple *b, Tuplesortstate *state)
{
















}

static void
copytup_datum(Tuplesortstate *state, SortTuple *stup, void *tup)
{


}

static void
writetup_datum(Tuplesortstate *state, int tapenum, SortTuple *stup)
{



































}

static void
readtup_datum(Tuplesortstate *state, SortTuple *stup, int tapenum, unsigned int len)
{






























}

/*
 * Parallel sort routines
 */

/*
 * tuplesort_estimate_shared - estimate required shared memory allocation
 *
 * nWorkers is an estimate of the number of workers (it's the number that
 * will be requested).
 */
Size
tuplesort_estimate_shared(int nWorkers)
{









}

/*
 * tuplesort_initialize_shared - initialize shared tuplesort state
 *
 * Must be called from leader process before workers are launched, to
 * establish state needed up-front for worker tuplesortstates.  nWorkers
 * should match the argument passed to tuplesort_estimate_shared().
 */
void
tuplesort_initialize_shared(Sharedsort *shared, int nWorkers, dsm_segment *seg)
{













}

/*
 * tuplesort_attach_shared - attach to shared tuplesort state
 *
 * Must be called by all worker processes.
 */
void
tuplesort_attach_shared(Sharedsort *shared, dsm_segment *seg)
{


}

/*
 * worker_get_identifier - Assign and return ordinal identifier for worker
 *
 * The order in which these are assigned is not well defined, and should not
 * matter; worker numbers across parallel sort participants need only be
 * distinct and gapless.  logtape.c requires this.
 *
 * Note that the identifiers assigned from here have no relation to
 * ParallelWorkerNumber number, to avoid making any assumption about
 * caller's requirements.  However, we do follow the ParallelWorkerNumber
 * convention of representing a non-worker with worker number -1.  This
 * includes the leader, as well as serial Tuplesort processes.
 */
static int
worker_get_identifier(Tuplesortstate *state)
{










}

/*
 * worker_freeze_result_tape - freeze worker's result tape for leader
 *
 * This is called by workers just after the result tape has been determined,
 * instead of calling LogicalTapeFreeze() directly.  They do so because
 * workers require a few additional steps over similar serial
 * TSS_SORTEDONTAPE external sort cases, which also happen here.  The extra
 * steps are around freeing now unneeded resources, and representing to
 * leader that worker's input run is available for its merge.
 *
 * There should only be one final output run for each worker, which consists
 * of all tuples that were originally input into worker.
 */
static void
worker_freeze_result_tape(Tuplesortstate *state)
{



























}

/*
 * worker_nomergeruns - dump memtuples in worker, without merging
 *
 * This called as an alternative to mergeruns() with a worker when no
 * merging is required.
 */
static void
worker_nomergeruns(Tuplesortstate *state)
{





}

/*
 * leader_takeover_tapes - create tapeset for leader from worker tapes
 *
 * So far, leader Tuplesortstate has performed no actual sorting.  By now, all
 * sorting has occurred in workers, all of which must have already returned
 * from tuplesort_performsort().
 *
 * When this returns, leader process is left in a state that is virtually
 * indistinguishable from it having generated runs as a serial external sort
 * might have.
 */
static void
leader_takeover_tapes(Tuplesortstate *state)
{


























































}

/*
 * Convenience routine to free a tuple previously loaded into sort memory
 */
static void
free_sort_tuple(Tuplesortstate *state, SortTuple *stup)
{






}