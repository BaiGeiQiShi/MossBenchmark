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
  char *ch = PG_GETARG_CSTRING(0);

  PG_RETURN_CHAR(ch[0]);
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
  char ch = PG_GETARG_CHAR(0);
  char *result = (char *)palloc(2);

  result[0] = ch;
  result[1] = '\0';
  PG_RETURN_CSTRING(result);
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
  char arg1 = PG_GETARG_CHAR(0);
  char arg2 = PG_GETARG_CHAR(1);

  PG_RETURN_BOOL(arg1 == arg2);
}

Datum
charne(PG_FUNCTION_ARGS)
{
  char arg1 = PG_GETARG_CHAR(0);
  char arg2 = PG_GETARG_CHAR(1);

  PG_RETURN_BOOL(arg1 != arg2);
}

Datum
charlt(PG_FUNCTION_ARGS)
{
  char arg1 = PG_GETARG_CHAR(0);
  char arg2 = PG_GETARG_CHAR(1);

  PG_RETURN_BOOL((uint8)arg1 < (uint8)arg2);
}

Datum
charle(PG_FUNCTION_ARGS)
{
  char arg1 = PG_GETARG_CHAR(0);
  char arg2 = PG_GETARG_CHAR(1);

  PG_RETURN_BOOL((uint8)arg1 <= (uint8)arg2);
}

Datum
chargt(PG_FUNCTION_ARGS)
{
  char arg1 = PG_GETARG_CHAR(0);
  char arg2 = PG_GETARG_CHAR(1);

  PG_RETURN_BOOL((uint8)arg1 > (uint8)arg2);
}

Datum
charge(PG_FUNCTION_ARGS)
{
  char arg1 = PG_GETARG_CHAR(0);
  char arg2 = PG_GETARG_CHAR(1);

  PG_RETURN_BOOL((uint8)arg1 >= (uint8)arg2);
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
  text *arg1 = PG_GETARG_TEXT_PP(0);
  char result;

  /*
   * An empty input string is converted to \0 (for consistency with charin).
   * If the input is longer than one character, the excess data is silently
   * discarded.
   */
  if (VARSIZE_ANY_EXHDR(arg1) > 0)
  {
    result = *(VARDATA_ANY(arg1));
  }
  else
  {

  }

  PG_RETURN_CHAR(result);
}

Datum
char_text(PG_FUNCTION_ARGS)
{


















}
