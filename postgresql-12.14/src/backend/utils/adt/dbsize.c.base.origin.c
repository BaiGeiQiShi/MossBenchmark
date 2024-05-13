/*
 * dbsize.c
 *		Database object size functions, and related inquiries
 *
 * Copyright (c) 2002-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/dbsize.c
 *
 */

#include "postgres.h"

#include <sys/stat.h>

#include "access/htup_details.h"
#include "access/relation.h"
#include "catalog/catalog.h"
#include "catalog/namespace.h"
#include "catalog/pg_authid.h"
#include "catalog/pg_tablespace.h"
#include "commands/dbcommands.h"
#include "commands/tablespace.h"
#include "miscadmin.h"
#include "storage/fd.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/numeric.h"
#include "utils/rel.h"
#include "utils/relfilenodemap.h"
#include "utils/relmapper.h"
#include "utils/syscache.h"

/* Divide by two and round away from zero */
#define half_rounded(x) (((x) + ((x) < 0 ? -1 : 1)) / 2)

/* Return physical size of directory contents, or 0 if dir doesn't exist */
static int64
db_dir_size(const char *path)
{









































}

/*
 * calculate size of database in all tablespaces
 */
static int64
calculate_database_size(Oid dbOid)
{











































}

Datum
pg_database_size_oid(PG_FUNCTION_ARGS)
{











}

Datum
pg_database_size_name(PG_FUNCTION_ARGS)
{












}

/*
 * Calculate total size of tablespace. Returns -1 if the tablespace directory
 * cannot be found.
 */
static int64
calculate_tablespace_size(Oid tblspcOid)
{













































































}

Datum
pg_tablespace_size_oid(PG_FUNCTION_ARGS)
{











}

Datum
pg_tablespace_size_name(PG_FUNCTION_ARGS)
{












}

/*
 * calculate size of (one fork of) a relation
 *
 * Note: we can safely apply this to temp tables of other sessions, so there
 * is no check here or at the call sites for that.
 */
static int64
calculate_relation_size(RelFileNode *rfn, BackendId backend, ForkNumber forknum)
{





































}

Datum
pg_relation_size(PG_FUNCTION_ARGS)
{
























}

/*
 * Calculate total on-disk size of a TOAST relation, including its indexes.
 * Must not be applied to non-TOAST relations.
 */
static int64
calculate_toast_table_size(Oid toastrelid)
{


































}

/*
 * Calculate total on-disk size of a given table,
 * including FSM and VM, plus TOAST table if any.
 * Indexes other than the TOAST table's index are not included.
 *
 * Note that this also behaves sanely if applied to an index or toast table;
 * those won't have attached toast tables, but they can have multiple forks.
 */
static int64
calculate_table_size(Relation rel)
{




















}

/*
 * Calculate total on-disk size of all indexes attached to the given table.
 *
 * Can be applied safely to an index, but you'll just get zero.
 */
static int64
calculate_indexes_size(Relation rel)
{






























}

Datum
pg_table_size(PG_FUNCTION_ARGS)
{
















}

Datum
pg_indexes_size(PG_FUNCTION_ARGS)
{
















}

/*
 *	Compute the on-disk size of all files for the relation,
 *	including heap data, index data, toast data, FSM, VM.
 */
static int64
calculate_total_relation_size(Relation rel)
{














}

Datum
pg_total_relation_size(PG_FUNCTION_ARGS)
{
















}

/*
 * formatting with size units
 */
Datum
pg_size_pretty(PG_FUNCTION_ARGS)
{












































}

static char *
numeric_to_cstring(Numeric n)
{



}

static Numeric
int64_to_numeric(int64 v)
{



}

static bool
numeric_is_less(Numeric a, Numeric b)
{




}

static Numeric
numeric_absolute(Numeric n)
{





}

static Numeric
numeric_half_rounded(Numeric n)
{





















}

static Numeric
numeric_truncated_divide(Numeric n, int64 divisor)
{







}

Datum
pg_size_pretty_numeric(PG_FUNCTION_ARGS)
{






















































}

/*
 * Convert a human-readable size to a size in bytes
 */
Datum
pg_size_bytes(PG_FUNCTION_ARGS)
{





















































































































































}

/*
 * Get the filenode of a relation
 *
 * This is expected to be used in queries like
 *		SELECT pg_relation_filenode(oid) FROM pg_class;
 * That leads to a couple of choices.  We work from the pg_class row alone
 * rather than actually opening each relation, for efficiency.  We don't
 * fail if we can't find the relation --- some rows might be visible in
 * the query's MVCC snapshot even though the relations have been dropped.
 * (Note: we could avoid using the catcache, but there's little point
 * because the relation mapper also works "in the now".)  We also don't
 * fail if the relation doesn't have storage.  In all these cases it
 * seems better to quietly return NULL.
 */
Datum
pg_relation_filenode(PG_FUNCTION_ARGS)
{












































}

/*
 * Get the relation via (reltablespace, relfilenode)
 *
 * This is expected to be used when somebody wants to match an individual file
 * on the filesystem back to its table. That's not trivially possible via
 * pg_class, because that doesn't contain the relfilenodes of shared and nailed
 * tables.
 *
 * We don't fail but return NULL if we cannot find a mapping.
 *
 * InvalidOid can be passed instead of the current database's default
 * tablespace.
 */
Datum
pg_filenode_relation(PG_FUNCTION_ARGS)
{




















}

/*
 * Get the pathname (relative to $PGDATA) of a relation
 *
 * See comments for pg_relation_filenode.
 */
Datum
pg_relation_filepath(PG_FUNCTION_ARGS)
{































































































}