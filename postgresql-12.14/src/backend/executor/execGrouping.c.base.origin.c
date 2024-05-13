/*-------------------------------------------------------------------------
 *
 * execGrouping.c
 *	  executor utility routines for grouping, hashing, and aggregation
 *
 * Note: we currently assume that equality and hashing functions are not
 * collation-sensitive, so the code in this file has no support for passing
 * collation settings through from callers.  That may have to change someday.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/executor/execGrouping.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/parallel.h"
#include "executor/executor.h"
#include "miscadmin.h"
#include "utils/lsyscache.h"
#include "utils/hashutils.h"
#include "utils/memutils.h"

static uint32
TupleHashTableHash(struct tuplehash_hash *tb, const MinimalTuple tuple);
static int
TupleHashTableMatch(struct tuplehash_hash *tb, const MinimalTuple tuple1, const MinimalTuple tuple2);

/*
 * Define parameters for tuple hash table code generation. The interface is
 * *also* declared in execnodes.h (to generate the types, which are externally
 * visible).
 */
#define SH_PREFIX tuplehash
#define SH_ELEMENT_TYPE TupleHashEntryData
#define SH_KEY_TYPE MinimalTuple
#define SH_KEY firstTuple
#define SH_HASH_KEY(tb, key) TupleHashTableHash(tb, key)
#define SH_EQUAL(tb, a, b) TupleHashTableMatch(tb, a, b) == 0
#define SH_SCOPE extern
#define SH_STORE_HASH
#define SH_GET_HASH(tb, a) a->hash
#define SH_DEFINE
#include "lib/simplehash.h"

/*****************************************************************************
 *		Utility routines for grouping tuples together
 *****************************************************************************/

/*
 * execTuplesMatchPrepare
 *		Build expression that can be evaluated using ExecQual(),
 *returning whether an ExprContext's inner/outer tuples are NOT DISTINCT
 */
ExprState *
execTuplesMatchPrepare(TupleDesc desc, int numCols, const AttrNumber *keyColIdx, const Oid *eqOperators, const Oid *collations, PlanState *parent)
{



















}

/*
 * execTuplesHashPrepare
 *		Look up the equality and hashing functions needed for a
 *TupleHashTable.
 *
 * This is similar to execTuplesMatchPrepare, but we also need to find the
 * hash functions associated with the equality operators.  *eqFunctions and
 * *hashFunctions receive the palloc'd result arrays.
 *
 * Note: we expect that the given operators are not cross-type comparisons.
 */
void
execTuplesHashPrepare(int numCols, const Oid *eqOperators, Oid **eqFuncOids, FmgrInfo **hashFunctions)
{






















}

/*****************************************************************************
 *		Utility routines for all-in-memory hash tables
 *
 * These routines build hash tables for grouping tuples together (eg, for
 * hash aggregation).  There is one entry for each not-distinct set of tuples
 * presented.
 *****************************************************************************/

/*
 * Construct an empty TupleHashTable
 *
 *	numCols, keyColIdx: identify the tuple fields to use as lookup key
 *	eqfunctions: equality comparison functions to use
 *	hashfunctions: datatype-specific hashing functions to use
 *	nbuckets: initial estimate of hashtable size
 *	additionalsize: size of data stored in ->additional
 *	metacxt: memory context for long-lived allocation, but not per-entry
 *data tablecxt: memory context in which to store table entries tempcxt:
 *short-lived context for evaluation hash and comparison functions
 *
 * The function arrays may be made with execTuplesHashPrepare().  Note they
 * are not cross-type functions, but expect to see the table datatype(s)
 * on both sides.
 *
 * Note that keyColIdx, eqfunctions, and hashfunctions must be allocated in
 * storage that will live as long as the hashtable does.
 */
TupleHashTable
BuildTupleHashTableExt(PlanState *parent, TupleDesc inputDesc, int numCols, AttrNumber *keyColIdx, const Oid *eqfuncoids, FmgrInfo *hashfunctions, Oid *collations, long nbuckets, Size additionalsize, MemoryContext metacxt, MemoryContext tablecxt, MemoryContext tempcxt, bool use_variable_hash_iv)
{












































































}

/*
 * BuildTupleHashTable is a backwards-compatibilty wrapper for
 * BuildTupleHashTableExt(), that allocates the hashtable's metadata in
 * tablecxt. Note that hashtables created this way cannot be reset leak-free
 * with ResetTupleHashTable().
 */
TupleHashTable
BuildTupleHashTable(PlanState *parent, TupleDesc inputDesc, int numCols, AttrNumber *keyColIdx, const Oid *eqfuncoids, FmgrInfo *hashfunctions, Oid *collations, long nbuckets, Size additionalsize, MemoryContext tablecxt, MemoryContext tempcxt, bool use_variable_hash_iv)
{

}

/*
 * Reset contents of the hashtable to be empty, preserving all the non-content
 * state. Note that the tablecxt passed to BuildTupleHashTableExt() should
 * also be reset, otherwise there will be leaks.
 */
void
ResetTupleHashTable(TupleHashTable hashtable)
{

}

/*
 * Find or create a hashtable entry for the tuple group containing the
 * given tuple.  The tuple must be the same type as the hashtable entries.
 *
 * If isnew is NULL, we do not create new entries; we return NULL if no
 * match is found.
 *
 * If isnew isn't NULL, then a new entry is created if no existing entry
 * matches.  On return, *isnew is true if the entry is newly created,
 * false if it existed already.  ->additional_data in the new entry has
 * been zeroed.
 */
TupleHashEntry
LookupTupleHashEntry(TupleHashTable hashtable, TupleTableSlot *slot, bool *isnew)
{











































}

/*
 * Search for a hashtable entry matching the given tuple.  No entry is
 * created if there's not a match.  This is similar to the non-creating
 * case of LookupTupleHashEntry, except that it supports cross-type
 * comparisons, in which the given tuple is not of the same type as the
 * table entries.  The caller must provide the hash functions to use for
 * the input tuple, as well as the equality functions, since these may be
 * different from the table's internal functions.
 */
TupleHashEntry
FindTupleHashEntry(TupleHashTable hashtable, TupleTableSlot *slot, ExprState *eqcomp, FmgrInfo *hashfunctions)
{


















}

/*
 * Compute the hash value for a tuple
 *
 * The passed-in key is a pointer to TupleHashEntryData.  In an actual hash
 * table entry, the firstTuple field points to a tuple (in MinimalTuple
 * format).  LookupTupleHashEntry sets up a dummy TupleHashEntryData with a
 * NULL firstTuple field --- that cues us to look at the inputslot instead.
 * This convention avoids the need to materialize virtual input tuples unless
 * they actually need to get copied into the table.
 *
 * Also, the caller must select an appropriate memory context for running
 * the hash functions. (dynahash.c doesn't change CurrentMemoryContext.)
 */
static uint32
TupleHashTableHash(struct tuplehash_hash *tb, const MinimalTuple tuple)
{






















































}

/*
 * See whether two tuples (presumably of the same hash value) match
 *
 * As above, the passed pointers are pointers to TupleHashEntryData.
 */
static int
TupleHashTableMatch(struct tuplehash_hash *tb, const MinimalTuple tuple1, const MinimalTuple tuple2)
{





















}