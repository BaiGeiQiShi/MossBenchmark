/*-------------------------------------------------------------------------
 *
 * standbydesc.c
 *	  rmgr descriptor routines for storage/ipc/standby.c
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/rmgrdesc/standbydesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "storage/standbydefs.h"

static void
standby_desc_running_xacts(StringInfo buf, xl_running_xacts *xlrec)
{
















}

void
standby_desc(StringInfo buf, XLogReaderState *record)
{

























}

const char *
standby_identify(uint8 info)
{
















}

/*
 * This routine is used by both standby_desc and xact_desc, because
 * transaction commits and XLOG_INVALIDATIONS messages contain invalidations;
 * it seems pointless to duplicate the code.
 */
void
standby_desc_invalidations(StringInfo buf, int nmsgs, SharedInvalidationMessage *msgs, Oid dbId, Oid tsId, bool relcacheInitFileInval)
{











































}