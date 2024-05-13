/*-------------------------------------------------------------------------
 *
 * tuplestore.c
 *	  Generalized routines for temporary tuple storage.
 *
 * This module handles temporary storage of tuples for purposes such
 * as Materialize nodes, hashjoin batch files, etc.  It is essentially
 * a dumbed-down version of tuplesort.c; it does no sorting of tuples
 * but can only store and regurgitate a sequence of tuples.  However,
 * because no sort is required, it is allowed to start reading the sequence
 * before it has all been written.  This is particularly useful for cursors,
 * because it allows random access within the already-scanned portion of
 * a query without having to process the underlying scan to completion.
 * Also, it is possible to support multiple independent read pointers.
 *
 * A temporary file is used to handle the data if it exceeds the
 * space limit specified by the caller.
 *
 * The (approximate) amount of memory allowed to the tuplestore is specified
 * in kilobytes by the caller.  We absorb tuples and simply store them in an
 * in-memory array as long as we haven't exceeded maxKBytes.  If we do exceed
 * maxKBytes, we dump all the tuples into a temp file and then read from that
 * when needed.
 *
 * Upon creation, a tuplestore supports a single read pointer, numbered 0.
 * Additional read pointers can be created using tuplestore_alloc_read_pointer.
 * Mark/restore behavior is supported by copying read pointers.
 *
 * When the caller requests backward-scan capability, we write the temp file
 * in a format that allows either forward or backward scan.  Otherwise, only
 * forward scan is allowed.  A request for backward scan must be made before
 * putting any tuples into the tuplestore.  Rewind is normally allowed but
 * can be turned off via tuplestore_set_eflags; turning off rewind for all
 * read pointers enables truncation of the tuplestore at the oldest read point
 * for minimal memory usage.  (The caller must explicitly call tuplestore_trim
 * at appropriate times for truncation to actually happen.)
 *
 * Note: in TSS_WRITEFILE state, the temp file's seek position is the
 * current write position, and the write-position variables in the tuplestore
 * aren't kept up to date.  Similarly, in TSS_READFILE state the temp file's
 * seek position is the active read pointer's position, and that read pointer
 * isn't kept up to date.  We update the appropriate variables using ftell()
 * before switching to the other state or activating a different read pointer.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/sort/tuplestore.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <limits.h>

#include "access/htup_details.h"
#include "commands/tablespace.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "storage/buffile.h"
#include "utils/memutils.h"
#include "utils/resowner.h"

/*
 * Possible states of a Tuplestore object.  These denote the states that
 * persist between calls of Tuplestore routines.
 */
typedef enum
{
  TSS_INMEM,     /* Tuples still fit in memory */
  TSS_WRITEFILE, /* Writing to temp file */
  TSS_READFILE   /* Reading from temp file */
} TupStoreStatus;

/*
 * State for a single read pointer.  If we are in state INMEM then all the
 * read pointers' "current" fields denote the read positions.  In state
 * WRITEFILE, the file/offset fields denote the read positions.  In state
 * READFILE, inactive read pointers have valid file/offset, but the active
 * read pointer implicitly has position equal to the temp file's seek position.
 *
 * Special case: if eof_reached is true, then the pointer's read position is
 * implicitly equal to the write position, and current/file/offset aren't
 * maintained.  This way we need not update all the read pointers each time
 * we write.
 */
typedef struct
{
  int eflags;       /* capability flags */
  bool eof_reached; /* read has reached EOF */
  int current;      /* next array index to read */
  int file;         /* temp file# */
  off_t offset;     /* byte offset in file */
} TSReadPointer;

/*
 * Private state of a Tuplestore operation.
 */
struct Tuplestorestate
{
  TupStoreStatus status;  /* enumerated value as shown above */
  int eflags;             /* capability flags (OR of pointers' flags) */
  bool backward;          /* store extra length words in file? */
  bool interXact;         /* keep open through transactions? */
  bool truncated;         /* tuplestore_trim has removed tuples? */
  int64 availMem;         /* remaining memory available, in bytes */
  int64 allowedMem;       /* total memory allowed, in bytes */
  int64 tuples;           /* number of tuples added */
  BufFile *myfile;        /* underlying file, or NULL if none */
  MemoryContext context;  /* memory context for holding tuples */
  ResourceOwner resowner; /* resowner for holding temp files */

  /*
   * These function pointers decouple the routines that must know what kind
   * of tuple we are handling from the routines that don't need to know it.
   * They are set up by the tuplestore_begin_xxx routines.
   *
   * (Although tuplestore.c currently only supports heap tuples, I've copied
   * this part of tuplesort.c so that extension to other kinds of objects
   * will be easy if it's ever needed.)
   *
   * Function to copy a supplied input tuple into palloc'd space. (NB: we
   * assume that a single pfree() is enough to release the tuple later, so
   * the representation must be "flat" in one palloc chunk.) state->availMem
   * must be decreased by the amount of space used.
   */
  void *(*copytup)(Tuplestorestate *state, void *tup);

  /*
   * Function to write a stored tuple onto tape.  The representation of the
   * tuple on tape need not be the same as it is in memory; requirements on
   * the tape representation are given below.  After writing the tuple,
   * pfree() it, and increase state->availMem by the amount of memory space
   * thereby released.
   */
  void (*writetup)(Tuplestorestate *state, void *tup);

  /*
   * Function to read a stored tuple from tape back into memory. 'len' is
   * the already-read length of the stored tuple.  Create and return a
   * palloc'd copy, and decrease state->availMem by the amount of memory
   * space consumed.
   */
  void *(*readtup)(Tuplestorestate *state, unsigned int len);

  /*
   * This array holds pointers to tuples in memory if we are in state INMEM.
   * In states WRITEFILE and READFILE it's not used.
   *
   * When memtupdeleted > 0, the first memtupdeleted pointers are already
   * released due to a tuplestore_trim() operation, but we haven't expended
   * the effort to slide the remaining pointers down.  These unused pointers
   * are set to NULL to catch any invalid accesses.  Note that memtupcount
   * includes the deleted pointers.
   */
  void **memtuples;   /* array of pointers to palloc'd tuples */
  int memtupdeleted;  /* the first N slots are currently unused */
  int memtupcount;    /* number of tuples currently present */
  int memtupsize;     /* allocated length of memtuples array */
  bool growmemtuples; /* memtuples' growth still underway? */

  /*
   * These variables are used to keep track of the current positions.
   *
   * In state WRITEFILE, the current file seek position is the write point;
   * in state READFILE, the write position is remembered in writepos_xxx.
   * (The write position is the same as EOF, but since BufFileSeek doesn't
   * currently implement SEEK_END, we have to remember it explicitly.)
   */
  TSReadPointer *readptrs; /* array of read pointers */
  int activeptr;           /* index of the active read pointer */
  int readptrcount;        /* number of pointers currently valid */
  int readptrsize;         /* allocated length of readptrs array */

  int writepos_file;     /* file# (valid if READFILE state) */
  off_t writepos_offset; /* offset (valid if READFILE state) */
};

#define COPYTUP(state, tup) ((*(state)->copytup)(state, tup))
#define WRITETUP(state, tup) ((*(state)->writetup)(state, tup))
#define READTUP(state, len) ((*(state)->readtup)(state, len))
#define LACKMEM(state) ((state)->availMem < 0)
#define USEMEM(state, amt) ((state)->availMem -= (amt))
#define FREEMEM(state, amt) ((state)->availMem += (amt))

/*--------------------
 *
 * NOTES about on-tape representation of tuples:
 *
 * We require the first "unsigned int" of a stored tuple to be the total size
 * on-tape of the tuple, including itself (so it is never zero).
 * The remainder of the stored tuple
 * may or may not match the in-memory representation of the tuple ---
 * any conversion needed is the job of the writetup and readtup routines.
 *
 * If state->backward is true, then the stored representation of
 * the tuple must be followed by another "unsigned int" that is a copy of the
 * length --- so the total tape space used is actually sizeof(unsigned int)
 * more than the stored length value.  This allows read-backwards.  When
 * state->backward is not set, the write/read routines may omit the extra
 * length word.
 *
 * writetup is expected to write both length words as well as the tuple
 * data.  When readtup is called, the tape is positioned just after the
 * front length word; readtup must read the tuple data and advance past
 * the back length word (if present).
 *
 * The write/read routines can make use of the tuple description data
 * stored in the Tuplestorestate record, if needed. They are also expected
 * to adjust state->availMem by the amount of memory space (not tape space!)
 * released or consumed.  There is no error return from either writetup
 * or readtup; they should ereport() on failure.
 *
 *
 * NOTES about memory consumption calculations:
 *
 * We count space allocated for tuples against the maxKBytes limit,
 * plus the space used by the variable-size array memtuples.
 * Fixed-size space (primarily the BufFile I/O buffer) is not counted.
 * We don't worry about the size of the read pointer array, either.
 *
 * Note that we count actual space used (as shown by GetMemoryChunkSpace)
 * rather than the originally-requested size.  This is important since
 * palloc can add substantial overhead.  It's not a complete answer since
 * we won't count any wasted space in palloc allocation blocks, but it's
 * a lot better than what we were doing before 7.3.
 *
 *--------------------
 */

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

/*
 *		tuplestore_begin_xxx
 *
 * Initialize for a tuple store operation.
 */
static Tuplestorestate *
tuplestore_begin_common(int eflags, bool interXact, int maxKBytes)
{







































}

/*
 * tuplestore_begin_heap
 *
 * Create a new tuplestore; other types of tuple stores (other than
 * "heap" tuple stores, for heap tuples) are possible, but not presently
 * implemented.
 *
 * randomAccess: if true, both forward and backward accesses to the
 * tuple store are allowed.
 *
 * interXact: if true, the files used for on-disk storage persist beyond the
 * end of the current transaction.  NOTE: It's the caller's responsibility to
 * create such a tuplestore in a memory context and resource owner that will
 * also survive transaction boundaries, and to ensure the tuplestore is closed
 * when it's no longer wanted.
 *
 * maxKBytes: how much data to store in memory (any data beyond this
 * amount is paged to disk).  When in doubt, use work_mem.
 */
Tuplestorestate *
tuplestore_begin_heap(bool randomAccess, bool interXact, int maxKBytes)
{
















}

/*
 * tuplestore_set_eflags
 *
 * Set the capability flags for read pointer 0 at a finer grain than is
 * allowed by tuplestore_begin_xxx.  This must be called before inserting
 * any data into the tuplestore.
 *
 * eflags is a bitmask following the meanings used for executor node
 * startup flags (see executor.h).  tuplestore pays attention to these bits:
 *		EXEC_FLAG_REWIND		need rewind to start
 *		EXEC_FLAG_BACKWARD		need backward fetch
 * If tuplestore_set_eflags is not called, REWIND is allowed, and BACKWARD
 * is set per "randomAccess" in the tuplestore_begin_xxx call.
 *
 * NOTE: setting BACKWARD without REWIND means the pointer can read backwards,
 * but not further than the truncation point (the furthest-back read pointer
 * position at the time of the last tuplestore_trim call).
 */
void
tuplestore_set_eflags(Tuplestorestate *state, int eflags)
{













}

/*
 * tuplestore_alloc_read_pointer - allocate another read pointer.
 *
 * Returns the pointer's index.
 *
 * The new pointer initially copies the position of read pointer 0.
 * It can have its own eflags, but if any data has been inserted into
 * the tuplestore, these eflags must not represent an increase in
 * requirements.
 */
int
tuplestore_alloc_read_pointer(Tuplestorestate *state, int eflags)
{

























}

/*
 * tuplestore_clear
 *
 *	Delete all the contents of a tuplestore, and reset its read pointers
 *	to the start.
 */
void
tuplestore_clear(Tuplestorestate *state)
{



























}

/*
 * tuplestore_end
 *
 *	Release resources and clean up.
 */
void
tuplestore_end(Tuplestorestate *state)
{
















}

/*
 * tuplestore_select_read_pointer - make the specified read pointer active
 */
void
tuplestore_select_read_pointer(Tuplestorestate *state, int ptr)
{


























































}

/*
 * tuplestore_tuple_count
 *
 * Returns the number of tuples added since creation or the last
 * tuplestore_clear().
 */
int64
tuplestore_tuple_count(Tuplestorestate *state)
{

}

/*
 * tuplestore_ateof
 *
 * Returns the active read pointer's eof_reached state.
 */
bool
tuplestore_ateof(Tuplestorestate *state)
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
grow_memtuples(Tuplestorestate *state)
{

























































































































}

/*
 * Accept one tuple and append it to the tuplestore.
 *
 * Note that the input tuple is always copied; the caller need not save it.
 *
 * If the active read pointer is currently "at EOF", it remains so (the read
 * pointer implicitly advances along with the write pointer); otherwise the
 * read pointer is unchanged.  Non-active read pointers do not move, which
 * means they are certain to not be "at EOF" immediately after puttuple.
 * This curious-seeming behavior is for the convenience of nodeMaterial.c and
 * nodeCtescan.c, which would otherwise need to do extra pointer repositioning
 * steps.
 *
 * tuplestore_puttupleslot() is a convenience routine to collect data from
 * a TupleTableSlot without an extra copy operation.
 */
void
tuplestore_puttupleslot(Tuplestorestate *state, TupleTableSlot *slot)
{












}

/*
 * "Standard" case to copy from a HeapTuple.  This is actually now somewhat
 * deprecated, but not worth getting rid of in view of the number of callers.
 */
void
tuplestore_puttuple(Tuplestorestate *state, HeapTuple tuple)
{











}

/*
 * Similar to tuplestore_puttuple(), but work from values + nulls arrays.
 * This avoids an extra tuple-construction operation.
 */
void
tuplestore_putvalues(Tuplestorestate *state, TupleDesc tdesc, Datum *values, bool *isnull)
{









}

static void
tuplestore_puttuple_common(Tuplestorestate *state, void *tuple)
{



























































































































}

/*
 * Fetch the next tuple in either forward or back direction.
 * Returns NULL if no more tuples.  If should_free is set, the
 * caller must pfree the returned tuple when done with it.
 *
 * Backward scan is only allowed if randomAccess was set true or
 * EXEC_FLAG_BACKWARD was specified to tuplestore_set_eflags().
 */
static void *
tuplestore_gettuple(Tuplestorestate *state, bool forward, bool *should_free)
{
























































































































































}

/*
 * tuplestore_gettupleslot - exported function to fetch a MinimalTuple
 *
 * If successful, put tuple in slot and return true; else, clear the slot
 * and return false.
 *
 * If copy is true, the slot receives a copied tuple (allocated in current
 * memory context) that will stay valid regardless of future manipulations of
 * the tuplestore's state.  If copy is false, the slot may just receive a
 * pointer to a tuple held within the tuplestore.  The latter is more
 * efficient but the slot contents may be corrupted if additional writes to
 * the tuplestore occur.  (If using tuplestore_trim, see comments therein.)
 */
bool
tuplestore_gettupleslot(Tuplestorestate *state, bool forward, bool copy, TupleTableSlot *slot)
{




















}

/*
 * tuplestore_advance - exported function to adjust position without fetching
 *
 * We could optimize this case to avoid palloc/pfree overhead, but for the
 * moment it doesn't seem worthwhile.
 */
bool
tuplestore_advance(Tuplestorestate *state, bool forward)
{

















}

/*
 * Advance over N tuples in either forward or back direction,
 * without returning any data.  N<=0 is a no-op.
 * Returns true if successful, false if ran out of tuples.
 */
bool
tuplestore_skiptuples(Tuplestorestate *state, int64 ntuples, bool forward)
{



































































}

/*
 * dumptuples - remove tuples from memory and write to tape
 *
 * As a side effect, we must convert each read pointer's position from
 * "current" to file/offset format.  But eof_reached pointers don't
 * need to change state.
 */
static void
dumptuples(Tuplestorestate *state)
{






















}

/*
 * tuplestore_rescan		- rewind the active read pointer to start
 */
void
tuplestore_rescan(Tuplestorestate *state)
{



























}

/*
 * tuplestore_copy_read_pointer - copy a read pointer's state to another
 */
void
tuplestore_copy_read_pointer(Tuplestorestate *state, int srcptr, int destptr)
{











































































}

/*
 * tuplestore_trim	- remove all no-longer-needed tuples
 *
 * Calling this function authorizes the tuplestore to delete all tuples
 * before the oldest read pointer, if no read pointer is marked as requiring
 * REWIND capability.
 *
 * Note: this is obviously safe if no pointer has BACKWARD capability either.
 * If a pointer is marked as BACKWARD but not REWIND capable, it means that
 * the pointer can be moved backward but not before the oldest other read
 * pointer.
 */
void
tuplestore_trim(Tuplestorestate *state)
{


































































































}

/*
 * tuplestore_in_memory
 *
 * Returns true if the tuplestore has not spilled to disk.
 *
 * XXX exposing this is a violation of modularity ... should get rid of it.
 */
bool
tuplestore_in_memory(Tuplestorestate *state)
{

}

/*
 * Tape interface routines
 */

static unsigned int
getlen(Tuplestorestate *state, bool eofOK)
{













}

/*
 * Routines specialized for HeapTuple case
 *
 * The stored form is actually a MinimalTuple, but for largely historical
 * reasons we allow COPYTUP to work from a HeapTuple.
 *
 * Since MinimalTuple already has length in its first word, we don't need
 * to write that separately.
 */

static void *
copytup_heap(Tuplestorestate *state, void *tup)
{





}

static void
writetup_heap(Tuplestorestate *state, void *tup)
{


















}

static void *
readtup_heap(Tuplestorestate *state, unsigned int len)
{























}