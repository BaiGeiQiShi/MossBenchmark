/*-------------------------------------------------------------------------
 *
 * char.c
 *	  Functions for the built-in type "char" (not to be confused with
 *	  bpchar, which is the SQL CHAR(n) type).
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/char.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "libpq/pqformat.h"
#include "utils/builtins.h"

/*****************************************************************************
 *	 USER I/O ROUTINES
 **
 *****************************************************************************/

/*
 *		charin			- converts "x" to 'x'
 *
 * Note that an empty input string will implicitly be converted to \0.
 */
Datum
charin(PG_FUNCTION_ARGS)
{



}

/*
 *		charout			- converts 'x' to "x"
 *
 * Note that if the char value is \0, the resulting string will appear
 * to be empty (null-terminated after zero characters).  So this is the
 * inverse of the charin() function for such data.
 */
Datum
charout(PG_FUNCTION_ARGS)
{






}

/*
 *		charrecv			- converts external binary
 *format to char
 *
 * The external representation is one byte, with no character set
 * conversion.  This is somewhat dubious, perhaps, but in many
 * cases people use char for a 1-byte binary type.
 */
Datum
charrecv(PG_FUNCTION_ARGS)
{



}

/*
 *		charsend			- converts char to binary format
 */
Datum
charsend(PG_FUNCTION_ARGS)
{






}

/*****************************************************************************
 *	 PUBLIC ROUTINES
 **
 *****************************************************************************/

/*
 * NOTE: comparisons are done as though char is unsigned (uint8).
 * Conversions to and from integer are done as though char is signed (int8).
 *
 * You wanted consistency?
 */

Datum
chareq(PG_FUNCTION_ARGS)
{




}

Datum
charne(PG_FUNCTION_ARGS)
{




}

Datum
charlt(PG_FUNCTION_ARGS)
{




}

Datum
charle(PG_FUNCTION_ARGS)
{




}

Datum
chargt(PG_FUNCTION_ARGS)
{




}

Datum
charge(PG_FUNCTION_ARGS)
{




}

Datum
chartoi4(PG_FUNCTION_ARGS)
{



}

Datum
i4tochar(PG_FUNCTION_ARGS)
{








}

Datum
text_char(PG_FUNCTION_ARGS)
{


















}

Datum
char_text(PG_FUNCTION_ARGS)
{


















}