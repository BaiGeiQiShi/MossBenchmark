/*-------------------------------------------------------------------------
 *
 * partition.c
 *		  Partitioning related data structures and functions.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *		  src/backend/catalog/partition.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/tupconvert.h"
#include "access/sysattr.h"
#include "catalog/indexing.h"
#include "catalog/partition.h"
#include "catalog/pg_inherits.h"
#include "catalog/pg_partitioned_table.h"
#include "nodes/makefuncs.h"
#include "optimizer/optimizer.h"
#include "partitioning/partbounds.h"
#include "rewrite/rewriteManip.h"
#include "utils/fmgroids.h"
#include "utils/partcache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static Oid
get_partition_parent_worker(Relation inhRel, Oid relid);
static void
get_partition_ancestors_worker(Relation inhRel, Oid relid, List **ancestors);

/*
 * get_partition_parent
 *		Obtain direct parent of given relation
 *
 * Returns inheritance parent of a partition by scanning pg_inherits
 *
 * Note: Because this function assumes that the relation whose OID is passed
 * as an argument will have precisely one parent, it should only be called
 * when it is known that the relation is a partition.
 */
Oid
get_partition_parent(Oid relid)
{















}

/*
 * get_partition_parent_worker
 *		Scan the pg_inherits relation to return the OID of the parent of
 *the given relation
 */
static Oid
get_partition_parent_worker(Relation inhRel, Oid relid)
{




















}

/*
 * get_partition_ancestors
 *		Obtain ancestors of given relation
 *
 * Returns a list of ancestors of the given relation.
 *
 * Note: Because this function assumes that the relation whose OID is passed
 * as an argument and each ancestor will have precisely one parent, it should
 * only be called when it is known that the relation is a partition.
 */
List *
get_partition_ancestors(Oid relid)
{










}

/*
 * get_partition_ancestors_worker
 *		recursive worker for get_partition_ancestors
 */
static void
get_partition_ancestors_worker(Relation inhRel, Oid relid, List **ancestors)
{











}

/*
 * index_get_partition
 *		Return the OID of index of the given partition that is a child
 *		of the given index, or InvalidOid if there isn't one.
 */
Oid
index_get_partition(Relation partition, Oid indexId)
{































}

/*
 * map_partition_varattnos - maps varattno of any Vars in expr from the
 * attno's of 'from_rel' to the attno's of 'to_rel' partition, each of which
 * may be either a leaf partition or a partitioned table, but both of which
 * must be from the same partitioning hierarchy.
 *
 * Even though all of the same column names must be present in all relations
 * in the hierarchy, and they must also have the same types, the attnos may
 * be different.
 *
 * If found_whole_row is not NULL, *found_whole_row returns whether a
 * whole-row variable was found in the input expression.
 *
 * Note: this will work on any node tree, so really the argument and result
 * should be declared "Node *".  But a substantial majority of the callers
 * are working on Lists, so it's less messy to do the casts internally.
 */
List *
map_partition_varattnos(List *expr, int fromrel_varno, Relation to_rel, Relation from_rel, bool *found_whole_row)
{
















}

/*
 * Checks if any of the 'attnums' is a partition key attribute for rel
 *
 * Sets *used_in_expr if any of the 'attnums' is found to be referenced in some
 * partition key expression.  It's possible for a column to be both used
 * directly and as part of an expression; if that happens, *used_in_expr may
 * end up as either true or false.  That's OK for current uses of this
 * function, because *used_in_expr is only used to tailor the error message
 * text.
 */
bool
has_partition_attrs(Relation rel, Bitmapset *attnums, bool *used_in_expr)
{





















































}

/*
 * get_default_partition_oid
 *
 * Given a relation OID, return the OID of the default partition, if one
 * exists.  Use get_default_oid_from_partdesc where possible, for
 * efficiency.
 */
Oid
get_default_partition_oid(Oid parentId)
{















}

/*
 * update_default_partition_oid
 *
 * Update pg_partitioned_table.partdefid with a new default partition OID.
 */
void
update_default_partition_oid(Oid parentId, Oid defaultPartId)
{



















}

/*
 * get_proposed_default_constraint
 *
 * This function returns the negation of new_part_constraints, which
 * would be an integral part of the default partition constraints after
 * addition of the partition to which the new_part_constraints belongs.
 */
List *
get_proposed_default_constraint(List *new_part_constraints)
{
















}