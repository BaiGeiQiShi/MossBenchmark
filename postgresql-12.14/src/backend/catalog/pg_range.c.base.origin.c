/*-------------------------------------------------------------------------
 *
 * pg_range.c
 *	  routines to support manipulation of the pg_range relation
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/catalog/pg_range.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/genam.h"
#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_range.h"
#include "catalog/pg_type.h"
#include "utils/fmgroids.h"
#include "utils/rel.h"

/*
 * RangeCreate
 *		Create an entry in pg_range.
 */
void
RangeCreate(Oid rangeTypeOid, Oid rangeSubType, Oid rangeCollation, Oid rangeSubOpclass, RegProcedure rangeCanonical, RegProcedure rangeSubDiff)
{
































































}

/*
 * RangeDelete
 *		Remove the pg_range entry for the specified type.
 */
void
RangeDelete(Oid rangeTypeOid)
{



















}