/*-------------------------------------------------------------------------
 *
 * fastpath.c
 *	  routines to handle function requests from the frontend
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/tcop/fastpath.c
 *
 * NOTES
 *	  This cruft is the server side of PQfn.
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/xact.h"
#include "catalog/objectaccess.h"
#include "catalog/pg_proc.h"
#include "libpq/libpq.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "port/pg_bswap.h"
#include "tcop/fastpath.h"
#include "tcop/tcopprot.h"
#include "utils/acl.h"
#include "utils/lsyscache.h"
#include "utils/snapmgr.h"
#include "utils/syscache.h"

/*
 * Formerly, this code attempted to cache the function and type info
 * looked up by fetch_fp_info, but only for the duration of a single
 * transaction command (since in theory the info could change between
 * commands).  This was utterly useless, because postgres.c executes
 * each fastpath call as a separate transaction command, and so the
 * cached data could never actually have been reused.  If it had worked
 * as intended, it would have had problems anyway with dangling references
 * in the FmgrInfo struct.  So, forget about caching and just repeat the
 * syscache fetches on each usage.  They're not *that* expensive.
 */
struct fp_info
{
  Oid funcid;
  FmgrInfo flinfo; /* function lookup info for funcid */
  Oid namespace;   /* other stuff from pg_proc */
  Oid rettype;
  Oid argtypes[FUNC_MAX_ARGS];
  char fname[NAMEDATALEN]; /* function name for logging */
};

static int16
parse_fcall_arguments(StringInfo msgBuf, struct fp_info *fip, FunctionCallInfo fcinfo);
static int16
parse_fcall_arguments_20(StringInfo msgBuf, struct fp_info *fip, FunctionCallInfo fcinfo);

/* ----------------
 *		GetOldFunctionMessage
 *
 * In pre-3.0 protocol, there is no length word on the message, so we have
 * to have code that understands the message layout to absorb the message
 * into a buffer.  We want to do this before we start execution, so that
 * we do not lose sync with the frontend if there's an error.
 *
 * The caller should already have initialized buf to empty.
 * ----------------
 */
int
GetOldFunctionMessage(StringInfo buf)
{






















































}

/* ----------------
 *		SendFunctionResult
 *
 * Note: although this routine doesn't check, the format had better be 1
 * (binary) when talking to a pre-3.0 client.
 * ----------------
 */
static void
SendFunctionResult(Datum retval, bool isnull, Oid rettype, int16 format)
{






















































}

/*
 * fetch_fp_info
 *
 * Performs catalog lookups to load a struct fp_info 'fip' for the
 * function 'func_id'.
 */
static void
fetch_fp_info(Oid func_id, struct fp_info *fip)
{
















































}

/*
 * HandleFunctionRequest
 *
 * Server side of PQfn (fastpath function calls from the frontend).
 * This corresponds to the libpq protocol symbol "F".
 *
 * INPUT:
 *		postgres.c has already read the message body and will pass it in
 *		msgBuf.
 *
 * Note: palloc()s done here and in the called function do not need to be
 * cleaned up explicitly.  We are called from PostgresMain() in the
 * MessageContext memory context, which will be automatically reset when
 * control returns to PostgresMain.
 */
void
HandleFunctionRequest(StringInfo msgBuf)
{










































































































































}

/*
 * Parse function arguments in a 3.0 protocol message
 *
 * Argument values are loaded into *fcinfo, and the desired result format
 * is returned.
 */
static int16
parse_fcall_arguments(StringInfo msgBuf, struct fp_info *fip, FunctionCallInfo fcinfo)
{








































































































































}

/*
 * Parse function arguments in a 2.0 protocol message
 *
 * Argument values are loaded into *fcinfo, and the desired result format
 * is returned.
 */
static int16
parse_fcall_arguments_20(StringInfo msgBuf, struct fp_info *fip, FunctionCallInfo fcinfo)
{



























































}