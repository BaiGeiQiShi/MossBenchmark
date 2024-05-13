/*-------------------------------------------------------------------------
 *
 * varbit.c
 *	  Functions for the SQL datatypes BIT() and BIT VARYING().
 *
 * The data structure contains the following elements:
 *   header  -- length of the whole data structure (incl header)
 *              in bytes (as with all varying length datatypes)
 *   data section -- private data section for the bits data structures
 *     bitlength -- length of the bit string in bits
 *     bitdata   -- bit string, most significant byte first
 *
 * The length of the bitdata vector should always be exactly as many
 * bytes as are needed for the given bitlength.  If the bitlength is
 * not a multiple of 8, the extra low-order padding bits of the last
 * byte must be zeroes.
 *
 * attypmod is defined as the length of the bit string in bits, or for
 * varying bits the maximum length.
 *
 * Code originally contributed by Adriaan Joubert.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/varbit.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/htup_details.h"
#include "common/int.h"
#include "libpq/pqformat.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/varbit.h"

#define HEXDIG(z) ((z) < 10 ? ((z) + '0') : ((z)-10 + 'A'))

/* Mask off any bits that should be zero in the last byte of a bitstring */
#define VARBIT_PAD(vb)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int32 pad_ = VARBITPAD(vb);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    Assert(pad_ >= 0 && pad_ < BITS_PER_BYTE);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    if (pad_ > 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      *(VARBITS(vb) + VARBITBYTES(vb) - 1) &= BITMASK << pad_;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
  } while (0)

/*
 * Many functions work byte-by-byte, so they have a pointer handy to the
 * last-plus-one byte, which saves a cycle or two.
 */
#define VARBIT_PAD_LAST(vb, ptr)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int32 pad_ = VARBITPAD(vb);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    Assert(pad_ >= 0 && pad_ < BITS_PER_BYTE);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    if (pad_ > 0)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                      \
      *((ptr)-1) &= BITMASK << pad_;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  } while (0)

/* Assert proper padding of a bitstring */
#ifdef USE_ASSERT_CHECKING
#define VARBIT_CORRECTLY_PADDED(vb)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    int32 pad_ = VARBITPAD(vb);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
    Assert(pad_ >= 0 && pad_ < BITS_PER_BYTE);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         \
    Assert(pad_ == 0 || (*(VARBITS(vb) + VARBITBYTES(vb) - 1) & ~(BITMASK << pad_)) == 0);                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                             \
  } while (0)
#else
#define VARBIT_CORRECTLY_PADDED(vb) ((void)0)
#endif

static VarBit *
bit_catenate(VarBit *arg1, VarBit *arg2);
static VarBit *
bitsubstring(VarBit *arg, int32 s, int32 l, bool length_not_specified);
static VarBit *
bit_overlay(VarBit *t1, VarBit *t2, int sp, int sl);

/*
 * common code for bittypmodin and varbittypmodin
 */
static int32
anybit_typmodin(ArrayType *ta, const char *typename)
{



























}

/*
 * common code for bittypmodout and varbittypmodout
 */
static char *
anybit_typmodout(int32 typmod)
{












}

/*
 * bit_in -
 *	  converts a char string to the internal representation of a bitstring.
 *		  The length is determined by the number of bits required plus
 *		  VARHDRSZ bytes or from atttypmod.
 */
Datum
bit_in(PG_FUNCTION_ARGS)
{







































































































































}

Datum
bit_out(PG_FUNCTION_ARGS)
{










































}

/*
 *		bit_recv			- converts external binary
 *format to bit
 */
Datum
bit_recv(PG_FUNCTION_ARGS)
{



































}

/*
 *		bit_send			- converts bit to binary format
 */
Datum
bit_send(PG_FUNCTION_ARGS)
{


}

/*
 * bit()
 * Converts a bit() type to a specific internal length.
 * len is the bitlength specified in the column definition.
 *
 * If doing implicit cast, raise error when source data is wrong length.
 * If doing explicit cast, silently truncate or zero-pad to specified length.
 */
Datum
bit(PG_FUNCTION_ARGS)
{

































}

Datum
bittypmodin(PG_FUNCTION_ARGS)
{



}

Datum
bittypmodout(PG_FUNCTION_ARGS)
{



}

/*
 * varbit_in -
 *	  converts a string to the internal representation of a bitstring.
 *		This is the same as bit_in except that atttypmod is taken as
 *		the maximum length, not the exact length to force the bitstring
 *to.
 */
Datum
varbit_in(PG_FUNCTION_ARGS)
{



































































































































}

/*
 * varbit_out -
 *	  Prints the string as bits to preserve length accurately
 *
 * XXX varbit_recv() and hex input to varbit_in() can load a value that this
 * cannot emit.  Consider using hex output for such values.
 */
Datum
varbit_out(PG_FUNCTION_ARGS)
{




































}

/*
 *		varbit_recv			- converts external binary
 *format to varbit
 *
 * External format is the bitlen as an int32, then the byte array.
 */
Datum
varbit_recv(PG_FUNCTION_ARGS)
{



































}

/*
 *		varbit_send			- converts varbit to binary
 *format
 */
Datum
varbit_send(PG_FUNCTION_ARGS)
{







}

/*
 * varbit_support()
 *
 * Planner support function for the varbit() length coercion function.
 *
 * Currently, the only interesting thing we can do is flatten calls that set
 * the new maximum length >= the previous maximum length.  We can ignore the
 * isExplicit argument, since that only affects truncation cases.
 */
Datum
varbit_support(PG_FUNCTION_ARGS)
{





























}

/*
 * varbit()
 * Converts a varbit() type to a specific internal length.
 * len is the maximum bitlength specified in the column definition.
 *
 * If doing implicit cast, raise error when source data is too long.
 * If doing explicit cast, silently truncate to max length.
 */
Datum
varbit(PG_FUNCTION_ARGS)
{




























}

Datum
varbittypmodin(PG_FUNCTION_ARGS)
{



}

Datum
varbittypmodout(PG_FUNCTION_ARGS)
{



}

/*
 * Comparison operators
 *
 * We only need one set of comparison operators for bitstrings, as the lengths
 * are stored in the same way for zero-padded and varying bit strings.
 *
 * Note that the standard is not unambiguous about the comparison between
 * zero-padded bit strings and varying bitstrings. If the same value is written
 * into a zero padded bitstring as into a varying bitstring, but the zero
 * padded bitstring has greater length, it will be bigger.
 *
 * Zeros from the beginning of a bitstring cannot simply be ignored, as they
 * may be part of a bit string and may be significant.
 *
 * Note: btree indexes need these routines not to leak memory; therefore,
 * be careful to free working copies of toasted datums.  Most places don't
 * need to be so careful.
 */

/*
 * bit_cmp
 *
 * Compares two bitstrings and returns <0, 0, >0 depending on whether the first
 * string is smaller, equal, or bigger than the second. All bits are considered
 * and additional zero bits may make one string smaller/larger than the other,
 * even if their zero-padded values would be the same.
 */
static int32
bit_cmp(VarBit *arg1, VarBit *arg2)
{

















}

Datum
biteq(PG_FUNCTION_ARGS)
{






















}

Datum
bitne(PG_FUNCTION_ARGS)
{






















}

Datum
bitlt(PG_FUNCTION_ARGS)
{










}

Datum
bitle(PG_FUNCTION_ARGS)
{










}

Datum
bitgt(PG_FUNCTION_ARGS)
{










}

Datum
bitge(PG_FUNCTION_ARGS)
{










}

Datum
bitcmp(PG_FUNCTION_ARGS)
{










}

/*
 * bitcat
 * Concatenation of bit strings
 */
Datum
bitcat(PG_FUNCTION_ARGS)
{




}

static VarBit *
bit_catenate(VarBit *arg1, VarBit *arg2)
{













































}

/*
 * bitsubstr
 * retrieve a substring from the bit string.
 * Note, s is 1-based.
 * SQL draft 6.10 9)
 */
Datum
bitsubstr(PG_FUNCTION_ARGS)
{

}

Datum
bitsubstr_no_len(PG_FUNCTION_ARGS)
{

}

static VarBit *
bitsubstring(VarBit *arg, int32 s, int32 l, bool length_not_specified)
{














































































}

/*
 * bitoverlay
 *	Replace specified substring of first string with second
 *
 * The SQL standard defines OVERLAY() in terms of substring and concatenation.
 * This code is a direct implementation of what the standard says.
 */
Datum
bitoverlay(PG_FUNCTION_ARGS)
{






}

Datum
bitoverlay_no_len(PG_FUNCTION_ARGS)
{







}

static VarBit *
bit_overlay(VarBit *t1, VarBit *t2, int sp, int sl)
{

























}

/*
 * bitlength, bitoctetlength
 * Return the length of a bit string
 */
Datum
bitlength(PG_FUNCTION_ARGS)
{



}

Datum
bitoctetlength(PG_FUNCTION_ARGS)
{



}

/*
 * bit_and
 * perform a logical AND on two bit strings.
 */
Datum
bit_and(PG_FUNCTION_ARGS)
{





























}

/*
 * bit_or
 * perform a logical OR on two bit strings.
 */
Datum
bit_or(PG_FUNCTION_ARGS)
{




























}

/*
 * bitxor
 * perform a logical XOR on two bit strings.
 */
Datum
bitxor(PG_FUNCTION_ARGS)
{





























}

/*
 * bitnot
 * perform a logical NOT on a bit string.
 */
Datum
bitnot(PG_FUNCTION_ARGS)
{



















}

/*
 * bitshiftleft
 * do a left shift (i.e. towards the beginning of the string)
 */
Datum
bitshiftleft(PG_FUNCTION_ARGS)
{



























































}

/*
 * bitshiftright
 * do a right shift (i.e. towards the end of the string)
 */
Datum
bitshiftright(PG_FUNCTION_ARGS)
{
































































}

/*
 * This is not defined in any standard. We retain the natural ordering of
 * bits here, as it just seems more intuitive.
 */
Datum
bitfromint4(PG_FUNCTION_ARGS)
{























































}

Datum
bittoint4(PG_FUNCTION_ARGS)
{




















}

Datum
bitfromint8(PG_FUNCTION_ARGS)
{























































}

Datum
bittoint8(PG_FUNCTION_ARGS)
{




















}

/*
 * Determines the position of S2 in the bitstring S1 (1-based string).
 * If S2 does not appear in S1 this function returns 0.
 * If S2 is of length 0 this function returns 1.
 * Compatible in usage with POSITION() functions for other data types.
 */
Datum
bitposition(PG_FUNCTION_ARGS)
{
































































































}

/*
 * bitsetbit
 *
 * Given an instance of type 'bit' creates a new one with
 * the Nth bit set to the given value.
 *
 * The bit location is specified left-to-right in a zero-based fashion
 * consistent with the other get_bit and set_bit functions, but
 * inconsistent with the standard substring, position, overlay functions
 */
Datum
bitsetbit(PG_FUNCTION_ARGS)
{
















































}

/*
 * bitgetbit
 *
 * returns the value of the Nth bit of a bit array (0 or 1).
 *
 * The bit location is specified left-to-right in a zero-based fashion
 * consistent with the other get_bit and set_bit functions, but
 * inconsistent with the standard substring, position, overlay functions
 */
Datum
bitgetbit(PG_FUNCTION_ARGS)
{

























}