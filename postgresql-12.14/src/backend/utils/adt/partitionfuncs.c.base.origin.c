/*-------------------------------------------------------------------------
 *
 * partitionfuncs.c
 *	  Functions for accessing partition-related metadata
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/partitionfuncs.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/partition.h"
#include "catalog/pg_class.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "utils/fmgrprotos.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

/*
 * Checks if a given relation can be part of a partition tree.  Returns
 * false if the relation cannot be processed, in which case it is up to
 * the caller to decide what to do, by either raising an error or doing
 * something else.
 */
static bool
check_rel_can_be_partition(Oid relid)
{



















}

/*
 * pg_partition_tree
 *
 * Produce a view with one row per member of a partition tree, beginning
 * from the top-most parent given by the caller.  This gives information
 * about each partition, its immediate partitioned parent, if it is
 * a leaf partition and its level in the hierarchy.
 */
Datum
pg_partition_tree(PG_FUNCTION_ARGS)
{















































































































}

/*
 * pg_partition_root
 *
 * Returns the top-most parent of the partition tree to which a given
 * relation belongs, or NULL if it's not (or cannot be) part of any
 * partition tree.
 */
Datum
pg_partition_root(PG_FUNCTION_ARGS)
{






























}

/*
 * pg_partition_ancestors
 *
 * Produces a view with one row per ancestor of the given partition,
 * including the input relation itself.
 */
Datum
pg_partition_ancestors(PG_FUNCTION_ARGS)
{








































}