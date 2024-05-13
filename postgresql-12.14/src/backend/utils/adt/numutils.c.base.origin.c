/*-------------------------------------------------------------------------
 *
 * numutils.c
 *	  utility functions for I/O of built-in numeric types.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/numutils.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <math.h>
#include <limits.h>
#include <ctype.h>

#include "common/int.h"
#include "utils/builtins.h"

/*
 * pg_atoi: convert string to integer
 *
 * allows any number of leading or trailing whitespace characters.
 *
 * 'size' is the sizeof() the desired integral result (1, 2, or 4 bytes).
 *
 * c, if not 0, is a terminator character that may appear after the
 * integer (plus whitespace).  If 0, the string must end after the integer.
 *
 * Unlike plain atoi(), this will throw ereport() upon bad input format or
 * overflow.
 */
int32
pg_atoi(const char *s, int size, int c)
{



































































}

/*
 * Convert input string to a signed 16 bit integer.
 *
 * Allows any number of leading or trailing whitespace characters. Will throw
 * ereport() upon bad input format or overflow.
 *
 * NB: Accumulate input as a negative number, to deal with two's complement
 * representation of the most negative number, which can't be represented as a
 * positive number.
 */
int16
pg_strtoint16(const char *s)
{




































































}

/*
 * Convert input string to a signed 32 bit integer.
 *
 * Allows any number of leading or trailing whitespace characters. Will throw
 * ereport() upon bad input format or overflow.
 *
 * NB: Accumulate input as a negative number, to deal with two's complement
 * representation of the most negative number, which can't be represented as a
 * positive number.
 */
int32
pg_strtoint32(const char *s)
{




































































}

/*
 * pg_itoa: converts a signed 16-bit integer to its string representation
 *
 * Caller must ensure that 'a' points to enough memory to hold the result
 * (at least 7 bytes, counting a leading sign and trailing NUL).
 *
 * It doesn't seem worth implementing this separately.
 */
void
pg_itoa(int16 i, char *a)
{

}

/*
 * pg_ltoa: converts a signed 32-bit integer to its string representation
 *
 * Caller must ensure that 'a' points to enough memory to hold the result
 * (at least 12 bytes, counting a leading sign and trailing NUL).
 */
void
pg_ltoa(int32 value, char *a)
{













































}

/*
 * pg_lltoa: convert a signed 64-bit integer to its string representation
 *
 * Caller must ensure that 'a' points to enough memory to hold the result
 * (at least MAXINT8LEN+1 bytes, counting a leading sign and trailing NUL).
 */
void
pg_lltoa(int64 value, char *a)
{













































}

/*
 * pg_ltostr_zeropad
 *		Converts 'value' into a decimal string representation stored at
 *'str'. 'minwidth' specifies the minimum width of the result; any extra space
 *		is filled up by prefixing the number with zeros.
 *
 * Returns the ending address of the string result (the last character written
 * plus 1).  Note that no NUL terminator is written.
 *
 * The intended use-case for this function is to build strings that contain
 * multiple individual numbers, for example:
 *
 *	str = pg_ltostr_zeropad(str, hours, 2);
 *	*str++ = ':';
 *	str = pg_ltostr_zeropad(str, mins, 2);
 *	*str++ = ':';
 *	str = pg_ltostr_zeropad(str, secs, 2);
 *	*str = '\0';
 *
 * Note: Caller must ensure that 'str' points to enough memory to hold the
 * result.
 */
char *
pg_ltostr_zeropad(char *str, int32 value, int32 minwidth)
{
























































}

/*
 * pg_ltostr
 *		Converts 'value' into a decimal string representation stored at
 *'str'.
 *
 * Returns the ending address of the string result (the last character written
 * plus 1).  Note that no NUL terminator is written.
 *
 * The intended use-case for this function is to build strings that contain
 * multiple individual numbers, for example:
 *
 *	str = pg_ltostr(str, a);
 *	*str++ = ' ';
 *	str = pg_ltostr(str, b);
 *	*str = '\0';
 *
 * Note: Caller must ensure that 'str' points to enough memory to hold the
 * result.
 */
char *
pg_ltostr(char *str, int32 value)
{
























































}

/*
 * pg_strtouint64
 *		Converts 'str' into an unsigned 64-bit integer.
 *
 * This has the identical API to strtoul(3), except that it will handle
 * 64-bit ints even where "long" is narrower than that.
 *
 * For the moment it seems sufficient to assume that the platform has
 * such a function somewhere; let's not roll our own.
 */
uint64
pg_strtouint64(const char *str, char **endptr, int base)
{







}