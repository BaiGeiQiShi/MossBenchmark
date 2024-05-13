/*-------------------------------------------------------------------------
 *
 * hash_xlog.c
 *	  WAL replay logic for hash index.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/hash/hash_xlog.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/bufmask.h"
#include "access/hash.h"
#include "access/hash_xlog.h"
#include "access/xlogutils.h"
#include "access/xlog.h"
#include "access/transam.h"
#include "storage/procarray.h"
#include "miscadmin.h"

/*
 * replay a hash index meta page
 */
static void
hash_xlog_init_meta_page(XLogReaderState *record)
{





























}

/*
 * replay a hash index bitmap page
 */
static void
hash_xlog_init_bitmap_page(XLogReaderState *record)
{




























































}

/*
 * replay a hash index insert without split
 */
static void
hash_xlog_insert(XLogReaderState *record)
{













































}

/*
 * replay addition of overflow page for hash index
 */
static void
hash_xlog_add_ovfl_page(XLogReaderState *record)
{








































































































































}

/*
 * replay allocation of page for split operation
 */
static void
hash_xlog_split_allocate_page(XLogReaderState *record)
{

















































































































}

/*
 * replay of split operation
 */
static void
hash_xlog_split_page(XLogReaderState *record)
{








}

/*
 * replay completion of split operation
 */
static void
hash_xlog_split_complete(XLogReaderState *record)
{























































}

/*
 * replay move of page contents for squeeze operation of hash index
 */
static void
hash_xlog_move_page_contents(XLogReaderState *record)
{

































































































































}

/*
 * replay squeeze page operation of hash index
 */
static void
hash_xlog_squeeze_page(XLogReaderState *record)
{






























































































































































































































}

/*
 * replay delete operation of hash index
 */
static void
hash_xlog_delete(XLogReaderState *record)
{














































































}

/*
 * replay split cleanup flag operation for primary bucket page.
 */
static void
hash_xlog_split_cleanup(XLogReaderState *record)
{



















}

/*
 * replay for update meta page
 */
static void
hash_xlog_update_meta_page(XLogReaderState *record)
{




















}

/*
 * replay delete operation in hash index to remove
 * tuples marked as DEAD during index tuple insertion.
 */
static void
hash_xlog_vacuum_one_page(XLogReaderState *record)
{













































































}

void
hash_redo(XLogReaderState *record)
{














































}

/*
 * Mask a hash page before performing consistency checks on it.
 */
void
hash_mask(char *pagedata, BlockNumber blkno)
{


































}