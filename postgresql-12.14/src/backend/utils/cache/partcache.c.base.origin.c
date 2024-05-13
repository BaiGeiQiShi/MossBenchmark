/*-------------------------------------------------------------------------
 *
 * partcache.c
 *		Support routines for manipulating partition information cached
 *in relcache
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		  src/backend/utils/cache/partcache.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/hash.h"
#include "access/htup_details.h"
#include "access/nbtree.h"
#include "access/relation.h"
#include "catalog/partition.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_partitioned_table.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "optimizer/optimizer.h"
#include "partitioning/partbounds.h"
#include "rewrite/rewriteHandler.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static List *
generate_partition_qual(Relation rel);

/*
 * RelationBuildPartitionKey
 *		Build partition key data of relation, and attach to relcache
 *
 * Partitioning key data is a complex structure; to avoid complicated logic to
 * free individual elements whenever the relcache entry is flushed, we give it
 * its own memory context, a child of CacheMemoryContext, which can easily be
 * deleted on its own.  To avoid leaking memory in that context in case of an
 * error partway through this function, the context is initially created as a
 * child of CurTransactionContext and only re-parented to CacheMemoryContext
 * at the end, when no further errors are possible.  Also, we don't make this
 * context the current context except in very brief code sections, out of fear
 * that some of our callees allocate memory on their own which would be leaked
 * permanently.
 */
void
RelationBuildPartitionKey(Relation relation)
{












































































































































































}

/*
 * RelationGetPartitionQual
 *
 * Returns a list of partition quals
 */
List *
RelationGetPartitionQual(Relation rel)
{







}

/*
 * get_partition_qual_relid
 *
 * Returns an expression tree describing the passed-in relation's partition
 * constraint.
 *
 * If the relation is not found, or is not a partition, or there is no
 * partition constraint, return NULL.  We must guard against the first two
 * cases because this supports a SQL function that could be passed any OID.
 * The last case can happen even if relispartition is true, when a default
 * partition is the only partition.
 */
Expr *
get_partition_qual_relid(Oid relid)
{





























}

/*
 * generate_partition_qual
 *
 * Generate partition predicate from rel's partition bound expression. The
 * function returns a NIL list if there is no predicate.
 *
 * We cache a copy of the result in the relcache entry, after constructing
 * it using the caller's context.  This approach avoids leaking any data
 * into long-lived cache contexts, especially if we fail partway through.
 */
static List *
generate_partition_qual(Relation rel)
{





























































































}