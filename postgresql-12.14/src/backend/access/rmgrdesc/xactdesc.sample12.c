/*-------------------------------------------------------------------------
 *
 * xactdesc.c
 *	  rmgr descriptor routines for access/transam/xact.c
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/rmgrdesc/xactdesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/transam.h"
#include "access/xact.h"
#include "storage/sinval.h"
#include "storage/standbydefs.h"
#include "utils/timestamp.h"

/*
 * Parse the WAL format of an xact commit and abort records into an easier to
 * understand format.
 *
 * This routines are in xactdesc.c because they're accessed in backend (when
 * replaying WAL) and frontend (pg_waldump) code. This file is the only xact
 * specific one shared between both. They're complicated enough that
 * duplication would be bothersome.
 */

void
ParseCommitRecord(uint8 info, xl_xact_commit *xlrec, xl_xact_parsed_commit *parsed)
{


























































































}

void
ParseAbortRecord(uint8 info, xl_xact_abort *xlrec, xl_xact_parsed_abort *parsed)
{















































































}

static void
xact_desc_commit(StringInfo buf, uint8 info, xl_xact_commit *xlrec, RepOriginId origin_id)
{














































}

static void
xact_desc_abort(StringInfo buf, uint8 info, xl_xact_abort *xlrec)
{
































}

static void
xact_desc_assignment(StringInfo buf, xl_xact_assignment *xlrec)
{








}

void
xact_desc(StringInfo buf, XLogReaderState *record)
{



























}

const char *
xact_identify(uint8 info)
{

























}