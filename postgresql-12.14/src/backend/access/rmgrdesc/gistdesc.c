/*-------------------------------------------------------------------------
 *
 * gistdesc.c
 *	  rmgr descriptor routines for access/gist/gistxlog.c
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/rmgrdesc/gistdesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/gistxlog.h"
#include "lib/stringinfo.h"
#include "storage/relfilenode.h"

static void
out_gistxlogPageUpdate(StringInfo buf, gistxlogPageUpdate *xlrec)
{
}

static void
out_gistxlogPageReuse(StringInfo buf, gistxlogPageReuse *xlrec)
{

}

static void
out_gistxlogDelete(StringInfo buf, gistxlogDelete *xlrec)
{

}

static void
out_gistxlogPageSplit(StringInfo buf, gistxlogPageSplit *xlrec)
{

}

static void
out_gistxlogPageDelete(StringInfo buf, gistxlogPageDelete *xlrec)
{

}

void
gist_desc(StringInfo buf, XLogReaderState *record)
{





















}

const char *
gist_identify(uint8 info)
{






















}
