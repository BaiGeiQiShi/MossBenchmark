/*-------------------------------------------------------------------------
 *
 * spginsert.c
 *	  Externally visible index creation/insertion routines
 *
 * All the actual insertion logic is in spgdoinsert.c.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/access/spgist/spginsert.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
#include "access/spgist_private.h"
#include "access/spgxlog.h"
#include "access/tableam.h"
#include "access/xlog.h"
#include "access/xloginsert.h"
#include "catalog/index.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "storage/smgr.h"
#include "utils/memutils.h"
#include "utils/rel.h"

typedef struct
{
  SpGistState spgstate; /* SPGiST's working state */
  int64 indtuples;      /* total number of tuples indexed */
  MemoryContext tmpCtx; /* per-tuple temporary context */
} SpGistBuildState;

/* Callback to process one heap tuple during table_index_build_scan */
static void
spgistBuildCallback(Relation index, HeapTuple htup, Datum *values, bool *isnull, bool tupleIsAlive, void *state)
{






















}

/*
 * Build an SP-GiST index.
 */
IndexBuildResult *
spgbuild(Relation heap, Relation index, IndexInfo *indexInfo)
{

































































}

/*
 * Build an empty SPGiST index in the initialization fork
 */
void
spgbuildempty(Relation index)
{





































}

/*
 * Insert one new tuple into an SPGiST index.
 */
bool
spginsert(Relation index, Datum *values, bool *isnull, ItemPointer ht_ctid, Relation heapRel, IndexUniqueCheck checkUnique, IndexInfo *indexInfo)
{




























}