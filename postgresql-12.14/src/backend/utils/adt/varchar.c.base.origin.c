/*-------------------------------------------------------------------------
 *
 * varchar.c
 *	  Functions for the built-in types char(n) and varchar(n).
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/varchar.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/tuptoaster.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "libpq/pqformat.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/hashutils.h"
#include "utils/lsyscache.h"
#include "utils/pg_locale.h"
#include "utils/varlena.h"
#include "mb/pg_wchar.h"

/* common code for bpchartypmodin and varchartypmodin */
static int32
anychar_typmodin(ArrayType *ta, const char *typename)
{
































}

/* common code for bpchartypmodout and varchartypmodout */
static char *
anychar_typmodout(int32 typmod)
{












}

/*
 * CHAR() and VARCHAR() types are part of the SQL standard. CHAR()
 * is for blank-padded string whose length is specified in CREATE TABLE.
 * VARCHAR is for storing string whose length is at most the length specified
 * at CREATE TABLE time.
 *
 * It's hard to implement these types because we cannot figure out
 * the length of the type from the type itself. I changed (hopefully all) the
 * fmgr calls that invoke input functions of a data type to supply the
 * length also. (eg. in INSERTs, we have the tupleDescriptor which contains
 * the length of the attributes and hence the exact length of the char() or
 * varchar(). We pass this to bpcharin() or varcharin().) In the case where
 * we cannot determine the length, we pass in -1 instead and the input
 * converter does not enforce any length check.
 *
 * We actually implement this as a varlena so that we don't have to pass in
 * the length for the comparison functions. (The difference between these
 * types and "text" is that we truncate and possibly blank-pad the string
 * at insertion time.)
 *
 *															  -
 *ay 6/95
 */

/*****************************************************************************
 *	 bpchar - char()
 **
 *****************************************************************************/

/*
 * bpchar_input -- common guts of bpcharin and bpcharrecv
 *
 * s is the input text of length len (may not be null-terminated)
 * atttypmod is the typmod value to apply
 *
 * Note that atttypmod is measured in characters, which
 * is not necessarily the same as the number of bytes.
 *
 * If the input string is too long, raise an error, unless the extra
 * characters are spaces, in which case they're truncated.  (per SQL)
 */
static BpChar *
bpchar_input(const char *s, size_t len, int32 atttypmod)
{






























































}

/*
 * Convert a C string to CHARACTER internal representation.  atttypmod
 * is the declared length of the type plus VARHDRSZ.
 */
Datum
bpcharin(PG_FUNCTION_ARGS)
{










}

/*
 * Convert a CHARACTER value to a C string.
 *
 * Uses the text conversion functions, which is only appropriate if BpChar
 * and text are equivalent types.
 */
Datum
bpcharout(PG_FUNCTION_ARGS)
{



}

/*
 *		bpcharrecv			- converts external binary
 *format to bpchar
 */
Datum
bpcharrecv(PG_FUNCTION_ARGS)
{














}

/*
 *		bpcharsend			- converts bpchar to binary
 *format
 */
Datum
bpcharsend(PG_FUNCTION_ARGS)
{


}

/*
 * Converts a CHARACTER type to the specified size.
 *
 * maxlen is the typmod, ie, declared length plus VARHDRSZ bytes.
 * isExplicit is true if this is for an explicit cast to char(N).
 *
 * Truncation rules: for an explicit cast, silently truncate to the given
 * length; for an implicit cast, raise error unless extra characters are
 * all spaces.  (This is sort-of per SQL: the spec would actually have us
 * raise a "completion condition" for the explicit cast case, but Postgres
 * hasn't got such a concept.)
 */
Datum
bpchar(PG_FUNCTION_ARGS)
{
















































































}

/* char_bpchar()
 * Convert char to bpchar(1).
 */
Datum
char_bpchar(PG_FUNCTION_ARGS)
{









}

/* bpchar_name()
 * Converts a bpchar() type to a NameData type.
 */
Datum
bpchar_name(PG_FUNCTION_ARGS)
{





























}

/* name_bpchar()
 * Converts a NameData type to a bpchar type.
 *
 * Uses the text conversion functions, which is only appropriate if BpChar
 * and text are equivalent types.
 */
Datum
name_bpchar(PG_FUNCTION_ARGS)
{





}

Datum
bpchartypmodin(PG_FUNCTION_ARGS)
{



}

Datum
bpchartypmodout(PG_FUNCTION_ARGS)
{



}

/*****************************************************************************
 *	 varchar - varchar(n)
 *
 * Note: varchar piggybacks on type text for most operations, and so has no
 * C-coded functions except for I/O and typmod checking.
 *****************************************************************************/

/*
 * varchar_input -- common guts of varcharin and varcharrecv
 *
 * s is the input text of length len (may not be null-terminated)
 * atttypmod is the typmod value to apply
 *
 * Note that atttypmod is measured in characters, which
 * is not necessarily the same as the number of bytes.
 *
 * If the input string is too long, raise an error, unless the extra
 * characters are spaces, in which case they're truncated.  (per SQL)
 *
 * Uses the C string to text conversion function, which is only appropriate
 * if VarChar and text are equivalent types.
 */
static VarChar *
varchar_input(const char *s, size_t len, int32 atttypmod)
{
























}

/*
 * Convert a C string to VARCHAR internal representation.  atttypmod
 * is the declared length of the type plus VARHDRSZ.
 */
Datum
varcharin(PG_FUNCTION_ARGS)
{










}

/*
 * Convert a VARCHAR value to a C string.
 *
 * Uses the text to C string conversion function, which is only appropriate
 * if VarChar and text are equivalent types.
 */
Datum
varcharout(PG_FUNCTION_ARGS)
{



}

/*
 *		varcharrecv			- converts external binary
 *format to varchar
 */
Datum
varcharrecv(PG_FUNCTION_ARGS)
{














}

/*
 *		varcharsend			- converts varchar to binary
 *format
 */
Datum
varcharsend(PG_FUNCTION_ARGS)
{


}

/*
 * varchar_support()
 *
 * Planner support function for the varchar() length coercion function.
 *
 * Currently, the only interesting thing we can do is flatten calls that set
 * the new maximum length >= the previous maximum length.  We can ignore the
 * isExplicit argument, since that only affects truncation cases.
 */
Datum
varchar_support(PG_FUNCTION_ARGS)
{





























}

/*
 * Converts a VARCHAR type to the specified size.
 *
 * maxlen is the typmod, ie, declared length plus VARHDRSZ bytes.
 * isExplicit is true if this is for an explicit cast to varchar(N).
 *
 * Truncation rules: for an explicit cast, silently truncate to the given
 * length; for an implicit cast, raise error unless extra characters are
 * all spaces.  (This is sort-of per SQL: the spec would actually have us
 * raise a "completion condition" for the explicit cast case, but Postgres
 * hasn't got such a concept.)
 */
Datum
varchar(PG_FUNCTION_ARGS)
{



































}

Datum
varchartypmodin(PG_FUNCTION_ARGS)
{



}

Datum
varchartypmodout(PG_FUNCTION_ARGS)
{



}

/*****************************************************************************
 * Exported functions
 *****************************************************************************/

/* "True" length (not counting trailing blanks) of a BpChar */
static inline int
bcTruelen(BpChar *arg)
{

}

int
bpchartruelen(char *s, int len)
{














}

Datum
bpcharlen(PG_FUNCTION_ARGS)
{













}

Datum
bpcharoctetlen(PG_FUNCTION_ARGS)
{




}

/*****************************************************************************
 *	Comparison Functions used for bpchar
 *
 * Note: btree indexes need these routines not to leak memory; therefore,
 * be careful to free working copies of toasted datums.  Most places don't
 * need to be so careful.
 *****************************************************************************/

static void
check_collation_set(Oid collid)
{








}

Datum
bpchareq(PG_FUNCTION_ARGS)
{



































}

Datum
bpcharne(PG_FUNCTION_ARGS)
{



































}

Datum
bpcharlt(PG_FUNCTION_ARGS)
{














}

Datum
bpcharle(PG_FUNCTION_ARGS)
{














}

Datum
bpchargt(PG_FUNCTION_ARGS)
{














}

Datum
bpcharge(PG_FUNCTION_ARGS)
{














}

Datum
bpcharcmp(PG_FUNCTION_ARGS)
{














}

Datum
bpchar_sortsupport(PG_FUNCTION_ARGS)
{












}

Datum
bpchar_larger(PG_FUNCTION_ARGS)
{











}

Datum
bpchar_smaller(PG_FUNCTION_ARGS)
{











}

/*
 * bpchar needs a specialized hash function because we want to ignore
 * trailing blanks in comparisons.
 */
Datum
hashbpchar(PG_FUNCTION_ARGS)
{























































}

Datum
hashbpcharextended(PG_FUNCTION_ARGS)
{






















































}

/*
 * The following operators support character-by-character comparison
 * of bpchar datums, to allow building indexes suitable for LIKE clauses.
 * Note that the regular bpchareq/bpcharne comparison operators, and
 * regular support functions 1 and 2 with "C" collation are assumed to be
 * compatible with these!
 */

static int
internal_bpchar_pattern_compare(BpChar *arg1, BpChar *arg2)
{























}

Datum
bpchar_pattern_lt(PG_FUNCTION_ARGS)
{










}

Datum
bpchar_pattern_le(PG_FUNCTION_ARGS)
{










}

Datum
bpchar_pattern_ge(PG_FUNCTION_ARGS)
{










}

Datum
bpchar_pattern_gt(PG_FUNCTION_ARGS)
{










}

Datum
btbpchar_pattern_cmp(PG_FUNCTION_ARGS)
{










}

Datum
btbpchar_pattern_sortsupport(PG_FUNCTION_ARGS)
{











}