/*-------------------------------------------------------------------------
 *
 * xid.c
 *	  POSTGRES transaction identifier and command identifier datatypes.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/xid.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "access/multixact.h"
#include "access/transam.h"
#include "access/xact.h"
#include "libpq/pqformat.h"
#include "utils/builtins.h"

#define PG_GETARG_TRANSACTIONID(n) DatumGetTransactionId(PG_GETARG_DATUM(n))
#define PG_RETURN_TRANSACTIONID(x) return TransactionIdGetDatum(x)

#define PG_GETARG_COMMANDID(n) DatumGetCommandId(PG_GETARG_DATUM(n))
#define PG_RETURN_COMMANDID(x) return CommandIdGetDatum(x)

Datum
xidin(PG_FUNCTION_ARGS)
{



}

Datum
xidout(PG_FUNCTION_ARGS)
{





}

/*
 *		xidrecv			- converts external binary format to xid
 */
Datum
xidrecv(PG_FUNCTION_ARGS)
{



}

/*
 *		xidsend			- converts xid to binary format
 */
Datum
xidsend(PG_FUNCTION_ARGS)
{






}

/*
 *		xideq			- are two xids equal?
 */
Datum
xideq(PG_FUNCTION_ARGS)
{




}

/*
 *		xidneq			- are two xids different?
 */
Datum
xidneq(PG_FUNCTION_ARGS)
{




}

/*
 *		xid_age			- compute age of an XID (relative to
 *latest stable xid)
 */
Datum
xid_age(PG_FUNCTION_ARGS)
{










}

/*
 *		mxid_age			- compute age of a multi XID
 *(relative to latest stable mxid)
 */
Datum
mxid_age(PG_FUNCTION_ARGS)
{









}

/*
 * xidComparator
 *		qsort comparison function for XIDs
 *
 * We can't use wraparound comparison for XIDs because that does not respect
 * the triangle inequality!  Any old sort order will do.
 */
int
xidComparator(const void *arg1, const void *arg2)
{












}

/*
 * xidLogicalComparator
 *		qsort comparison function for XIDs
 *
 * This is used to compare only XIDs from the same epoch (e.g. for backends
 * running at the same time). So there must be only normal XIDs, so there's
 * no issue with triangle inequality.
 */
int
xidLogicalComparator(const void *arg1, const void *arg2)
{

















}

/*****************************************************************************
 *	 COMMAND IDENTIFIER ROUTINES
 **
 *****************************************************************************/

/*
 *		cidin	- converts CommandId to internal representation.
 */
Datum
cidin(PG_FUNCTION_ARGS)
{



}

/*
 *		cidout	- converts a cid to external representation.
 */
Datum
cidout(PG_FUNCTION_ARGS)
{





}

/*
 *		cidrecv			- converts external binary format to cid
 */
Datum
cidrecv(PG_FUNCTION_ARGS)
{



}

/*
 *		cidsend			- converts cid to binary format
 */
Datum
cidsend(PG_FUNCTION_ARGS)
{






}

Datum
cideq(PG_FUNCTION_ARGS)
{




}