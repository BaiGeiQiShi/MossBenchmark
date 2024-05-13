/*
 * brin_xlog.c
 *		XLog replay routines for BRIN indexes
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/brin/brin_xlog.c
 */
#include "postgres.h"

#include "access/brin_page.h"
#include "access/brin_pageops.h"
#include "access/brin_xlog.h"
#include "access/bufmask.h"
#include "access/xlogutils.h"

/*
 * xlog replay routines
 */
static void
brin_xlog_createidx(XLogReaderState *record)
{













}

/*
 * Common part of an insert or update. Inserts the new tuple and updates the
 * revmap.
 */
static void
brin_xlog_insert_update(XLogReaderState *record, xl_brin_insert *xlrec)
{












































































}

/*
 * replay a BRIN index insertion
 */
static void
brin_xlog_insert(XLogReaderState *record)
{



}

/*
 * replay a BRIN index update
 */
static void
brin_xlog_update(XLogReaderState *record)
{





























}

/*
 * Update a tuple on a single page.
 */
static void
brin_xlog_samepage_update(XLogReaderState *record)
{


































}

/*
 * Replay a revmap page extension
 */
static void
brin_xlog_revmap_extend(XLogReaderState *record)
{

























































}

static void
brin_xlog_desummarize_page(XLogReaderState *record)
{







































}

void
brin_redo(XLogReaderState *record)
{

























}

/*
 * Mask a BRIN page before doing consistency checks.
 */
void
brin_mask(char *pagedata, BlockNumber blkno)
{






















}