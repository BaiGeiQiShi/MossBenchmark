/*-------------------------------------------------------------------------
 *
 * xlogdesc.c
 *	  rmgr descriptor routines for access/transam/xlog.c
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/rmgrdesc/xlogdesc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/transam.h"
#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "catalog/pg_control.h"
#include "utils/guc.h"
#include "utils/timestamp.h"

/*
 * GUC support
 */
const struct config_enum_entry wal_level_options[] = {{"minimal", WAL_LEVEL_MINIMAL, false}, {"replica", WAL_LEVEL_REPLICA, false}, {"archive", WAL_LEVEL_REPLICA, true}, /* deprecated */
    {"hot_standby", WAL_LEVEL_REPLICA, true},                                                                                                                             /* deprecated */
    {"logical", WAL_LEVEL_LOGICAL, false}, {NULL, 0, false}};

void
xlog_desc(StringInfo buf, XLogReaderState *record)
{











































































}

const char *
xlog_identify(uint8 info)
{














































}