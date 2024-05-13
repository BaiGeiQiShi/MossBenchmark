/*-------------------------------------------------------------------------
 *
 * tid.c
 *	  Functions for the built-in type tuple id
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/tid.c
 *
 * NOTES
 *	  input routine largely stolen from boxin().
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>
#include <limits.h>

#include "access/heapam.h"
#include "access/sysattr.h"
#include "access/tableam.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "parser/parsetree.h"
#include "utils/acl.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/snapmgr.h"
#include "utils/varlena.h"

#define DatumGetItemPointer(X) ((ItemPointer)DatumGetPointer(X))
#define ItemPointerGetDatum(X) PointerGetDatum(X)
#define PG_GETARG_ITEMPOINTER(n) DatumGetItemPointer(PG_GETARG_DATUM(n))
#define PG_RETURN_ITEMPOINTER(x) return ItemPointerGetDatum(x)

#define LDELIM '('
#define RDELIM ')'
#define DELIM ','
#define NTIDARGS 2

/* ----------------------------------------------------------------
 *		tidin
 * ----------------------------------------------------------------
 */
Datum
tidin(PG_FUNCTION_ARGS)
{










































}

/* ----------------------------------------------------------------
 *		tidout
 * ----------------------------------------------------------------
 */
Datum
tidout(PG_FUNCTION_ARGS)
{












}

/*
 *		tidrecv			- converts external binary format to tid
 */
Datum
tidrecv(PG_FUNCTION_ARGS)
{













}

/*
 *		tidsend			- converts tid to binary format
 */
Datum
tidsend(PG_FUNCTION_ARGS)
{







}

/*****************************************************************************
 *	 PUBLIC ROUTINES
 **
 *****************************************************************************/

Datum
tideq(PG_FUNCTION_ARGS)
{




}

Datum
tidne(PG_FUNCTION_ARGS)
{




}

Datum
tidlt(PG_FUNCTION_ARGS)
{




}

Datum
tidle(PG_FUNCTION_ARGS)
{




}

Datum
tidgt(PG_FUNCTION_ARGS)
{




}

Datum
tidge(PG_FUNCTION_ARGS)
{




}

Datum
bttidcmp(PG_FUNCTION_ARGS)
{




}

Datum
tidlarger(PG_FUNCTION_ARGS)
{




}

Datum
tidsmaller(PG_FUNCTION_ARGS)
{




}

Datum
hashtid(PG_FUNCTION_ARGS)
{









}

Datum
hashtidextended(PG_FUNCTION_ARGS)
{





}

/*
 *	Functions to get latest tid of a specified tuple.
 *
 *	Maybe these implementations should be moved to another place
 */

static ItemPointerData Current_last_tid = {{0, 0}, 0};

void
setLastTid(const ItemPointer tid)
{

}

/*
 *	Handle CTIDs of views.
 *		CTID should be defined in the view and it must
 *		correspond to the CTID of a base relation.
 */
static Datum
currtid_for_view(Relation viewrel, ItemPointer tid)
{

































































}

Datum
currtid_byreloid(PG_FUNCTION_ARGS)
{












































}

Datum
currtid_byrelname(PG_FUNCTION_ARGS)
{








































}