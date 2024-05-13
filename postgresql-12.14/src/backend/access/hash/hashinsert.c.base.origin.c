/*-------------------------------------------------------------------------
 *
 * hashinsert.c
 *	  Item insertion in hash tables for Postgres.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/hash/hashinsert.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/hash.h"
#include "access/hash_xlog.h"
#include "miscadmin.h"
#include "utils/rel.h"
#include "storage/lwlock.h"
#include "storage/buf_internals.h"
#include "storage/predicate.h"

static void
_hash_vacuum_one_page(Relation rel, Relation hrel, Buffer metabuf, Buffer buf);

/*
 *	_hash_doinsert() -- Handle insertion of a single index tuple.
 *
 *		This routine is called by the public interface routines,
 *hashbuild and hashinsert.  By here, itup is completely filled in.
 */
void
_hash_doinsert(Relation rel, IndexTuple itup, Relation heapRel)
{



























































































































































































































}

/*
 *	_hash_pgaddtup() -- add a tuple to a particular page in the index.
 *
 * This routine adds the tuple to the page as requested; it does not write out
 * the page.  It is an error to call pgaddtup() without pin and write lock on
 * the target buffer.
 *
 * Returns the offset number at which the tuple was inserted.  This function
 * is responsible for preserving the condition that tuples in a hash index
 * page are sorted by hashkey value.
 */
OffsetNumber
_hash_pgaddtup(Relation rel, Buffer buf, Size itemsize, IndexTuple itup)
{

















}

/*
 *	_hash_pgaddmultitup() -- add a tuple vector to a particular page in the
 *							 index.
 *
 * This routine has same requirements for locking and tuple ordering as
 * _hash_pgaddtup().
 *
 * Returns the offset number array at which the tuples were inserted.
 */
void
_hash_pgaddmultitup(Relation rel, Buffer buf, IndexTuple *itups, OffsetNumber *itup_offsets, uint16 nitups)
{


























}

/*
 * _hash_vacuum_one_page - vacuum just one index page.
 *
 * Try to remove LP_DEAD items from the given page. We must acquire cleanup
 * lock on the page being modified before calling this function.
 */

static void
_hash_vacuum_one_page(Relation rel, Relation hrel, Buffer metabuf, Buffer buf)
{
























































































}