/*-------------------------------------------------------------------------
 *
 * ginscan.c
 *	  routines to manage scans of inverted index relations
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/gin/ginscan.c
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/gin_private.h"
#include "access/relscan.h"
#include "pgstat.h"
#include "utils/memutils.h"
#include "utils/rel.h"

IndexScanDesc
ginbeginscan(Relation rel, int nkeys, int norderbys)
{



















}

/*
 * Create a new GinScanEntry, unless an equivalent one already exists,
 * in which case just return it
 */
static GinScanEntry
ginFillScanEntry(GinScanOpaque so, OffsetNumber attnum, StrategyNumber strategy, int32 searchMode, Datum queryKey, GinNullCategory queryCategory, bool isPartialMatch, Pointer extra_data)
{






















































}

/*
 * Initialize the next GinScanKey using the output from the extractQueryFn
 */
static void
ginFillScanKey(GinScanOpaque so, OffsetNumber attnum, StrategyNumber strategy, int32 searchMode, Datum query, uint32 nQueryValues, Datum *queryValues, GinNullCategory *queryCategories, bool *partial_matches, Pointer *extra_data)
{





















































































}

/*
 * Release current scan keys, if any.
 */
void
ginFreeScanKeys(GinScanOpaque so)
{



































}

void
ginNewScanKey(IndexScanDesc scan)
{

































































































































}

void
ginrescan(IndexScanDesc scan, ScanKey scankey, int nscankeys, ScanKey orderbys, int norderbys)
{








}

void
ginendscan(IndexScanDesc scan)
{








}