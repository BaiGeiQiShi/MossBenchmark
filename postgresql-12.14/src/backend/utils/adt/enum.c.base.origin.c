/*-------------------------------------------------------------------------
 *
 * enum.c
 *	  I/O functions, operators, aggregates etc for enum types
 *
 * Copyright (c) 2006-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/enum.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/indexing.h"
#include "catalog/pg_enum.h"
#include "libpq/pqformat.h"
#include "storage/procarray.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"
#include "utils/typcache.h"

static Oid
enum_endpoint(Oid enumtypoid, ScanDirection direction);
static ArrayType *
enum_range_internal(Oid enumtypoid, Oid lower, Oid upper);

/*
 * Disallow use of an uncommitted pg_enum tuple.
 *
 * We need to make sure that uncommitted enum values don't get into indexes.
 * If they did, and if we then rolled back the pg_enum addition, we'd have
 * broken the index because value comparisons will not work reliably without
 * an underlying pg_enum entry.  (Note that removal of the heap entry
 * containing an enum value is not sufficient to ensure that it doesn't appear
 * in upper levels of indexes.)  To do this we prevent an uncommitted row from
 * being used for any SQL-level purpose.  This is stronger than necessary,
 * since the value might not be getting inserted into a table or there might
 * be no index on its column, but it's easy to enforce centrally.
 *
 * However, it's okay to allow use of uncommitted values belonging to enum
 * types that were themselves created in the same transaction, because then
 * any such index would also be new and would go away altogether on rollback.
 * We don't implement that fully right now, but we do allow free use of enum
 * values created during CREATE TYPE AS ENUM, which are surely of the same
 * lifespan as the enum type.  (This case is required by "pg_restore -1".)
 * Values added by ALTER TYPE ADD VALUE are currently restricted, but could
 * be allowed if the enum type could be proven to have been created earlier
 * in the same transaction.  (Note that comparing tuple xmins would not work
 * for that, because the type tuple might have been updated in the current
 * transaction.  Subtransactions also create hazards to be accounted for.)
 *
 * This function needs to be called (directly or indirectly) in any of the
 * functions below that could return an enum value to SQL operations.
 */
static void
check_safe_enum_use(HeapTuple enumval_tup)
{






































}

/* Basic I/O support */

Datum
enum_in(PG_FUNCTION_ARGS)
{





























}

Datum
enum_out(PG_FUNCTION_ARGS)
{

















}

/* Binary I/O support */
Datum
enum_recv(PG_FUNCTION_ARGS)
{































}

Datum
enum_send(PG_FUNCTION_ARGS)
{


















}

/* Comparison functions and related */

/*
 * enum_cmp_internal is the common engine for all the visible comparison
 * functions, except for enum_eq and enum_ne which can just check for OID
 * equality directly.
 */
static int
enum_cmp_internal(Oid arg1, Oid arg2, FunctionCallInfo fcinfo)
{






















































}

Datum
enum_lt(PG_FUNCTION_ARGS)
{




}

Datum
enum_le(PG_FUNCTION_ARGS)
{




}

Datum
enum_eq(PG_FUNCTION_ARGS)
{




}

Datum
enum_ne(PG_FUNCTION_ARGS)
{




}

Datum
enum_ge(PG_FUNCTION_ARGS)
{




}

Datum
enum_gt(PG_FUNCTION_ARGS)
{




}

Datum
enum_smaller(PG_FUNCTION_ARGS)
{




}

Datum
enum_larger(PG_FUNCTION_ARGS)
{




}

Datum
enum_cmp(PG_FUNCTION_ARGS)
{




}

/* Enum programming support functions */

/*
 * enum_endpoint: common code for enum_first/enum_last
 */
static Oid
enum_endpoint(Oid enumtypoid, ScanDirection direction)
{




































}

Datum
enum_first(PG_FUNCTION_ARGS)
{























}

Datum
enum_last(PG_FUNCTION_ARGS)
{























}

/* 2-argument variant of enum_range */
Datum
enum_range_bounds(PG_FUNCTION_ARGS)
{

































}

/* 1-argument variant of enum_range */
Datum
enum_range_all(PG_FUNCTION_ARGS)
{














}

static ArrayType *
enum_range_internal(Oid enumtypoid, Oid lower, Oid upper)
{


































































}