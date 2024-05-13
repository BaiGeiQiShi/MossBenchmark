/*-------------------------------------------------------------------------
 *
 * tsvector.c
 *	  I/O functions for tsvector
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/tsvector.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "libpq/pqformat.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_utils.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

typedef struct
{
  WordEntry entry; /* must be first! */
  WordEntryPos *pos;
  int poslen; /* number of elements in pos */
} WordEntryIN;

/* Compare two WordEntryPos values for qsort */
int
compareWordEntryPos(const void *a, const void *b)
{








}

/*
 * Removes duplicate pos entries. If there's two entries with same pos
 * but different weight, the higher weight is retained.
 *
 * Returns new length.
 */
static int
uniquePos(WordEntryPos *a, int l)
{






























}

/* Compare two WordEntryIN values for qsort */
static int
compareentry(const void *va, const void *vb, void *arg)
{





}

/*
 * Sort an array of WordEntryIN, remove duplicates.
 * *outbuflen receives the amount of space needed for strings and positions.
 */
static int
uniqueentry(WordEntryIN *a, int l, char *buf, int *outbuflen)
{

































































}

static int
WordEntryCMP(WordEntry *a, WordEntry *b, char *buf)
{

}

Datum
tsvectorin(PG_FUNCTION_ARGS)
{


































































































































}

Datum
tsvectorout(PG_FUNCTION_ARGS)
{




















































































}

/*
 * Binary Input / Output functions. The binary format is as follows:
 *
 * uint32	number of lexemes
 *
 * for each lexeme:
 *		lexeme text in client encoding, null-terminated
 *		uint16	number of positions
 *		for each position:
 *			uint16 WordEntryPos
 */

Datum
tsvectorsend(PG_FUNCTION_ARGS)
{



































}

Datum
tsvectorrecv(PG_FUNCTION_ARGS)
{




















































































































}