/*-------------------------------------------------------------------------
 *
 * mac.c
 *	  PostgreSQL type definitions for 6 byte, EUI-48, MAC addresses.
 *
 * Portions Copyright (c) 1998-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  src/backend/utils/adt/mac.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "lib/hyperloglog.h"
#include "libpq/pqformat.h"
#include "port/pg_bswap.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/hashutils.h"
#include "utils/inet.h"
#include "utils/sortsupport.h"

/*
 *	Utility macros used for sorting and comparing:
 */

#define hibits(addr) ((unsigned long)(((addr)->a << 16) | ((addr)->b << 8) | ((addr)->c)))

#define lobits(addr) ((unsigned long)(((addr)->d << 16) | ((addr)->e << 8) | ((addr)->f)))

/* sortsupport for macaddr */
typedef struct
{
  int64 input_count; /* number of non-null values seen */
  bool estimating;   /* true if estimating cardinality */

  hyperLogLogState abbr_card; /* cardinality estimator */
} macaddr_sortsupport_state;

static int
macaddr_cmp_internal(macaddr *a1, macaddr *a2);
static int
macaddr_fast_cmp(Datum x, Datum y, SortSupport ssup);
static int
macaddr_cmp_abbrev(Datum x, Datum y, SortSupport ssup);
static bool
macaddr_abbrev_abort(int memtupcount, SortSupport ssup);
static Datum
macaddr_abbrev_convert(Datum original, SortSupport ssup);

/*
 *	MAC address reader.  Accepts several common notations.
 */

Datum
macaddr_in(PG_FUNCTION_ARGS)
{





















































}

/*
 *	MAC address output function.  Fixed format.
 */

Datum
macaddr_out(PG_FUNCTION_ARGS)
{








}

/*
 *		macaddr_recv			- converts external binary
 *format to macaddr
 *
 * The external representation is just the six bytes, MSB first.
 */
Datum
macaddr_recv(PG_FUNCTION_ARGS)
{













}

/*
 *		macaddr_send			- converts macaddr to binary
 *format
 */
Datum
macaddr_send(PG_FUNCTION_ARGS)
{











}

/*
 *	Comparison function for sorting:
 */

static int
macaddr_cmp_internal(macaddr *a1, macaddr *a2)
{




















}

Datum
macaddr_cmp(PG_FUNCTION_ARGS)
{




}

/*
 *	Boolean comparisons.
 */

Datum
macaddr_lt(PG_FUNCTION_ARGS)
{




}

Datum
macaddr_le(PG_FUNCTION_ARGS)
{




}

Datum
macaddr_eq(PG_FUNCTION_ARGS)
{




}

Datum
macaddr_ge(PG_FUNCTION_ARGS)
{




}

Datum
macaddr_gt(PG_FUNCTION_ARGS)
{




}

Datum
macaddr_ne(PG_FUNCTION_ARGS)
{




}

/*
 * Support function for hash indexes on macaddr.
 */
Datum
hashmacaddr(PG_FUNCTION_ARGS)
{



}

Datum
hashmacaddrextended(PG_FUNCTION_ARGS)
{



}

/*
 * Arithmetic functions: bitwise NOT, AND, OR.
 */
Datum
macaddr_not(PG_FUNCTION_ARGS)
{











}

Datum
macaddr_and(PG_FUNCTION_ARGS)
{












}

Datum
macaddr_or(PG_FUNCTION_ARGS)
{












}

/*
 *	Truncation function to allow comparing mac manufacturers.
 *	From suggestion by Alex Pilosov <alex@pilosoft.com>
 */
Datum
macaddr_trunc(PG_FUNCTION_ARGS)
{













}

/*
 * SortSupport strategy function. Populates a SortSupport struct with the
 * information necessary to use comparison by abbreviated keys.
 */
Datum
macaddr_sortsupport(PG_FUNCTION_ARGS)
{




























}

/*
 * SortSupport "traditional" comparison function. Pulls two MAC addresses from
 * the heap and runs a standard comparison on them.
 */
static int
macaddr_fast_cmp(Datum x, Datum y, SortSupport ssup)
{




}

/*
 * SortSupport abbreviated key comparison function. Compares two MAC addresses
 * quickly by treating them like integers, and without having to go to the
 * heap.
 */
static int
macaddr_cmp_abbrev(Datum x, Datum y, SortSupport ssup)
{












}

/*
 * Callback for estimating effectiveness of abbreviated key optimization.
 *
 * We pay no attention to the cardinality of the non-abbreviated data, because
 * there is no equality fast-path within authoritative macaddr comparator.
 */
static bool
macaddr_abbrev_abort(int memtupcount, SortSupport ssup)
{





















































}

/*
 * SortSupport conversion routine. Converts original macaddr representation
 * to abbreviated key representation.
 *
 * Packs the bytes of a 6-byte MAC address into a Datum and treats it as an
 * unsigned integer for purposes of comparison. On a 64-bit machine, there
 * will be two zeroed bytes of padding. The integer is converted to native
 * endianness to facilitate easy comparison.
 */
static Datum
macaddr_abbrev_convert(Datum original, SortSupport ssup)
{















































}