/*-------------------------------------------------------------------------
 *
 * mac8.c
 *	  PostgreSQL type definitions for 8 byte (EUI-64) MAC addresses.
 *
 * EUI-48 (6 byte) MAC addresses are accepted as input and are stored in
 * EUI-64 format, with the 4th and 5th bytes set to FF and FE, respectively.
 *
 * Output is always in 8 byte (EUI-64) format.
 *
 * The following code is written with the assumption that the OUI field
 * size is 24 bits.
 *
 * Portions Copyright (c) 1998-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  src/backend/utils/adt/mac8.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "libpq/pqformat.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"
#include "utils/inet.h"

/*
 *	Utility macros used for sorting and comparing:
 */
#define hibits(addr) ((unsigned long)(((addr)->a << 24) | ((addr)->b << 16) | ((addr)->c << 8) | ((addr)->d)))

#define lobits(addr) ((unsigned long)(((addr)->e << 24) | ((addr)->f << 16) | ((addr)->g << 8) | ((addr)->h)))

static unsigned char
hex2_to_uchar(const unsigned char *str, const unsigned char *ptr);

static const signed char hexlookup[128] = {
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    10,
    11,
    12,
    13,
    14,
    15,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    10,
    11,
    12,
    13,
    14,
    15,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
};

/*
 * hex2_to_uchar - convert 2 hex digits to a byte (unsigned char)
 *
 * This will ereport() if the end of the string is reached ('\0' found), or if
 * either character is not a valid hex digit.
 *
 * ptr is the pointer to where the digits to convert are in the string, str is
 * the entire string, which is used only for error reporting.
 */
static inline unsigned char
hex2_to_uchar(const unsigned char *ptr, const unsigned char *str)
{








































}

/*
 * MAC address (EUI-48 and EUI-64) reader. Accepts several common notations.
 */
Datum
macaddr8_in(PG_FUNCTION_ARGS)
{


























































































































}

/*
 * MAC8 address (EUI-64) output function. Fixed format.
 */
Datum
macaddr8_out(PG_FUNCTION_ARGS)
{








}

/*
 * macaddr8_recv - converts external binary format(EUI-48 and EUI-64) to
 * macaddr8
 *
 * The external representation is just the eight bytes, MSB first.
 */
Datum
macaddr8_recv(PG_FUNCTION_ARGS)
{

























}

/*
 * macaddr8_send - converts macaddr8(EUI-64) to binary format
 */
Datum
macaddr8_send(PG_FUNCTION_ARGS)
{














}

/*
 * macaddr8_cmp_internal - comparison function for sorting:
 */
static int32
macaddr8_cmp_internal(macaddr8 *a1, macaddr8 *a2)
{




















}

Datum
macaddr8_cmp(PG_FUNCTION_ARGS)
{




}

/*
 * Boolean comparison functions.
 */

Datum
macaddr8_lt(PG_FUNCTION_ARGS)
{




}

Datum
macaddr8_le(PG_FUNCTION_ARGS)
{




}

Datum
macaddr8_eq(PG_FUNCTION_ARGS)
{




}

Datum
macaddr8_ge(PG_FUNCTION_ARGS)
{




}

Datum
macaddr8_gt(PG_FUNCTION_ARGS)
{




}

Datum
macaddr8_ne(PG_FUNCTION_ARGS)
{




}

/*
 * Support function for hash indexes on macaddr8.
 */
Datum
hashmacaddr8(PG_FUNCTION_ARGS)
{



}

Datum
hashmacaddr8extended(PG_FUNCTION_ARGS)
{



}

/*
 * Arithmetic functions: bitwise NOT, AND, OR.
 */
Datum
macaddr8_not(PG_FUNCTION_ARGS)
{














}

Datum
macaddr8_and(PG_FUNCTION_ARGS)
{















}

Datum
macaddr8_or(PG_FUNCTION_ARGS)
{















}

/*
 * Truncation function to allow comparing macaddr8 manufacturers.
 */
Datum
macaddr8_trunc(PG_FUNCTION_ARGS)
{















}

/*
 * Set 7th bit for modified EUI-64 as used in IPv6.
 */
Datum
macaddr8_set7bit(PG_FUNCTION_ARGS)
{















}

/*----------------------------------------------------------
 *	Conversion operators.
 *---------------------------------------------------------*/

Datum
macaddrtomacaddr8(PG_FUNCTION_ARGS)
{















}

Datum
macaddr8tomacaddr(PG_FUNCTION_ARGS)
{


















}