/*-------------------------------------------------------------------------
 *
 * partdesc.c
 *		Support routines for manipulating partition descriptors
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		  src/backend/partitioning/partdesc.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/partition.h"
#include "catalog/pg_inherits.h"
#include "partitioning/partbounds.h"
#include "partitioning/partdesc.h"
#include "storage/bufmgr.h"
#include "storage/sinval.h"
#include "utils/builtins.h"
#include "utils/inval.h"
#include "utils/fmgroids.h"
#include "utils/hsearch.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/partcache.h"
#include "utils/syscache.h"

typedef struct PartitionDirectoryData
{
  MemoryContext pdir_mcxt;
  HTAB *pdir_hash;
} PartitionDirectoryData;

typedef struct PartitionDirectoryEntry
{
  Oid reloid;
  Relation rel;
  PartitionDesc pd;
} PartitionDirectoryEntry;

/*
 * RelationBuildPartitionDesc
 *		Form rel's partition descriptor, and store in relcache entry
 *
 * Note: the descriptor won't be flushed from the cache by
 * RelationClearRelation() unless it's changed because of
 * addition or removal of a partition.  Hence, code holding a lock
 * that's sufficient to prevent that can assume that rd_partdesc
 * won't change underneath it.
 */
void
RelationBuildPartitionDesc(Relation rel)
{








































































































































































}

/*
 * CreatePartitionDirectory
 *		Create a new partition directory object.
 */
PartitionDirectory
CreatePartitionDirectory(MemoryContext mcxt)
{















}

/*
 * PartitionDirectoryLookup
 *		Look up the partition descriptor for a relation in the
 *directory.
 *
 * The purpose of this function is to ensure that we get the same
 * PartitionDesc for each relation every time we look it up.  In the
 * face of current DDL, different PartitionDescs may be constructed with
 * different views of the catalog state, but any single particular OID
 * will always get the same PartitionDesc for as long as the same
 * PartitionDirectory is used.
 */
PartitionDesc
PartitionDirectoryLookup(PartitionDirectory pdir, Relation rel)
{

















}

/*
 * DestroyPartitionDirectory
 *		Destroy a partition directory.
 *
 * Release the reference counts we're holding.
 */
void
DestroyPartitionDirectory(PartitionDirectory pdir)
{








}

/*
 * equalPartitionDescs
 *		Compare two partition descriptors for logical equality
 */
bool
equalPartitionDescs(PartitionKey key, PartitionDesc partdesc1, PartitionDesc partdesc2)
{























































}

/*
 * get_default_oid_from_partdesc
 *
 * Given a partition descriptor, return the OID of the default partition, if
 * one exists; else, return InvalidOid.
 */
Oid
get_default_oid_from_partdesc(PartitionDesc partdesc)
{






}