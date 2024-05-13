/*-------------------------------------------------------------------------
 *
 * gindesc.c
 *	  rmgr descriptor routines for access/transam/gin/ginxlog.c
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/rmgrdesc/gindesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/ginxlog.h"
#include "access/xlogutils.h"
#include "lib/stringinfo.h"
#include "storage/relfilenode.h"

static void
desc_recompress_leaf(StringInfo buf, ginxlogRecompressDataLeaf *insertData)
{













































}

void
gin_desc(StringInfo buf, XLogReaderState *record)
{






































































































}

const char *
gin_identify(uint8 info)
{


































}
