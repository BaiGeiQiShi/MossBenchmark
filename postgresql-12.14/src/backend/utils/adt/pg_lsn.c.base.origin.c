/*-------------------------------------------------------------------------
 *
 * pg_lsn.c
 *	  Operations for the pg_lsn datatype.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/pg_lsn.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "funcapi.h"
#include "libpq/pqformat.h"
#include "utils/builtins.h"
#include "utils/pg_lsn.h"

#define MAXPG_LSNLEN 17
#define MAXPG_LSNCOMPONENT 8

/*----------------------------------------------------------
 * Formatting and conversion routines.
 *---------------------------------------------------------*/

XLogRecPtr
pg_lsn_in_internal(const char *str, bool *have_error)
{
























}

Datum
pg_lsn_in(PG_FUNCTION_ARGS)
{











}

Datum
pg_lsn_out(PG_FUNCTION_ARGS)
{












}

Datum
pg_lsn_recv(PG_FUNCTION_ARGS)
{





}

Datum
pg_lsn_send(PG_FUNCTION_ARGS)
{






}

/*----------------------------------------------------------
 *	Operators for PostgreSQL LSNs
 *---------------------------------------------------------*/

Datum
pg_lsn_eq(PG_FUNCTION_ARGS)
{




}

Datum
pg_lsn_ne(PG_FUNCTION_ARGS)
{




}

Datum
pg_lsn_lt(PG_FUNCTION_ARGS)
{




}

Datum
pg_lsn_gt(PG_FUNCTION_ARGS)
{




}

Datum
pg_lsn_le(PG_FUNCTION_ARGS)
{




}

Datum
pg_lsn_ge(PG_FUNCTION_ARGS)
{




}

/* btree index opclass support */
Datum
pg_lsn_cmp(PG_FUNCTION_ARGS)
{















}

/* hash index opclass support */
Datum
pg_lsn_hash(PG_FUNCTION_ARGS)
{


}

Datum
pg_lsn_hash_extended(PG_FUNCTION_ARGS)
{

}

/*----------------------------------------------------------
 *	Arithmetic operators on PostgreSQL LSNs.
 *---------------------------------------------------------*/

Datum
pg_lsn_mi(PG_FUNCTION_ARGS)
{



















}