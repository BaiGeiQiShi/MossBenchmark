/*-------------------------------------------------------------------------
 *
 * oid.c
 *	  Functions for the built-in type Oid ... also oidvector.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/oid.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>
#include <limits.h>

#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "nodes/value.h"
#include "utils/array.h"
#include "utils/builtins.h"

#define OidVectorSize(n) (offsetof(oidvector, values) + (n) * sizeof(Oid))

/*****************************************************************************
 *	 USER I/O ROUTINES
 **
 *****************************************************************************/

static Oid
oidin_subr(const char *s, char **endloc)
{








































































}

Datum
oidin(PG_FUNCTION_ARGS)
{





}

Datum
oidout(PG_FUNCTION_ARGS)
{





}

/*
 *		oidrecv			- converts external binary format to oid
 */
Datum
oidrecv(PG_FUNCTION_ARGS)
{



}

/*
 *		oidsend			- converts oid to binary format
 */
Datum
oidsend(PG_FUNCTION_ARGS)
{






}

/*
 * construct oidvector given a raw array of Oids
 *
 * If oids is NULL then caller must fill values[] afterward
 */
oidvector *
buildoidvector(const Oid *oids, int n)
{





















}

/*
 *		oidvectorin			- converts "num num ..." to
 *internal form
 */
Datum
oidvectorin(PG_FUNCTION_ARGS)
{



































}

/*
 *		oidvectorout - converts internal form to "num num ..."
 */
Datum
oidvectorout(PG_FUNCTION_ARGS)
{



















}

/*
 *		oidvectorrecv			- converts external binary
 *format to oidvector
 */
Datum
oidvectorrecv(PG_FUNCTION_ARGS)
{




































}

/*
 *		oidvectorsend			- converts oidvector to binary
 *format
 */
Datum
oidvectorsend(PG_FUNCTION_ARGS)
{

}

/*
 *		oidparse				- get OID from
 *IConst/FConst node
 */
Oid
oidparse(Node *node)
{
















}

/* qsort comparison function for Oids */
int
oid_cmp(const void *p1, const void *p2)
{












}

/*****************************************************************************
 *	 PUBLIC ROUTINES
 **
 *****************************************************************************/

Datum
oideq(PG_FUNCTION_ARGS)
{




}

Datum
oidne(PG_FUNCTION_ARGS)
{




}

Datum
oidlt(PG_FUNCTION_ARGS)
{




}

Datum
oidle(PG_FUNCTION_ARGS)
{




}

Datum
oidge(PG_FUNCTION_ARGS)
{




}

Datum
oidgt(PG_FUNCTION_ARGS)
{




}

Datum
oidlarger(PG_FUNCTION_ARGS)
{




}

Datum
oidsmaller(PG_FUNCTION_ARGS)
{




}

Datum
oidvectoreq(PG_FUNCTION_ARGS)
{



}

Datum
oidvectorne(PG_FUNCTION_ARGS)
{



}

Datum
oidvectorlt(PG_FUNCTION_ARGS)
{



}

Datum
oidvectorle(PG_FUNCTION_ARGS)
{



}

Datum
oidvectorge(PG_FUNCTION_ARGS)
{



}

Datum
oidvectorgt(PG_FUNCTION_ARGS)
{



}