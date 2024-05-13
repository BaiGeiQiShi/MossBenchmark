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
  char *str = PG_GETARG_CSTRING(0);

  PG_RETURN_TRANSACTIONID((TransactionId)strtoul(str, NULL, 0));
}

Datum
xidout(PG_FUNCTION_ARGS)
{
  TransactionId transactionId = PG_GETARG_TRANSACTIONID(0);
  char *result = (char *)palloc(16);

  snprintf(result, 16, "%lu", (unsigned long)transactionId);
  PG_RETURN_CSTRING(result);
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
  TransactionId xid1 = PG_GETARG_TRANSACTIONID(0);
  TransactionId xid2 = PG_GETARG_TRANSACTIONID(1);

  PG_RETURN_BOOL(TransactionIdEquals(xid1, xid2));
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
  TransactionId xid1 = *(const TransactionId *)arg1;
  TransactionId xid2 = *(const TransactionId *)arg2;

  if (xid1 > xid2)
  {
    return 1;
  }
  if (xid1 < xid2)
  {
    return -1;
  }

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
  char *str = PG_GETARG_CSTRING(0);

  PG_RETURN_COMMANDID((CommandId)strtoul(str, NULL, 0));
}

/*
 *		cidout	- converts a cid to external representation.
 */
Datum
cidout(PG_FUNCTION_ARGS)
{
  CommandId c = PG_GETARG_COMMANDID(0);
  char *result = (char *)palloc(16);

  snprintf(result, 16, "%lu", (unsigned long)c);
  PG_RETURN_CSTRING(result);
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