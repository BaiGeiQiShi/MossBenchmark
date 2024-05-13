/*-----------------------------------------------------------------------
 * ascii.c
 *	 The PostgreSQL routine for string to ascii conversion.
 *
 *	 Portions Copyright (c) 1999-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/ascii.c
 *
 *-----------------------------------------------------------------------
 */
#include "postgres.h"

#include "mb/pg_wchar.h"
#include "utils/ascii.h"
#include "utils/builtins.h"

static void
pg_to_ascii(unsigned char *src, unsigned char *src_end, unsigned char *dest, int enc);
static text *
encode_to_ascii(text *data, int enc);

/* ----------
 * to_ascii
 * ----------
 */
static void
pg_to_ascii(unsigned char *src, unsigned char *src_end, unsigned char *dest, int enc)
{


































































}

/* ----------
 * encode text
 *
 * The text datum is overwritten in-place, therefore this coding method
 * cannot support conversions that change the string length!
 * ----------
 */
static text *
encode_to_ascii(text *data, int enc)
{






}

/* ----------
 * convert to ASCII - enc is set as 'name' arg.
 * ----------
 */
Datum
to_ascii_encname(PG_FUNCTION_ARGS)
{










}

/* ----------
 * convert to ASCII - enc is set as int4
 * ----------
 */
Datum
to_ascii_enc(PG_FUNCTION_ARGS)
{









}

/* ----------
 * convert to ASCII - current enc is DatabaseEncoding
 * ----------
 */
Datum
to_ascii_default(PG_FUNCTION_ARGS)
{




}

/* ----------
 * Copy a string in an arbitrary backend-safe encoding, converting it to a
 * valid ASCII string by replacing non-ASCII bytes with '?'.  Otherwise the
 * behavior is identical to strlcpy(), except that we don't bother with a
 * return value.
 *
 * This must not trigger ereport(ERROR), as it is called in postmaster.
 * ----------
 */
void
ascii_safe_strlcpy(char *dest, const char *src, size_t destsiz)
{
  if (destsiz == 0)
  { /* corner case: no room for trailing nul */

  }

  while (--destsiz > 0)
  {
    /* use unsigned char here to avoid compiler warning */
    unsigned char ch = *src++;

    if (ch == '\0')
    {
      break;
    }
    /* Keep printable ASCII characters */
    if (32 <= ch && ch <= 127)
    {
      *dest = ch;
    }
    /* White-space is also OK */









    dest++;
  }

  *dest = '\0';
}
