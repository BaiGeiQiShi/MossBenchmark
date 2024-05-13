/*-------------------------------------------------------------------------
 *
 * varlena.c
 *	  Functions for the variable-length built-in types.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/varlena.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>
#include <limits.h>

#include "access/tuptoaster.h"
#include "catalog/pg_collation.h"
#include "catalog/pg_type.h"
#include "common/int.h"
#include "lib/hyperloglog.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "parser/scansup.h"
#include "port/pg_bswap.h"
#include "regex/regex.h"
#include "utils/builtins.h"
#include "utils/bytea.h"
#include "utils/hashutils.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/pg_locale.h"
#include "utils/sortsupport.h"
#include "utils/varlena.h"

/* GUC variable */
int bytea_output = BYTEA_OUTPUT_HEX;

typedef struct varlena unknown;
typedef struct varlena VarString;

/*
 * State for text_position_* functions.
 */
typedef struct
{
  bool is_multibyte; /* T if multibyte encoding */
  bool is_multibyte_char_in_char;

  char *str1; /* haystack string */
  char *str2; /* needle string */
  int len1;   /* string lengths in bytes */
  int len2;

  /* Skip table for Boyer-Moore-Horspool search algorithm: */
  int skiptablemask;  /* mask for ANDing with skiptable subscripts */
  int skiptable[256]; /* skip distance for given mismatched char */

  char *last_match; /* pointer to last match in 'str1' */

  /*
   * Sometimes we need to convert the byte position of a match to a
   * character position.  These store the last position that was converted,
   * so that on the next call, we can continue from that point, rather than
   * count characters from the very beginning.
   */
  char *refpoint; /* pointer within original haystack string */
  int refpos;     /* 0-based character offset of the same point */
} TextPositionState;

typedef struct
{
  char *buf1;        /* 1st string, or abbreviation original string
                      * buf */
  char *buf2;        /* 2nd string, or abbreviation strxfrm() buf */
  int buflen1;       /* Allocated length of buf1 */
  int buflen2;       /* Allocated length of buf2 */
  int last_len1;     /* Length of last buf1 string/strxfrm() input */
  int last_len2;     /* Length of last buf2 string/strxfrm() blob */
  int last_returned; /* Last comparison result (cache) */
  bool cache_blob;   /* Does buf2 contain strxfrm() blob, etc? */
  bool collate_c;
  Oid typid;                  /* Actual datatype (text/bpchar/bytea/name) */
  hyperLogLogState abbr_card; /* Abbreviated key cardinality state */
  hyperLogLogState full_card; /* Full key cardinality state */
  double prop_card;           /* Required cardinality proportion */
  pg_locale_t locale;
} VarStringSortSupport;

/*
 * This should be large enough that most strings will fit, but small enough
 * that we feel comfortable putting it on the stack
 */
#define TEXTBUFLEN 1024

#define DatumGetUnknownP(X) ((unknown *)PG_DETOAST_DATUM(X))
#define DatumGetUnknownPCopy(X) ((unknown *)PG_DETOAST_DATUM_COPY(X))
#define PG_GETARG_UNKNOWN_P(n) DatumGetUnknownP(PG_GETARG_DATUM(n))
#define PG_GETARG_UNKNOWN_P_COPY(n) DatumGetUnknownPCopy(PG_GETARG_DATUM(n))
#define PG_RETURN_UNKNOWN_P(x) PG_RETURN_POINTER(x)

#define DatumGetVarStringP(X) ((VarString *)PG_DETOAST_DATUM(X))
#define DatumGetVarStringPP(X) ((VarString *)PG_DETOAST_DATUM_PACKED(X))

static int
varstrfastcmp_c(Datum x, Datum y, SortSupport ssup);
static int
bpcharfastcmp_c(Datum x, Datum y, SortSupport ssup);
static int
namefastcmp_c(Datum x, Datum y, SortSupport ssup);
static int
varlenafastcmp_locale(Datum x, Datum y, SortSupport ssup);
static int
namefastcmp_locale(Datum x, Datum y, SortSupport ssup);
static int
varstrfastcmp_locale(char *a1p, int len1, char *a2p, int len2, SortSupport ssup);
static int
varstrcmp_abbrev(Datum x, Datum y, SortSupport ssup);
static Datum
varstr_abbrev_convert(Datum original, SortSupport ssup);
static bool
varstr_abbrev_abort(int memtupcount, SortSupport ssup);
static int32
text_length(Datum str);
static text *
text_catenate(text *t1, text *t2);
static text *
text_substring(Datum str, int32 start, int32 length, bool length_not_specified);
static text *
text_overlay(text *t1, text *t2, int sp, int sl);
static int
text_position(text *t1, text *t2, Oid collid);
static void
text_position_setup(text *t1, text *t2, Oid collid, TextPositionState *state);
static bool
text_position_next(TextPositionState *state);
static char *
text_position_next_internal(char *start_ptr, TextPositionState *state);
static char *
text_position_get_match_ptr(TextPositionState *state);
static int
text_position_get_match_pos(TextPositionState *state);
static void
text_position_cleanup(TextPositionState *state);
static void
check_collation_set(Oid collid);
static int
text_cmp(text *arg1, text *arg2, Oid collid);
static bytea *
bytea_catenate(bytea *t1, bytea *t2);
static bytea *
bytea_substring(Datum str, int S, int L, bool length_not_specified);
static bytea *
bytea_overlay(bytea *t1, bytea *t2, int sp, int sl);
static void
appendStringInfoText(StringInfo str, const text *t);
static Datum text_to_array_internal(PG_FUNCTION_ARGS);
static text *
array_to_text_internal(FunctionCallInfo fcinfo, ArrayType *v, const char *fldsep, const char *null_string);
static StringInfo
makeStringAggState(FunctionCallInfo fcinfo);
static bool
text_format_parse_digits(const char **ptr, const char *end_ptr, int *value);
static const char *
text_format_parse_format(const char *start_ptr, const char *end_ptr, int *argpos, int *widthpos, int *flags, int *width);
static void
text_format_string_conversion(StringInfo buf, char conversion, FmgrInfo *typOutputInfo, Datum value, bool isNull, int flags, int width);
static void
text_format_append_string(StringInfo buf, const char *str, int flags, int width);

/*****************************************************************************
 *	 CONVERSION ROUTINES EXPORTED FOR USE BY C CODE
 **
 *****************************************************************************/

/*
 * cstring_to_text
 *
 * Create a text value from a null-terminated C string.
 *
 * The new text value is freshly palloc'd with a full-size VARHDR.
 */
text *
cstring_to_text(const char *s)
{

}

/*
 * cstring_to_text_with_len
 *
 * Same as cstring_to_text except the caller specifies the string length;
 * the string need not be null_terminated.
 */
text *
cstring_to_text_with_len(const char *s, int len)
{






}

/*
 * text_to_cstring
 *
 * Create a palloc'd, null-terminated C string from a text value.
 *
 * We support being passed a compressed or toasted text value.
 * This is a bit bogus since such values shouldn't really be referred to as
 * "text *", but it seems useful for robustness.  If we didn't handle that
 * case here, we'd need another routine that did, anyway.
 */
char *
text_to_cstring(const text *t)
{















}

/*
 * text_to_cstring_buffer
 *
 * Copy a text value into a caller-supplied buffer of size dst_len.
 *
 * The text string is truncated if necessary to fit.  The result is
 * guaranteed null-terminated (unless dst_len == 0).
 *
 * We support being passed a compressed or toasted text value.
 * This is a bit bogus since such values shouldn't really be referred to as
 * "text *", but it seems useful for robustness.  If we didn't handle that
 * case here, we'd need another routine that did, anyway.
 */
void
text_to_cstring_buffer(const text *src, char *dst, size_t dst_len)
{























}

/*****************************************************************************
 *	 USER I/O ROUTINES
 **
 *****************************************************************************/

#define VAL(CH) ((CH) - '0')
#define DIG(VAL) ((VAL) + '0')

/*
 *		byteain			- converts from printable representation
 *of byte array
 *
 *		Non-printable characters must be passed as '\nnn' (octal) and
 *are converted to internal form.  '\' must be passed as '\\'. ereport(ERROR,
 *...) if bad form.
 *
 *		BUGS:
 *				The input is scanned twice.
 *				The error checking of input is minimal.
 */
Datum
byteain(PG_FUNCTION_ARGS)
{

















































































}

/*
 *		byteaout		- converts to printable representation
 *of byte array
 *
 *		In the traditional escaped format, non-printable characters are
 *		printed as '\nnn' (octal) and '\' as '\\'.
 */
Datum
byteaout(PG_FUNCTION_ARGS)
{







































































}

/*
 *		bytearecv			- converts external binary
 *format to bytea
 */
Datum
bytearecv(PG_FUNCTION_ARGS)
{









}

/*
 *		byteasend			- converts bytea to binary
 *format
 *
 * This is a special case: just copy the input...
 */
Datum
byteasend(PG_FUNCTION_ARGS)
{



}

Datum
bytea_string_agg_transfn(PG_FUNCTION_ARGS)
{





























}

Datum
bytea_string_agg_finalfn(PG_FUNCTION_ARGS)
{




















}

/*
 *		textin			- converts "..." to internal
 *representation
 */
Datum
textin(PG_FUNCTION_ARGS)
{



}

/*
 *		textout			- converts internal representation to
 *"..."
 */
Datum
textout(PG_FUNCTION_ARGS)
{



}

/*
 *		textrecv			- converts external binary
 *format to text
 */
Datum
textrecv(PG_FUNCTION_ARGS)
{










}

/*
 *		textsend			- converts text to binary format
 */
Datum
textsend(PG_FUNCTION_ARGS)
{






}

/*
 *		unknownin			- converts "..." to internal
 *representation
 */
Datum
unknownin(PG_FUNCTION_ARGS)
{




}

/*
 *		unknownout			- converts internal
 *representation to "..."
 */
Datum
unknownout(PG_FUNCTION_ARGS)
{




}

/*
 *		unknownrecv			- converts external binary
 *format to unknown
 */
Datum
unknownrecv(PG_FUNCTION_ARGS)
{







}

/*
 *		unknownsend			- converts unknown to binary
 *format
 */
Datum
unknownsend(PG_FUNCTION_ARGS)
{







}

/* ========== PUBLIC ROUTINES ========== */

/*
 * textlen -
 *	  returns the logical length of a text*
 *	   (which is less than the VARSIZE of the text*)
 */
Datum
textlen(PG_FUNCTION_ARGS)
{




}

/*
 * text_length -
 *	Does the real work for textlen()
 *
 *	This is broken out so it can be called directly by other string
 *processing functions.  Note that the argument is passed as a Datum, to
 *indicate that it may still be in compressed form.  We can avoid decompressing
 *it at all in some cases.
 */
static int32
text_length(Datum str)
{











}

/*
 * textoctetlen -
 *	  returns the physical length of a text*
 *	   (which is less than the VARSIZE of the text*)
 */
Datum
textoctetlen(PG_FUNCTION_ARGS)
{




}

/*
 * textcat -
 *	  takes two text* and returns a text* that is the concatenation of
 *	  the two.
 *
 * Rewritten by Sapa, sapa@hq.icb.chel.su. 8-Jul-96.
 * Updated by Thomas, Thomas.Lockhart@jpl.nasa.gov 1997-07-10.
 * Allocate space for output in all cases.
 * XXX - thomas 1997-07-10
 */
Datum
textcat(PG_FUNCTION_ARGS)
{




}

/*
 * text_catenate
 *	Guts of textcat(), broken out so it can be used by other functions
 *
 * Arguments can be in short-header form, but not compressed or out-of-line
 */
static text *
text_catenate(text *t1, text *t2)
{



































}

/*
 * charlen_to_bytelen()
 *	Compute the number of bytes occupied by n characters starting at *p
 *
 * It is caller's responsibility that there actually are n characters;
 * the string need not be null-terminated.
 */
static int
charlen_to_bytelen(const char *p, int n)
{
















}

/*
 * text_substr()
 * Return a substring starting at the specified position.
 * - thomas 1997-12-31
 *
 * Input:
 *	- string
 *	- starting position (is one-based)
 *	- string length
 *
 * If the starting position is zero or less, then return from the start of the
 *string adjusting the length to be consistent with the "negative start" per
 *SQL. If the length is less than zero, return the remaining string.
 *
 * Added multibyte support.
 * - Tatsuo Ishii 1998-4-21
 * Changed behavior if starting position is less than one to conform to SQL
 *behavior. Formerly returned the entire string; now returns a portion.
 * - Thomas Lockhart 1998-12-10
 * Now uses faster TOAST-slicing interface
 * - John Gray 2002-02-22
 * Remove "#ifdef MULTIBYTE" and test for encoding_max_length instead. Change
 * behaviors conflicting with SQL to meet SQL (if E = S + L < S throw
 * error; if E < 1, return '', not entire string). Fixed MB related bug when
 * S > LC and < LC + 4 sometimes garbage characters are returned.
 * - Joe Conway 2002-08-10
 */
Datum
text_substr(PG_FUNCTION_ARGS)
{

}

/*
 * text_substr_no_len -
 *	  Wrapper to avoid opr_sanity failure due to
 *	  one function accepting a different number of args.
 */
Datum
text_substr_no_len(PG_FUNCTION_ARGS)
{

}

/*
 * text_substring -
 *	Does the real work for text_substr() and text_substr_no_len()
 *
 *	This is broken out so it can be called directly by other string
 *processing functions.  Note that the argument is passed as a Datum, to
 *indicate that it may still be in compressed/toasted form.  We can avoid
 *detoasting all of it in some cases.
 *
 *	The result is always a freshly palloc'd datum.
 */
static text *
text_substring(Datum str, int32 start, int32 length, bool length_not_specified)
{



























































































































































































































}

/*
 * textoverlay
 *	Replace specified substring of first string with second
 *
 * The SQL standard defines OVERLAY() in terms of substring and concatenation.
 * This code is a direct implementation of what the standard says.
 */
Datum
textoverlay(PG_FUNCTION_ARGS)
{






}

Datum
textoverlay_no_len(PG_FUNCTION_ARGS)
{







}

static text *
text_overlay(text *t1, text *t2, int sp, int sl)
{

























}

/*
 * textpos -
 *	  Return the position of the specified substring.
 *	  Implements the SQL POSITION() function.
 *	  Ref: A Guide To The SQL Standard, Date & Darwen, 1997
 * - thomas 1997-07-27
 */
Datum
textpos(PG_FUNCTION_ARGS)
{




}

/*
 * text_position -
 *	Does the real work for textpos()
 *
 * Inputs:
 *		t1 - string to be searched
 *		t2 - pattern to match within t1
 * Result:
 *		Character index of the first matched char, starting from 1,
 *		or 0 if no match.
 *
 *	This is broken out so it can be called directly by other string
 *processing functions.
 */
static int
text_position(text *t1, text *t2, Oid collid)
{


























}

/*
 * text_position_setup, text_position_next, text_position_cleanup -
 *	Component steps of text_position()
 *
 * These are broken out so that a string can be efficiently searched for
 * multiple occurrences of the same pattern.  text_position_next may be
 * called multiple times, and it advances to the next match on each call.
 * text_position_get_match_ptr() and text_position_get_match_pos() return
 * a pointer or 1-based character position of the last match, respectively.
 *
 * The "state" variable is normally just a local variable in the caller.
 *
 * NOTE: text_position_next skips over the matched portion.  For example,
 * searching for "xx" in "xxx" returns only one match, not two.
 */

static void
text_position_setup(text *t1, text *t2, Oid collid, TextPositionState *state)
{








































































































































}

/*
 * Advance to the next match, starting from the end of the previous match
 * (or the beginning of the string, on first call).  Returns true if a match
 * is found.
 *
 * Note that this refuses to match an empty-string needle.  Most callers
 * will have handled that case specially and we'll never see it here.
 */
static bool
text_position_next(TextPositionState *state)
{






























































}

/*
 * Subroutine of text_position_next().  This searches for the raw byte
 * sequence, ignoring any multi-byte encoding issues.  Returns the first
 * match starting at 'start_ptr', or NULL if no match is found.
 */
static char *
text_position_next_internal(char *start_ptr, TextPositionState *state)
{






























































}

/*
 * Return a pointer to the current match.
 *
 * The returned pointer points into correct position in the original
 * the haystack string.
 */
static char *
text_position_get_match_ptr(TextPositionState *state)
{

}

/*
 * Return the offset of the current match.
 *
 * The offset is in characters, 1-based.
 */
static int
text_position_get_match_pos(TextPositionState *state)
{















}

static void
text_position_cleanup(TextPositionState *state)
{

}

static void
check_collation_set(Oid collid)
{








}

/* varstr_cmp()
 * Comparison function for text strings with given lengths.
 * Includes locale support, but must copy strings to temporary memory
 *	to allow null-termination for inputs to strcoll().
 * Returns an integer less than, equal to, or greater than zero, indicating
 * whether arg1 is less than, equal to, or greater than arg2.
 *
 * Note: many functions that depend on this are marked leakproof; therefore,
 * avoid reporting the actual contents of the input when throwing errors.
 * All errors herein should be things that can't happen except on corrupt
 * data, anyway; otherwise we will have trouble with indexing strings that
 * would cause them.
 */
int
varstr_cmp(const char *arg1, int len1, const char *arg2, int len2, Oid collid)
{






































































































































































































































}

/* text_cmp()
 * Internal comparison function for text strings.
 * Returns -1, 0 or 1
 */
static int
text_cmp(text *arg1, text *arg2, Oid collid)
{










}

/*
 * Comparison functions for text strings.
 *
 * Note: btree indexes need these routines not to leak memory; therefore,
 * be careful to free working copies of toasted datums.  Most places don't
 * need to be so careful.
 */

Datum
texteq(PG_FUNCTION_ARGS)
{















































}

Datum
textne(PG_FUNCTION_ARGS)
{









































}

Datum
text_lt(PG_FUNCTION_ARGS)
{










}

Datum
text_le(PG_FUNCTION_ARGS)
{










}

Datum
text_gt(PG_FUNCTION_ARGS)
{










}

Datum
text_ge(PG_FUNCTION_ARGS)
{










}

Datum
text_starts_with(PG_FUNCTION_ARGS)
{





































}

Datum
bttextcmp(PG_FUNCTION_ARGS)
{










}

Datum
bttextsortsupport(PG_FUNCTION_ARGS)
{












}

/*
 * Generic sortsupport interface for character type's operator classes.
 * Includes locale support, and support for BpChar semantics (i.e. removing
 * trailing spaces before comparison).
 *
 * Relies on the assumption that text, VarChar, BpChar, and bytea all have the
 * same representation.  Callers that always use the C collation (e.g.
 * non-collatable type callers like bytea) may have NUL bytes in their strings;
 * this will not work with any other collation, though.
 */
void
varstr_sortsupport(SortSupport ssup, Oid typid, Oid collid)
{





































































































































































}

/*
 * sortsupport comparison func (for C locale case)
 */
static int
varstrfastcmp_c(Datum x, Datum y, SortSupport ssup)
{




























}

/*
 * sortsupport comparison func (for BpChar C locale case)
 *
 * BpChar outsources its sortsupport to this module.  Specialization for the
 * varstr_sortsupport BpChar case, modeled on
 * internal_bpchar_pattern_compare().
 */
static int
bpcharfastcmp_c(Datum x, Datum y, SortSupport ssup)
{




























}

/*
 * sortsupport comparison func (for NAME C locale case)
 */
static int
namefastcmp_c(Datum x, Datum y, SortSupport ssup)
{




}

/*
 * sortsupport comparison func (for locale case with all varlena types)
 */
static int
varlenafastcmp_locale(Datum x, Datum y, SortSupport ssup)
{
























}

/*
 * sortsupport comparison func (for locale case with NAME type)
 */
static int
namefastcmp_locale(Datum x, Datum y, SortSupport ssup)
{




}

/*
 * sortsupport comparison func for locale cases
 */
static int
varstrfastcmp_locale(char *a1p, int len1, char *a2p, int len2, SortSupport ssup)
{











































































































































}

/*
 * Abbreviated key comparison func
 */
static int
varstrcmp_abbrev(Datum x, Datum y, SortSupport ssup)
{



















}

/*
 * Conversion routine for sortsupport.  Converts original to abbreviated key
 * representation.  Our encoding strategy is simple -- pack the first 8 bytes
 * of a strxfrm() blob into a Datum (on little-endian machines, the 8 bytes are
 * stored in reverse order), and treat it as an unsigned integer.  When the "C"
 * locale is used, or in case of bytea, just memcpy() from original instead.
 */
static Datum
varstr_abbrev_convert(Datum original, SortSupport ssup)
{







































































































































































































































}

/*
 * Callback for estimating effectiveness of abbreviated key optimization, using
 * heuristic rules.  Returns value indicating if the abbreviation optimization
 * should be aborted, based on its projected effectiveness.
 */
static bool
varstr_abbrev_abort(int memtupcount, SortSupport ssup)
{

















































































































}

Datum
text_larger(PG_FUNCTION_ARGS)
{







}

Datum
text_smaller(PG_FUNCTION_ARGS)
{







}

/*
 * Cross-type comparison functions for types text and name.
 */

Datum
nameeqtext(PG_FUNCTION_ARGS)
{





















}

Datum
texteqname(PG_FUNCTION_ARGS)
{





















}

Datum
namenetext(PG_FUNCTION_ARGS)
{





















}

Datum
textnename(PG_FUNCTION_ARGS)
{





















}

Datum
btnametextcmp(PG_FUNCTION_ARGS)
{









}

Datum
bttextnamecmp(PG_FUNCTION_ARGS)
{









}

#define CmpCall(cmpfunc) DatumGetInt32(DirectFunctionCall2Coll(cmpfunc, PG_GET_COLLATION(), PG_GETARG_DATUM(0), PG_GETARG_DATUM(1)))

Datum
namelttext(PG_FUNCTION_ARGS)
{

}

Datum
nameletext(PG_FUNCTION_ARGS)
{

}

Datum
namegttext(PG_FUNCTION_ARGS)
{

}

Datum
namegetext(PG_FUNCTION_ARGS)
{

}

Datum
textltname(PG_FUNCTION_ARGS)
{

}

Datum
textlename(PG_FUNCTION_ARGS)
{

}

Datum
textgtname(PG_FUNCTION_ARGS)
{

}

Datum
textgename(PG_FUNCTION_ARGS)
{

}

#undef CmpCall

/*
 * The following operators support character-by-character comparison
 * of text datums, to allow building indexes suitable for LIKE clauses.
 * Note that the regular texteq/textne comparison operators, and regular
 * support functions 1 and 2 with "C" collation are assumed to be
 * compatible with these!
 */

static int
internal_text_pattern_compare(text *arg1, text *arg2)
{























}

Datum
text_pattern_lt(PG_FUNCTION_ARGS)
{










}

Datum
text_pattern_le(PG_FUNCTION_ARGS)
{










}

Datum
text_pattern_ge(PG_FUNCTION_ARGS)
{










}

Datum
text_pattern_gt(PG_FUNCTION_ARGS)
{










}

Datum
bttext_pattern_cmp(PG_FUNCTION_ARGS)
{










}

Datum
bttext_pattern_sortsupport(PG_FUNCTION_ARGS)
{











}

/*-------------------------------------------------------------
 * byteaoctetlen
 *
 * get the number of bytes contained in an instance of type 'bytea'
 *-------------------------------------------------------------
 */
Datum
byteaoctetlen(PG_FUNCTION_ARGS)
{




}

/*
 * byteacat -
 *	  takes two bytea* and returns a bytea* that is the concatenation of
 *	  the two.
 *
 * Cloned from textcat and modified as required.
 */
Datum
byteacat(PG_FUNCTION_ARGS)
{




}

/*
 * bytea_catenate
 *	Guts of byteacat(), broken out so it can be used by other functions
 *
 * Arguments can be in short-header form, but not compressed or out-of-line
 */
static bytea *
bytea_catenate(bytea *t1, bytea *t2)
{



































}

#define PG_STR_GET_BYTEA(str_) DatumGetByteaPP(DirectFunctionCall1(byteain, CStringGetDatum(str_)))

/*
 * bytea_substr()
 * Return a substring starting at the specified position.
 * Cloned from text_substr and modified as required.
 *
 * Input:
 *	- string
 *	- starting position (is one-based)
 *	- string length (optional)
 *
 * If the starting position is zero or less, then return from the start of the
 *string adjusting the length to be consistent with the "negative start" per
 *SQL. If the length is less than zero, an ERROR is thrown. If no third argument
 * (length) is provided, the length to the end of the string is assumed.
 */
Datum
bytea_substr(PG_FUNCTION_ARGS)
{

}

/*
 * bytea_substr_no_len -
 *	  Wrapper to avoid opr_sanity failure due to
 *	  one function accepting a different number of args.
 */
Datum
bytea_substr_no_len(PG_FUNCTION_ARGS)
{

}

static bytea *
bytea_substring(Datum str, int S, int L, bool length_not_specified)
{




















































}

/*
 * byteaoverlay
 *	Replace specified substring of first string with second
 *
 * The SQL standard defines OVERLAY() in terms of substring and concatenation.
 * This code is a direct implementation of what the standard says.
 */
Datum
byteaoverlay(PG_FUNCTION_ARGS)
{






}

Datum
byteaoverlay_no_len(PG_FUNCTION_ARGS)
{







}

static bytea *
bytea_overlay(bytea *t1, bytea *t2, int sp, int sl)
{

























}

/*
 * byteapos -
 *	  Return the position of the specified substring.
 *	  Implements the SQL POSITION() function.
 * Cloned from textpos and modified as required.
 */
Datum
byteapos(PG_FUNCTION_ARGS)
{































}

/*-------------------------------------------------------------
 * byteaGetByte
 *
 * this routine treats "bytea" as an array of bytes.
 * It returns the Nth byte (a number between 0 and 255).
 *-------------------------------------------------------------
 */
Datum
byteaGetByte(PG_FUNCTION_ARGS)
{















}

/*-------------------------------------------------------------
 * byteaGetBit
 *
 * This routine treats a "bytea" type like an array of bits.
 * It returns the value of the Nth bit (0 or 1).
 *
 *-------------------------------------------------------------
 */
Datum
byteaGetBit(PG_FUNCTION_ARGS)
{



























}

/*-------------------------------------------------------------
 * byteaSetByte
 *
 * Given an instance of type 'bytea' creates a new one with
 * the Nth byte set to the given value.
 *
 *-------------------------------------------------------------
 */
Datum
byteaSetByte(PG_FUNCTION_ARGS)
{


















}

/*-------------------------------------------------------------
 * byteaSetBit
 *
 * Given an instance of type 'bytea' creates a new one with
 * the Nth bit set to the given value.
 *
 *-------------------------------------------------------------
 */
Datum
byteaSetBit(PG_FUNCTION_ARGS)
{











































}

/* text_name()
 * Converts a text type to a Name type.
 */
Datum
text_name(PG_FUNCTION_ARGS)
{

















}

/* name_text()
 * Converts a Name type to a text type.
 */
Datum
name_text(PG_FUNCTION_ARGS)
{



}

/*
 * textToQualifiedNameList - convert a text object to list of names
 *
 * This implements the input parsing needed by nextval() and other
 * functions that take a text parameter representing a qualified name.
 * We split the name at dots, downcase if not double-quoted, and
 * truncate names if they're too long.
 */
List *
textToQualifiedNameList(text *textval)
{






























}

/*
 * SplitIdentifierString --- parse a string containing identifiers
 *
 * This is the guts of textToQualifiedNameList, and is exported for use in
 * other situations such as parsing GUC variables.  In the GUC case, it's
 * important to avoid memory leaks, so the API is designed to minimize the
 * amount of stuff that needs to be allocated and freed.
 *
 * Inputs:
 *	rawstring: the input string; must be overwritable!	On return, it's
 *			   been modified to contain the separated identifiers.
 *	separator: the separator punctuation expected between identifiers
 *			   (typically '.' or ',').  Whitespace may also appear
 *around identifiers. Outputs: namelist: filled with a palloc'd list of pointers
 *to identifiers within rawstring.  Caller should list_free() this even on error
 *return.
 *
 * Returns true if okay, false if there is a syntax error in the string.
 *
 * Note that an empty string is considered okay here, though not in
 * textToQualifiedNameList.
 */
bool
SplitIdentifierString(char *rawstring, char separator, List **namelist)
{


















































































































}

/*
 * SplitDirectoriesString --- parse a string containing file/directory names
 *
 * This works fine on file names too; the function name is historical.
 *
 * This is similar to SplitIdentifierString, except that the parsing
 * rules are meant to handle pathnames instead of identifiers: there is
 * no downcasing, embedded spaces are allowed, the max length is MAXPGPATH-1,
 * and we apply canonicalize_path() to each extracted string.  Because of the
 * last, the returned strings are separately palloc'd rather than being
 * pointers into rawstring --- but we still scribble on rawstring.
 *
 * Inputs:
 *	rawstring: the input string; must be modifiable!
 *	separator: the separator punctuation expected between directories
 *			   (typically ',' or ';').  Whitespace may also appear
 *around directories. Outputs: namelist: filled with a palloc'd list of
 *directory names. Caller should list_free_deep() this even on error return.
 *
 * Returns true if okay, false if there is a syntax error in the string.
 *
 * Note that an empty string is considered okay here.
 */
bool
SplitDirectoriesString(char *rawstring, char separator, List **namelist)
{









































































































}

/*
 * SplitGUCList --- parse a string containing identifiers or file names
 *
 * This is used to split the value of a GUC_LIST_QUOTE GUC variable, without
 * presuming whether the elements will be taken as identifiers or file names.
 * We assume the input has already been through flatten_set_variable_args(),
 * so that we need never downcase (if appropriate, that was done already).
 * Nor do we ever truncate, since we don't know the correct max length.
 * We disallow embedded whitespace for simplicity (it shouldn't matter,
 * because any embedded whitespace should have led to double-quoting).
 * Otherwise the API is identical to SplitIdentifierString.
 *
 * XXX it's annoying to have so many copies of this string-splitting logic.
 * However, it's not clear that having one function with a bunch of option
 * flags would be much better.
 *
 * XXX there is a version of this function in src/bin/pg_dump/dumputils.c.
 * Be sure to update that if you have to change this.
 *
 * Inputs:
 *	rawstring: the input string; must be overwritable!	On return, it's
 *			   been modified to contain the separated identifiers.
 *	separator: the separator punctuation expected between identifiers
 *			   (typically '.' or ',').  Whitespace may also appear
 *around identifiers. Outputs: namelist: filled with a palloc'd list of pointers
 *to identifiers within rawstring.  Caller should list_free() this even on error
 *return.
 *
 * Returns true if okay, false if there is a syntax error in the string.
 */
bool
SplitGUCList(char *rawstring, char separator, List **namelist)
{





























































































}

/*****************************************************************************
 *	Comparison Functions used for bytea
 *
 * Note: btree indexes need these routines not to leak memory; therefore,
 * be careful to free working copies of toasted datums.  Most places don't
 * need to be so careful.
 *****************************************************************************/

Datum
byteaeq(PG_FUNCTION_ARGS)
{



























}

Datum
byteane(PG_FUNCTION_ARGS)
{



























}

Datum
bytealt(PG_FUNCTION_ARGS)
{














}

Datum
byteale(PG_FUNCTION_ARGS)
{














}

Datum
byteagt(PG_FUNCTION_ARGS)
{














}

Datum
byteage(PG_FUNCTION_ARGS)
{














}

Datum
byteacmp(PG_FUNCTION_ARGS)
{


















}

Datum
bytea_sortsupport(PG_FUNCTION_ARGS)
{











}

/*
 * appendStringInfoText
 *
 * Append a text to str.
 * Like appendStringInfoString(str, text_to_cstring(t)) but faster.
 */
static void
appendStringInfoText(StringInfo str, const text *t)
{

}

/*
 * replace_text
 * replace all occurrences of 'old_sub_str' in 'orig_str'
 * with 'new_sub_str' to form 'new_str'
 *
 * returns 'orig_str' if 'old_sub_str' == '' or 'orig_str' == ''
 * otherwise returns 'new_str'
 */
Datum
replace_text(PG_FUNCTION_ARGS)
{


































































}

/*
 * check_replace_text_has_escape_char
 *
 * check whether replace_text contains escape char.
 */
static bool
check_replace_text_has_escape_char(const text *replace_text)
{

























}

/*
 * appendStringInfoRegexpSubstr
 *
 * Append replace_text to str, substituting regexp back references for
 * \n escapes.  start_ptr is the start of the match in the source string,
 * at logical character position data_pos.
 */
static void
appendStringInfoRegexpSubstr(StringInfo str, text *replace_text, regmatch_t *pmatch, char *start_ptr, int data_pos)
{




























































































}

#define REGEXP_REPLACE_BACKREF_CNT 10

/*
 * replace_text_regexp
 *
 * replace text that matches to regexp in src_text to replace_text.
 *
 * Note: to avoid having to include regex.h in builtins.h, we declare
 * the regexp argument as void *, but really it's regex_t *.
 */
text *
replace_text_regexp(text *src_text, void *regexp, text *replace_text, bool glob)
{


























































































































}

/*
 * split_text
 * parse input string
 * return ord item (1 based)
 * based on provided field separator
 */
Datum
split_text(PG_FUNCTION_ARGS)
{


































































































}

/*
 * Convenience function to return true when two text params are equal.
 */
static bool
text_isequal(text *txt1, text *txt2, Oid collid)
{

}

/*
 * text_to_array
 * parse input string and return text array of elements,
 * based on provided field separator
 */
Datum
text_to_array(PG_FUNCTION_ARGS)
{

}

/*
 * text_to_array_null
 * parse input string and return text array of elements,
 * based on provided field separator and null string
 *
 * This is a separate entry point only to prevent the regression tests from
 * complaining about different argument sets for the same internal function.
 */
Datum
text_to_array_null(PG_FUNCTION_ARGS)
{

}

/*
 * common code for text_to_array and text_to_array_null functions
 *
 * These are not strict so we have to test for null inputs explicitly.
 */
static Datum
text_to_array_internal(PG_FUNCTION_ARGS)
{
































































































































































}

/*
 * array_to_text
 * concatenate Cstring representation of input array elements
 * using provided field separator
 */
Datum
array_to_text(PG_FUNCTION_ARGS)
{




}

/*
 * array_to_text_null
 * concatenate Cstring representation of input array elements
 * using provided field separator and null string
 *
 * This version is not strict so we have to test for null inputs explicitly.
 */
Datum
array_to_text_null(PG_FUNCTION_ARGS)
{
























}

/*
 * common code for array_to_text and array_to_text_null functions
 */
static text *
array_to_text_internal(FunctionCallInfo fcinfo, ArrayType *v, const char *fldsep, const char *null_string)
{



















































































































}

#define HEXBASE 16
/*
 * Convert an int32 to a string containing a base 16 (hex) representation of
 * the number.
 */
Datum
to_hex32(PG_FUNCTION_ARGS)
{















}

/*
 * Convert an int64 to a string containing a base 16 (hex) representation of
 * the number.
 */
Datum
to_hex64(PG_FUNCTION_ARGS)
{















}

/*
 * Return the size of a datum, possibly compressed
 *
 * Works on any data type
 */
Datum
pg_column_size(PG_FUNCTION_ARGS)
{









































}

/*
 * string_agg - Concatenates values and returns string.
 *
 * Syntax: string_agg(value text, delimiter text) RETURNS text
 *
 * Note: Any NULL values are ignored. The first-call delimiter isn't
 * actually used at all, and on subsequent calls the delimiter precedes
 * the associated value.
 */

/* subroutine to initialize state */
static StringInfo
makeStringAggState(FunctionCallInfo fcinfo)
{



















}

Datum
string_agg_transfn(PG_FUNCTION_ARGS)
{

























}

Datum
string_agg_finalfn(PG_FUNCTION_ARGS)
{















}

/*
 * Prepare cache with fmgr info for the output functions of the datatypes of
 * the arguments of a concat-like function, beginning with argument "argidx".
 * (Arguments before that will have corresponding slots in the resulting
 * FmgrInfo array, but we don't fill those slots.)
 */
static FmgrInfo *
build_concat_foutcache(FunctionCallInfo fcinfo, int argidx)
{

























}

/*
 * Implementation of both concat() and concat_ws().
 *
 * sepstr is the separator string to place between values.
 * argidx identifies the first argument to concatenate (counting from zero);
 * note that this must be constant across any one series of calls.
 *
 * Returns NULL if result should be NULL, else text value.
 */
static text *
concat_internal(const char *sepstr, int argidx, FunctionCallInfo fcinfo)
{














































































}

/*
 * Concatenate all arguments. NULL arguments are ignored.
 */
Datum
text_concat(PG_FUNCTION_ARGS)
{








}

/*
 * Concatenate all but first argument value with separators. The first
 * parameter is used as the separator. NULL arguments are ignored.
 */
Datum
text_concat_ws(PG_FUNCTION_ARGS)
{
















}

/*
 * Return first n characters in the string. When n is negative,
 * return all but last |n| characters.
 */
Datum
text_left(PG_FUNCTION_ARGS)
{

















}

/*
 * Return last n characters in the string. When n is negative,
 * return all but first |n| characters.
 */
Datum
text_right(PG_FUNCTION_ARGS)
{

















}

/*
 * Return reversed string
 */
Datum
text_reverse(PG_FUNCTION_ARGS)
{


































}

/*
 * Support macros for text_format()
 */
#define TEXT_FORMAT_FLAG_MINUS 0x0001 /* is minus flag present? */

#define ADVANCE_PARSE_POINTER(ptr, end_ptr)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                            \
  do                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   \
  {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    \
    if (++(ptr) >= (end_ptr))                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          \
      ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("unterminated format() type specifier"), errhint("For a single \"%%\" use \"%%%%\".")));                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        \
  } while (0)

/*
 * Returns a formatted string
 */
Datum
text_format(PG_FUNCTION_ARGS)
{
















































































































































































































































































}

/*
 * Parse contiguous digits as a decimal number.
 *
 * Returns true if some digits could be parsed.
 * The value is returned into *value, and *ptr is advanced to the next
 * character to be parsed.
 *
 * Note parsing invariant: at least one character is known available before
 * string end (end_ptr) at entry, and this is still true at exit.
 */
static bool
text_format_parse_digits(const char **ptr, const char *end_ptr, int *value)
{




















}

/*
 * Parse a format specifier (generally following the SUS printf spec).
 *
 * We have already advanced over the initial '%', and we are looking for
 * [argpos][flags][width]type (but the type character is not consumed here).
 *
 * Inputs are start_ptr (the position after '%') and end_ptr (string end + 1).
 * Output parameters:
 *	argpos: argument position for value to be printed.  -1 means
 *unspecified. widthpos: argument position for width.  Zero means the argument
 *position was unspecified (ie, take the next arg) and -1 means no width
 *			argument (width was omitted or specified as a constant).
 *	flags: bitmask of flags.
 *	width: directly-specified width value.  Zero means the width was omitted
 *			(note it's not necessary to distinguish this case from
 *an explicit zero width value).
 *
 * The function result is the next character position to be parsed, ie, the
 * location where the type character is/should be.
 *
 * Note parsing invariant: at least one character is known available before
 * string end (end_ptr) at entry, and this is still true at exit.
 */
static const char *
text_format_parse_format(const char *start_ptr, const char *end_ptr, int *argpos, int *widthpos, int *flags, int *width)
{







































































}

/*
 * Format a %s, %I, or %L conversion
 */
static void
text_format_string_conversion(StringInfo buf, char conversion, FmgrInfo *typOutputInfo, Datum value, bool isNull, int flags, int width)
{












































}

/*
 * Append str to buf, padding as directed by flags/width
 */
static void
text_format_append_string(StringInfo buf, const char *str, int flags, int width)
{













































}

/*
 * text_format_nv - nonvariadic wrapper for text_format function.
 *
 * note: this wrapper is necessary to pass the sanity check in opr_sanity,
 * which checks that all built-in functions that share the implementing C
 * function take the same number of arguments.
 */
Datum
text_format_nv(PG_FUNCTION_ARGS)
{

}

/*
 * Helper function for Levenshtein distance functions. Faster than memcmp(),
 * for this use case.
 */
static inline bool
rest_of_char_same(const char *s1, const char *s2, int len)
{









}

/* Expand each Levenshtein distance variant */
#include "levenshtein.c"
#define LEVENSHTEIN_LESS_EQUAL
#include "levenshtein.c"