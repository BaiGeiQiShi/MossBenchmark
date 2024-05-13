/*-------------------------------------------------------------------------
 *
 * sharedtuplestore.c
 *	  Simple mechanism for sharing tuples between backends.
 *
 * This module contains a shared temporary tuple storage mechanism providing
 * a parallel-aware subset of the features of tuplestore.c.  Multiple backends
 * can write to a SharedTuplestore, and then multiple backends can later scan
 * the stored tuples.  Currently, the only scan type supported is a parallel
 * scan where each backend reads an arbitrary subset of the tuples that were
 * written.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/sort/sharedtuplestore.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup.h"
#include "access/htup_details.h"
#include "miscadmin.h"
#include "storage/buffile.h"
#include "storage/lwlock.h"
#include "storage/sharedfileset.h"
#include "utils/sharedtuplestore.h"

#include <limits.h>

/*
 * The size of chunks, in pages.  This is somewhat arbitrarily set to match
 * the size of HASH_CHUNK, so that Parallel Hash obtains new chunks of tuples
 * at approximately the same rate as it allocates new chunks of memory to
 * insert them into.
 */
#define STS_CHUNK_PAGES 4
#define STS_CHUNK_HEADER_SIZE offsetof(SharedTuplestoreChunk, data)
#define STS_CHUNK_DATA_SIZE (STS_CHUNK_PAGES * BLCKSZ - STS_CHUNK_HEADER_SIZE)

/* Chunk written to disk. */
typedef struct SharedTuplestoreChunk
{
  int ntuples;  /* Number of tuples in this chunk. */
  int overflow; /* If overflow, how many including this one? */
  char data[FLEXIBLE_ARRAY_MEMBER];
} SharedTuplestoreChunk;

/* Per-participant shared state. */
typedef struct SharedTuplestoreParticipant
{
  LWLock lock;
  BlockNumber read_page; /* Page number for next read. */
  BlockNumber npages;    /* Number of pages written. */
  bool writing;          /* Used only for assertions. */
} SharedTuplestoreParticipant;

/* The control object that lives in shared memory. */
struct SharedTuplestore
{
  int nparticipants;      /* Number of participants that can write. */
  int flags;              /* Flag bits from SHARED_TUPLESTORE_XXX */
  size_t meta_data_size;  /* Size of per-tuple header. */
  char name[NAMEDATALEN]; /* A name for this tuplestore. */

  /* Followed by per-participant shared state. */
  SharedTuplestoreParticipant participants[FLEXIBLE_ARRAY_MEMBER];
};

/* Per-participant state that lives in backend-local memory. */
struct SharedTuplestoreAccessor
{
  int participant;        /* My participant number. */
  SharedTuplestore *sts;  /* The shared state. */
  SharedFileSet *fileset; /* The SharedFileSet holding files. */
  MemoryContext context;  /* Memory context for buffers. */

  /* State for reading. */
  int read_participant;       /* The current participant to read from. */
  BufFile *read_file;         /* The current file to read from. */
  int read_ntuples_available; /* The number of tuples in chunk. */
  int read_ntuples;           /* How many tuples have we read from chunk? */
  size_t read_bytes;          /* How many bytes have we read from chunk? */
  char *read_buffer;          /* A buffer for loading tuples. */
  size_t read_buffer_size;
  BlockNumber read_next_page; /* Lowest block we'll consider reading. */

  /* State for writing. */
  SharedTuplestoreChunk *write_chunk; /* Buffer for writing. */
  BufFile *write_file;                /* The current file to write to. */
  BlockNumber write_page;             /* The next page to write to. */
  char *write_pointer;                /* Current write pointer within chunk. */
  char *write_end;                    /* One past the end of the current chunk. */
};

static void
sts_filename(char *name, SharedTuplestoreAccessor *accessor, int participant);

/*
 * Return the amount of shared memory required to hold SharedTuplestore for a
 * given number of participants.
 */
size_t
sts_estimate(int participants)
{

}

/*
 * Initialize a SharedTuplestore in existing shared memory.  There must be
 * space for sts_estimate(participants) bytes.  If flags includes the value
 * SHARED_TUPLESTORE_SINGLE_PASS, the files may in future be removed more
 * eagerly (but this isn't yet implemented).
 *
 * Tuples that are stored may optionally carry a piece of fixed sized
 * meta-data which will be retrieved along with the tuple.  This is useful for
 * the hash values used in multi-batch hash joins, but could have other
 * applications.
 *
 * The caller must supply a SharedFileSet, which is essentially a directory
 * that will be cleaned up automatically, and a name which must be unique
 * across all SharedTuplestores created in the same SharedFileSet.
 */
SharedTuplestoreAccessor *
sts_initialize(SharedTuplestore *sts, int participants, int my_participant_number, size_t meta_data_size, int flags, SharedFileSet *fileset, const char *name)
{









































}

/*
 * Attach to a SharedTuplestore that has been initialized by another backend,
 * so that this backend can read and write tuples.
 */
SharedTuplestoreAccessor *
sts_attach(SharedTuplestore *sts, int my_participant_number, SharedFileSet *fileset)
{











}

static void
sts_flush_chunk(SharedTuplestoreAccessor *accessor)
{







}

/*
 * Finish writing tuples.  This must be called by all backends that have
 * written data before any backend begins reading it.
 */
void
sts_end_write(SharedTuplestoreAccessor *accessor)
{









}

/*
 * Prepare to rescan.  Only one participant must call this.  After it returns,
 * all participants may call sts_begin_parallel_scan() and then loop over
 * sts_parallel_scan_next().  This function must not be called concurrently
 * with a scan, and synchronization to avoid that is the caller's
 * responsibility.
 */
void
sts_reinitialize(SharedTuplestoreAccessor *accessor)
{











}

/*
 * Begin scanning the contents in parallel.
 */
void
sts_begin_parallel_scan(SharedTuplestoreAccessor *accessor)
{






















}

/*
 * Finish a parallel scan, freeing associated backend-local resources.
 */
void
sts_end_parallel_scan(SharedTuplestoreAccessor *accessor)
{










}

/*
 * Write a tuple.  If a meta-data size was provided to sts_initialize, then a
 * pointer to meta data of that size must be provided.
 */
void
sts_puttuple(SharedTuplestoreAccessor *accessor, void *meta_data, MinimalTuple tuple)
{
































































































}

static MinimalTuple
sts_read_tuple(SharedTuplestoreAccessor *accessor, void *meta_data)
{




















































































}

/*
 * Get the next tuple in the current parallel scan.
 */
MinimalTuple
sts_parallel_scan_next(SharedTuplestoreAccessor *accessor, void *meta_data)
{

































































































}

/*
 * Create the name used for the BufFile that a given participant will write.
 */
static void
sts_filename(char *name, SharedTuplestoreAccessor *accessor, int participant)
{

}