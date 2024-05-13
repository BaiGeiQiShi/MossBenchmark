/*-------------------------------------------------------------------------
 *
 * heapdesc.c
 *	  rmgr descriptor routines for access/heap/heapam.c
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/rmgrdesc/heapdesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/heapam_xlog.h"

static void
out_infobits(StringInfo buf, uint8 infobits)
{




















}

void
heap_desc(StringInfo buf, XLogReaderState *record)
{







































































}
void
heap2_desc(StringInfo buf, XLogReaderState *record)
{
















































}

const char *
heap_identify(uint8 info)
{








































}

const char *
heap2_identify(uint8 info)
{


































}
