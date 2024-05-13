/*-------------------------------------------------------------------------
 *
 * mxactdesc.c
 *	  rmgr descriptor routines for access/transam/multixact.c
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/rmgrdesc/mxactdesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/multixact.h"

static void
out_member(StringInfo buf, MultiXactMember *member)
{

























}

void
multixact_desc(StringInfo buf, XLogReaderState *record)
{



























}

const char *
multixact_identify(uint8 info)
{



















}