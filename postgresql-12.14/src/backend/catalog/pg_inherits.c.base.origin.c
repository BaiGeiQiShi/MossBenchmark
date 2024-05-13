/*-------------------------------------------------------------------------
 *
 * pg_inherits.c
 *	  routines to support manipulation of the pg_inherits relation
 *
 * Note: currently, this module mostly contains inquiry functions; actual
 * creation and deletion of pg_inherits entries is mostly done in tablecmds.c.
 * Perhaps someday that code should be moved here, but it'd have to be
 * disentangled from other stuff such as pg_depend updates.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/pg_inherits.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/pg_inherits.h"
#include "parser/parse_type.h"
#include "storage/lmgr.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/memutils.h"
#include "utils/syscache.h"

/*
 * Entry of a hash table used in find_all_inheritors. See below.
 */
typedef struct SeenRelsEntry
{
  Oid rel_id;                /* relation oid */
  ListCell *numparents_cell; /* corresponding list cell */
} SeenRelsEntry;

/*
 * find_inheritance_children
 *
 * Returns a list containing the OIDs of all relations which
 * inherit *directly* from the relation with OID 'parentrelId'.
 *
 * The specified lock type is acquired on each child relation (but not on the
 * given rel; caller should already have locked it).  If lockmode is NoLock
 * then no locks are acquired, but caller must beware of race conditions
 * against possible DROPs of child relations.
 */
List *
find_inheritance_children(Oid parentrelId, LOCKMODE lockmode)
{

























































































}

/*
 * find_all_inheritors -
 *		Returns a list of relation OIDs including the given rel plus
 *		all relations that inherit from it, directly or indirectly.
 *		Optionally, it also returns the number of parents found for
 *		each such relation within the inheritance tree rooted at the
 *		given rel.
 *
 * The specified lock type is acquired on all child relations (but not on the
 * given rel; caller should already have locked it).  If lockmode is NoLock
 * then no locks are acquired, but caller must beware of race conditions
 * against possible DROPs of child relations.
 */
List *
find_all_inheritors(Oid parentrelId, LOCKMODE lockmode, List **numparents)
{










































































}

/*
 * has_subclass - does this relation have any children?
 *
 * In the current implementation, has_subclass returns whether a
 * particular class *might* have a subclass. It will not return the
 * correct result if a class had a subclass which was later dropped.
 * This is because relhassubclass in pg_class is not updated immediately
 * when a subclass is dropped, primarily because of concurrency concerns.
 *
 * Currently has_subclass is only used as an efficiency hack to skip
 * unnecessary inheritance searches, so this is OK.  Note that ANALYZE
 * on a childless table will clean up the obsolete relhassubclass flag.
 *
 * Although this doesn't actually touch pg_inherits, it seems reasonable
 * to keep it here since it's normally used with the other routines here.
 */
bool
has_subclass(Oid relationId)
{












}

/*
 * has_superclass - does this relation inherit from another?
 *
 * Unlike has_subclass, this can be relied on to give an accurate answer.
 * However, the caller must hold a lock on the given relation so that it
 * can't be concurrently added to or removed from an inheritance hierarchy.
 */
bool
has_superclass(Oid relationId)
{













}

/*
 * Given two type OIDs, determine whether the first is a complex type
 * (class type) that inherits from the second.
 *
 * This essentially asks whether the first type is guaranteed to be coercible
 * to the second.  Therefore, we allow the first type to be a domain over a
 * complex type that inherits from the second; that creates no difficulties.
 * But the second type cannot be a domain.
 */
bool
typeInheritsFrom(Oid subclassTypeId, Oid superclassTypeId)
{



































































































}

/*
 * Create a single pg_inherits row with the given data
 */
void
StoreSingleInheritance(Oid relationId, Oid parentOid, int32 seqNumber)
{























}

/*
 * DeleteInheritsTuple
 *
 * Delete pg_inherits tuples with the given inhrelid.  inhparent may be given
 * as InvalidOid, in which case all tuples matching inhrelid are deleted;
 * otherwise only delete tuples with the specified inhparent.
 *
 * Returns whether at least one row was deleted.
 */
bool
DeleteInheritsTuple(Oid inhrelid, Oid inhparent)
{































}