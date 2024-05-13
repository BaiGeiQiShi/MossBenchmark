/*-------------------------------------------------------------------------
 *
 * name.c
 *	  Functions for the built-in type "name".
 *
 * name replaces char16 and is carefully implemented so that it
 * is a string of physical length NAMEDATALEN.
 * DO NOT use hard-coded constants anywhere
 * always use NAMEDATALEN as the symbolic constant!   - jolly 8/21/95
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/name.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/namespace.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"
#include "miscadmin.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/varlena.h"

/*****************************************************************************
 *	 USER I/O ROUTINES (none)
 **
 *****************************************************************************/

/*
 *		namein	- converts "..." to internal representation
 *
 *		Note:
 *				[Old] Currently if strlen(s) < NAMEDATALEN, the
 *extra chars are nulls Now, always NULL terminated
 */
Datum
namein(PG_FUNCTION_ARGS)
{

















}

/*
 *		nameout - converts internal representation to "..."
 */
Datum
nameout(PG_FUNCTION_ARGS)
{



}

/*
 *		namerecv			- converts external binary
 *format to name
 */
Datum
namerecv(PG_FUNCTION_ARGS)
{














}

/*
 *		namesend			- converts name to binary format
 */
Datum
namesend(PG_FUNCTION_ARGS)
{






}

/*****************************************************************************
 *	 COMPARISON/SORTING ROUTINES
 **
 *****************************************************************************/

/*
 *		nameeq	- returns 1 iff arguments are equal
 *		namene	- returns 1 iff arguments are not equal
 *		namelt	- returns 1 iff a < b
 *		namele	- returns 1 iff a <= b
 *		namegt	- returns 1 iff a > b
 *		namege	- returns 1 iff a >= b
 *
 * Note that the use of strncmp with NAMEDATALEN limit is mostly historical;
 * strcmp would do as well, because we do not allow NAME values that don't
 * have a '\0' terminator.  Whatever might be past the terminator is not
 * considered relevant to comparisons.
 */
static int
namecmp(Name arg1, Name arg2, Oid collid)
{








}

Datum
nameeq(PG_FUNCTION_ARGS)
{




}

Datum
namene(PG_FUNCTION_ARGS)
{




}

Datum
namelt(PG_FUNCTION_ARGS)
{




}

Datum
namele(PG_FUNCTION_ARGS)
{




}

Datum
namegt(PG_FUNCTION_ARGS)
{




}

Datum
namege(PG_FUNCTION_ARGS)
{




}

Datum
btnamecmp(PG_FUNCTION_ARGS)
{




}

Datum
btnamesortsupport(PG_FUNCTION_ARGS)
{












}

/*****************************************************************************
 *	 MISCELLANEOUS PUBLIC ROUTINES
 **
 *****************************************************************************/

int
namecpy(Name n1, const NameData *n2)
{






}

#ifdef NOT_USED
int
namecat(Name n1, Name n2)
{
  return namestrcat(n1, NameStr(*n2)); /* n2 can't be any longer than n1 */
}
#endif

int
namestrcpy(Name name, const char *str)
{






}

#ifdef NOT_USED
int
namestrcat(Name name, const char *str)
{
  int i;
  char *p, *q;

  if (!name || !str)
  {
    return -1;
  }
  for (i = 0, p = NameStr(*name); i < NAMEDATALEN && *p; ++i, ++p)
    ;
  for (q = str; i < NAMEDATALEN; ++i, ++p, ++q)
  {
    *p = *q;
    if (!*q)
    {
      break;
    }
  }
  return 0;
}
#endif

/*
 * Compare a NAME to a C string
 *
 * Assumes C collation always; be careful when using this for
 * anything but equality checks!
 */
int
namestrcmp(Name name, const char *str)
{













}

/*
 * SQL-functions CURRENT_USER, SESSION_USER
 */
Datum
current_user(PG_FUNCTION_ARGS)
{

}

Datum
session_user(PG_FUNCTION_ARGS)
{

}

/*
 * SQL-functions CURRENT_SCHEMA, CURRENT_SCHEMAS
 */
Datum
current_schema(PG_FUNCTION_ARGS)
{














}

Datum
current_schemas(PG_FUNCTION_ARGS)
{


























}

/*
 * SQL-function nameconcatoid(name, oid) returns name
 *
 * This is used in the information_schema to produce specific_name columns,
 * which are supposed to be unique per schema.  We achieve that (in an ugly
 * way) by appending the object's OID.  The result is the same as
 *		($1::text || '_' || $2::text)::name
 * except that, if it would not fit in NAMEDATALEN, we make it do so by
 * truncating the name input (not the oid).
 */
Datum
nameconcatoid(PG_FUNCTION_ARGS)
{






















}