/*-------------------------------------------------------------------------
 *
 * amutils.c
 *	  SQL-level APIs related to index access methods.
 *
 * Copyright (c) 2016-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/amutils.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/amapi.h"
#include "access/htup_details.h"
#include "catalog/pg_class.h"
#include "catalog/pg_index.h"
#include "utils/builtins.h"
#include "utils/syscache.h"

/* Convert string property name to enum, for efficiency */
struct am_propname
{
  const char *name;
  IndexAMProperty prop;
};

static const struct am_propname am_propnames[] = {
    {"asc", AMPROP_ASC},
    {"desc", AMPROP_DESC},
    {"nulls_first", AMPROP_NULLS_FIRST},
    {"nulls_last", AMPROP_NULLS_LAST},
    {"orderable", AMPROP_ORDERABLE},
    {"distance_orderable", AMPROP_DISTANCE_ORDERABLE},
    {"returnable", AMPROP_RETURNABLE},
    {"search_array", AMPROP_SEARCH_ARRAY},
    {"search_nulls", AMPROP_SEARCH_NULLS},
    {"clusterable", AMPROP_CLUSTERABLE},
    {"index_scan", AMPROP_INDEX_SCAN},
    {"bitmap_scan", AMPROP_BITMAP_SCAN},
    {"backward_scan", AMPROP_BACKWARD_SCAN},
    {"can_order", AMPROP_CAN_ORDER},
    {"can_unique", AMPROP_CAN_UNIQUE},
    {"can_multi_col", AMPROP_CAN_MULTI_COL},
    {"can_exclude", AMPROP_CAN_EXCLUDE},
    {"can_include", AMPROP_CAN_INCLUDE},
};

static IndexAMProperty
lookup_prop_name(const char *name)
{












}

/*
 * Common code for properties that are just bit tests of indoptions.
 *
 * tuple: the pg_index heaptuple
 * attno: identify the index column to test the indoptions of.
 * guard: if false, a boolean false result is forced (saves code in caller).
 * iopt_mask: mask for interesting indoption bit.
 * iopt_expect: value for a "true" result (should be 0 or iopt_mask).
 *
 * Returns false to indicate a NULL result (for "unknown/inapplicable"),
 * otherwise sets *res to the boolean value to return.
 */
static bool
test_indoption(HeapTuple tuple, int attno, bool guard, int16 iopt_mask, int16 iopt_expect, bool *res)
{




















}

/*
 * Test property of an index AM, index, or index column.
 *
 * This is common code for different SQL-level funcs, so the amoid and
 * index_oid parameters are mutually exclusive; we look up the amoid from the
 * index_oid if needed, or if no index oid is given, we're looking at AM-wide
 * properties.
 */
static Datum
indexam_property(FunctionCallInfo fcinfo, const char *propname, Oid amoid, Oid index_oid, int attno)
{


































































































































































































































































}

/*
 * Test property of an AM specified by AM OID
 */
Datum
pg_indexam_has_property(PG_FUNCTION_ARGS)
{




}

/*
 * Test property of an index specified by index OID
 */
Datum
pg_index_has_property(PG_FUNCTION_ARGS)
{




}

/*
 * Test property of an index column specified by index OID and column number
 */
Datum
pg_index_column_has_property(PG_FUNCTION_ARGS)
{











}

/*
 * Return the name of the given phase, as used for progress reporting by the
 * given AM.
 */
Datum
pg_indexam_progress_phasename(PG_FUNCTION_ARGS)
{


















}