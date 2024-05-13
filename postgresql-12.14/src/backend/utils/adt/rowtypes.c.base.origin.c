/*-------------------------------------------------------------------------
 *
 * rowtypes.c
 *	  I/O and comparison functions for generic composite types.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/rowtypes.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "access/htup_details.h"
#include "access/tuptoaster.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/datum.h"
#include "utils/lsyscache.h"
#include "utils/typcache.h"

/*
 * structure to cache metadata needed for record I/O
 */
typedef struct ColumnIOData
{
  Oid column_type;
  Oid typiofunc;
  Oid typioparam;
  bool typisvarlena;
  FmgrInfo proc;
} ColumnIOData;

typedef struct RecordIOData
{
  Oid record_type;
  int32 record_typmod;
  int ncolumns;
  ColumnIOData columns[FLEXIBLE_ARRAY_MEMBER];
} RecordIOData;

/*
 * structure to cache metadata needed for record comparison
 */
typedef struct ColumnCompareData
{
  TypeCacheEntry *typentry; /* has everything we need, actually */
} ColumnCompareData;

typedef struct RecordCompareData
{
  int ncolumns; /* allocated length of columns[] */
  Oid record1_type;
  int32 record1_typmod;
  Oid record2_type;
  int32 record2_typmod;
  ColumnCompareData columns[FLEXIBLE_ARRAY_MEMBER];
} RecordCompareData;

/*
 * record_in		- input routine for any composite type.
 */
Datum
record_in(PG_FUNCTION_ARGS)
{



















































































































































































































}

/*
 * record_out		- output routine for any composite type.
 */
Datum
record_out(PG_FUNCTION_ARGS)
{













































































































































}

/*
 * record_recv		- binary input routine for any composite type.
 */
Datum
record_recv(PG_FUNCTION_ARGS)
{














































































































































































}

/*
 * record_send		- binary output routine for any composite type.
 */
Datum
record_send(PG_FUNCTION_ARGS)
{
















































































































}

/*
 * record_cmp()
 * Internal comparison function for records.
 *
 * Returns -1, 0 or 1
 *
 * Do not assume that the two inputs are exactly the same record type;
 * for instance we might be comparing an anonymous ROW() construct against a
 * named composite type.  We will compare as long as they have the same number
 * of non-dropped columns of the same types.
 */
static int
record_cmp(FunctionCallInfo fcinfo)
{


























































































































































































































}

/*
 * record_eq :
 *		  compares two records for equality
 * result :
 *		  returns true if the records are equal, false otherwise.
 *
 * Note: we do not use record_cmp here, since equality may be meaningful in
 * datatypes that don't have a total ordering (and hence no btree support).
 */
Datum
record_eq(PG_FUNCTION_ARGS)
{










































































































































































































}

Datum
record_ne(PG_FUNCTION_ARGS)
{

}

Datum
record_lt(PG_FUNCTION_ARGS)
{

}

Datum
record_gt(PG_FUNCTION_ARGS)
{

}

Datum
record_le(PG_FUNCTION_ARGS)
{

}

Datum
record_ge(PG_FUNCTION_ARGS)
{

}

Datum
btrecordcmp(PG_FUNCTION_ARGS)
{

}

/*
 * record_image_cmp :
 * Internal byte-oriented comparison function for records.
 *
 * Returns -1, 0 or 1
 *
 * Note: The normal concepts of "equality" do not apply here; different
 * representation of values considered to be equal are not considered to be
 * identical.  As an example, for the citext type 'A' and 'a' are equal, but
 * they are not identical.
 */
static int
record_image_cmp(FunctionCallInfo fcinfo)
{





































































































































































































































}

/*
 * record_image_eq :
 *		  compares two records for identical contents, based on byte
 *images result : returns true if the records are identical, false otherwise.
 *
 * Note: we do not use record_image_cmp here, since we can avoid
 * de-toasting for unequal lengths this way.
 */
Datum
record_image_eq(PG_FUNCTION_ARGS)
{





































































































































































}

Datum
record_image_ne(PG_FUNCTION_ARGS)
{

}

Datum
record_image_lt(PG_FUNCTION_ARGS)
{

}

Datum
record_image_gt(PG_FUNCTION_ARGS)
{

}

Datum
record_image_le(PG_FUNCTION_ARGS)
{

}

Datum
record_image_ge(PG_FUNCTION_ARGS)
{

}

Datum
btrecordimagecmp(PG_FUNCTION_ARGS)
{

}