/*-------------------------------------------------------------------------
 *
 * ginxlog.c
 *	  WAL replay logic for inverted index.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			 src/backend/access/gin/ginxlog.c
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/bufmask.h"
#include "access/gin_private.h"
#include "access/ginxlog.h"
#include "access/xlogutils.h"
#include "utils/memutils.h"

static MemoryContext opCtx; /* working memory for operations */

static void
ginRedoClearIncompleteSplit(XLogReaderState *record, uint8 block_id)
{
















}

static void
ginRedoCreatePTree(XLogReaderState *record)
{






















}

static void
ginRedoInsertEntry(Buffer buffer, bool isLeaf, BlockNumber rightblkno, void *rdata)
{
































}

/*
 * Redo recompression of posting list.  Doing all the changes in-place is not
 * always possible, because it might require more space than we've on the page.
 * Instead, once modification is required we copy unprocessed tail of the page
 * into separately allocated chunk of memory for further reading original
 * versions of segments.  Thanks to that we don't bother about moving page data
 * in-place.
 */
static void
ginRedoRecompress(Page page, ginxlogRecompressDataLeaf *data)
{


































































































































































































}

static void
ginRedoInsertData(Buffer buffer, bool isLeaf, BlockNumber rightblkno, void *rdata)
{























}

static void
ginRedoInsert(XLogReaderState *record)
{




















































}

static void
ginRedoSplit(XLogReaderState *record)
{



































}

/*
 * VACUUM_PAGE record contains simply a full image of the page, similar to
 * an XLOG_FPI record.
 */
static void
ginRedoVacuumPage(XLogReaderState *record)
{







}

static void
ginRedoVacuumDataLeafPage(XLogReaderState *record)
{






















}

static void
ginRedoDeletePage(XLogReaderState *record)
{




















































}

static void
ginRedoUpdateMetapage(XLogReaderState *record)
{
































































































}

static void
ginRedoInsertListPage(XLogReaderState *record)
{


















































}

static void
ginRedoDeleteListPages(XLogReaderState *record)
{














































}

void
gin_redo(XLogReaderState *record)
{












































}

void
gin_xlog_startup(void)
{

}

void
gin_xlog_cleanup(void)
{


}

/*
 * Mask a GIN page before running consistency checks on it.
 */
void
gin_mask(char *pagedata, BlockNumber blkno)
{






















}