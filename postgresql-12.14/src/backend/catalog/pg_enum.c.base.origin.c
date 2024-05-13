/*-------------------------------------------------------------------------
 *
 * pg_enum.c
 *	  routines to support manipulation of the pg_enum relation
 *
 * Copyright (c) 2006-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/pg_enum.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/binary_upgrade.h"
#include "catalog/catalog.h"
#include "catalog/indexing.h"
#include "catalog/pg_enum.h"
#include "catalog/pg_type.h"
#include "storage/lmgr.h"
#include "miscadmin.h"
#include "nodes/value.h"
#include "utils/builtins.h"
#include "utils/catcache.h"
#include "utils/fmgroids.h"
#include "utils/hsearch.h"
#include "utils/memutils.h"
#include "utils/syscache.h"

/* Potentially set by pg_upgrade_support functions */
Oid binary_upgrade_next_pg_enum_oid = InvalidOid;

/*
 * Hash table of enum value OIDs created during the current transaction by
 * AddEnumLabel.  We disallow using these values until the transaction is
 * committed; otherwise, they might get into indexes where we can't clean
 * them up, and then if the transaction rolls back we have a broken index.
 * (See comments for check_safe_enum_use() in enum.c.)  Values created by
 * EnumValuesCreate are *not* blacklisted; we assume those are created during
 * CREATE TYPE, so they can't go away unless the enum type itself does.
 */
static HTAB *enum_blacklist = NULL;

static void
RenumberEnumType(Relation pg_enum, HeapTuple *existing, int nelems);
static int
sort_order_cmp(const void *p1, const void *p2);

/*
 * EnumValuesCreate
 *		Create an entry in pg_enum for each of the supplied enum values.
 *
 * vals is a list of Value strings.
 */
void
EnumValuesCreate(Oid enumTypeOid, List *vals)
{


















































































}

/*
 * EnumValuesDelete
 *		Remove all the pg_enum entries for the specified enum type.
 */
void
EnumValuesDelete(Oid enumTypeOid)
{



















}

/*
 * Initialize the enum blacklist for this transaction.
 */
static void
init_enum_blacklist(void)
{







}

/*
 * AddEnumLabel
 *		Add a new label to the enum set. By default it goes at
 *		the end, but the user can choose to place it before or
 *		after any existing set member.
 */
void
AddEnumLabel(Oid enumTypeOid, const char *newVal, const char *neighbor, bool newValIsAfter, bool skipIfExists)
{







































































































































































































































































































}

/*
 * RenameEnumLabel
 *		Rename a label in an enum set.
 */
void
RenameEnumLabel(Oid enumTypeOid, const char *oldVal, const char *newVal)
{







































































}

/*
 * Test if the given enum value is on the blacklist
 */
bool
EnumBlacklisted(Oid enum_id)
{











}

/*
 * Clean up enum stuff after end of top-level transaction.
 */
void
AtEOXact_Enum(void)
{






}

/*
 * RenumberEnumType
 *		Renumber existing enum elements to have sort positions 1..n.
 *
 * We avoid doing this unless absolutely necessary; in most installations
 * it will never happen.  The reason is that updating existing pg_enum
 * entries creates hazards for other backends that are concurrently reading
 * pg_enum.  Although system catalog scans now use MVCC semantics, the
 * syscache machinery might read different pg_enum entries under different
 * snapshots, so some other backend might get confused about the proper
 * ordering if a concurrent renumbering occurs.
 *
 * We therefore make the following choices:
 *
 * 1. Any code that is interested in the enumsortorder values MUST read
 * all the relevant pg_enum entries with a single MVCC snapshot, or else
 * acquire lock on the enum type to prevent concurrent execution of
 * AddEnumLabel().
 *
 * 2. Code that is not examining enumsortorder can use a syscache
 * (for example, enum_in and enum_out do so).
 */
static void
RenumberEnumType(Relation pg_enum, HeapTuple *existing, int nelems)
{





























}

/* qsort comparison function for tuples by sort order */
static int
sort_order_cmp(const void *p1, const void *p2)
{

















}

Size
EstimateEnumBlacklistSpace(void)
{













}

void
SerializeEnumBlacklist(void *space, Size size)
{





























}

void
RestoreEnumBlacklist(void *space)
{




















}