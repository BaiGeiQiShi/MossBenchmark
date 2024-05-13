/*
 * conversion functions between pg_wchar and multibyte streams.
 * Tatsuo Ishii
 * src/backend/utils/mb/wchar.c
 *
 */
/* can be used in either frontend or backend */
#ifdef FRONTEND
#include "postgres_fe.h"
#else
#include "postgres.h"
#endif

#include "mb/pg_wchar.h"

/*
 * Operations on multi-byte encodings are driven by a table of helper
 * functions.
 *
 * To add an encoding support, define mblen(), dsplen() and verifier() for
 * the encoding.  For server-encodings, also define mb2wchar() and wchar2mb()
 * conversion functions.
 *
 * These functions generally assume that their input is validly formed.
 * The "verifier" functions, further down in the file, have to be more
 * paranoid.
 *
 * We expect that mblen() does not need to examine more than the first byte
 * of the character to discover the correct length.  GB18030 is an exception
 * to that rule, though, as it also looks at second byte.  But even that
 * behaves in a predictable way, if you only pass the first byte: it will
 * treat 4-byte encoded characters as two 2-byte encoded characters, which is
 * good enough for all current uses.
 *
 * Note: for the display output of psql to work properly, the return values
 * of the dsplen functions must conform to the Unicode standard. In particular
 * the NUL character is zero width and control characters are generally
 * width -1. It is recommended that non-ASCII encodings refer their ASCII
 * subset to the ASCII routines to ensure consistency.
 */

/*
 * SQL/ASCII
 */
static int
pg_ascii2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{










}

static int
pg_ascii_mblen(const unsigned char *s)
{

}

static int
pg_ascii_dsplen(const unsigned char *s)
{










}

/*
 * EUC
 */
static int
pg_euc2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{


































}

static inline int
pg_euc_mblen(const unsigned char *s)
{



















}

static inline int
pg_euc_dsplen(const unsigned char *s)
{



















}

/*
 * EUC_JP
 */
static int
pg_eucjp2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{

}

static int
pg_eucjp_mblen(const unsigned char *s)
{

}

static int
pg_eucjp_dsplen(const unsigned char *s)
{



















}

/*
 * EUC_KR
 */
static int
pg_euckr2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{

}

static int
pg_euckr_mblen(const unsigned char *s)
{

}

static int
pg_euckr_dsplen(const unsigned char *s)
{

}

/*
 * EUC_CN
 *
 */
static int
pg_euccn2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{


































}

static int
pg_euccn_mblen(const unsigned char *s)
{











}

static int
pg_euccn_dsplen(const unsigned char *s)
{











}

/*
 * EUC_TW
 *
 */
static int
pg_euctw2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{



































}

static int
pg_euctw_mblen(const unsigned char *s)
{



















}

static int
pg_euctw_dsplen(const unsigned char *s)
{



















}

/*
 * Convert pg_wchar to EUC_* encoding.
 * caller must allocate enough space for "to", including a trailing zero!
 * len: length of from.
 * "from" not necessarily null terminated.
 */
static int
pg_wchar2euc_with_len(const pg_wchar *from, unsigned char *to, int len)
{





































}

/*
 * JOHAB
 */
static int
pg_johab_mblen(const unsigned char *s)
{

}

static int
pg_johab_dsplen(const unsigned char *s)
{

}

/*
 * convert UTF8 string to pg_wchar (UCS-4)
 * caller must allocate enough space for "to", including a trailing zero!
 * len: length of from.
 * "from" not necessarily null terminated.
 */
static int
pg_utf2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{

























































}

/*
 * Map a Unicode code point to UTF-8.  utf8string must have 4 bytes of
 * space allocated.
 */
unsigned char *
unicode_to_utf8(pg_wchar c, unsigned char *utf8string)
{
























}

/*
 * Trivial conversion from pg_wchar to UTF-8.
 * caller should allocate enough space for "to"
 * len: length of from.
 * "from" not necessarily null terminated.
 */
static int
pg_wchar2utf_with_len(const pg_wchar *from, unsigned char *to, int len)
{















}

/*
 * Return the byte length of a UTF8 character pointed to by s
 *
 * Note: in the current implementation we do not support UTF8 sequences
 * of more than 4 bytes; hence do NOT return a value larger than 4.
 * We return "1" for any leading byte that is either flat-out illegal or
 * indicates a length larger than we support.
 *
 * pg_utf2wchar_with_len(), utf8_to_unicode(), pg_utf8_islegal(), and perhaps
 * other places would need to be fixed to change this.
 */
int
pg_utf_mblen(const unsigned char *s)
{

































}

/*
 * This is an implementation of wcwidth() and wcswidth() as defined in
 * "The Single UNIX Specification, Version 2, The Open Group, 1997"
 * <http://www.UNIX-systems.org/online.html>
 *
 * Markus Kuhn -- 2001-09-08 -- public domain
 *
 * customised for PostgreSQL
 *
 * original available at : http://www.cl.cam.ac.uk/~mgk25/ucs/wcwidth.c
 */

struct mbinterval
{
  unsigned short first;
  unsigned short last;
};

/* auxiliary function for binary search in interval table */
static int
mbbisearch(pg_wchar ucs, const struct mbinterval *table, int max)
{

























}

/* The following functions define the column width of an ISO 10646
 * character as follows:
 *
 *	  - The null character (U+0000) has a column width of 0.
 *
 *	  - Other C0/C1 control characters and DEL will lead to a return
 *		value of -1.
 *
 *	  - Non-spacing and enclosing combining characters (general
 *		category code Mn or Me in the Unicode database) have a
 *		column width of 0.
 *
 *	  - Other format characters (general category code Cf in the Unicode
 *		database) and ZERO WIDTH SPACE (U+200B) have a column width of
 *0.
 *
 *	  - Hangul Jamo medial vowels and final consonants (U+1160-U+11FF)
 *		have a column width of 0.
 *
 *	  - Spacing characters in the East Asian Wide (W) or East Asian
 *		FullWidth (F) category as defined in Unicode Technical
 *		Report #11 have a column width of 2.
 *
 *	  - All remaining characters (including all printable
 *		ISO 8859-1 and WGL4 characters, Unicode control characters,
 *		etc.) have a column width of 1.
 *
 * This implementation assumes that wchar_t characters are encoded
 * in ISO 10646.
 */

static int
ucs_wcwidth(pg_wchar ucs)
{































































































































































































































}

/*
 * Convert a UTF-8 character to a Unicode code point.
 * This is a one-character version of pg_utf2wchar_with_len.
 *
 * No error checks here, c must point to a long-enough string.
 */
pg_wchar
utf8_to_unicode(const unsigned char *c)
{





















}

static int
pg_utf_dsplen(const unsigned char *s)
{

}

/*
 * convert mule internal code to pg_wchar
 * caller should allocate enough space for "to"
 * len: length of from.
 * "from" not necessarily null terminated.
 */
static int
pg_mule2wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{










































}

/*
 * convert pg_wchar to mule internal code
 * caller should allocate enough space for "to"
 * len: length of from.
 * "from" not necessarily null terminated.
 */
static int
pg_wchar2mule_with_len(const pg_wchar *from, unsigned char *to, int len)
{




























































}

int
pg_mule_mblen(const unsigned char *s)
{























}

static int
pg_mule_dsplen(const unsigned char *s)
{






























}

/*
 * ISO8859-1
 */
static int
pg_latin12wchar_with_len(const unsigned char *from, pg_wchar *to, int len)
{










}

/*
 * Trivial conversion from pg_wchar to single byte encoding. Just ignores
 * high bits.
 * caller should allocate enough space for "to"
 * len: length of from.
 * "from" not necessarily null terminated.
 */
static int
pg_wchar2single_with_len(const pg_wchar *from, unsigned char *to, int len)
{










}

static int
pg_latin1_mblen(const unsigned char *s)
{

}

static int
pg_latin1_dsplen(const unsigned char *s)
{

}

/*
 * SJIS
 */
static int
pg_sjis_mblen(const unsigned char *s)
{















}

static int
pg_sjis_dsplen(const unsigned char *s)
{















}

/*
 * Big5
 */
static int
pg_big5_mblen(const unsigned char *s)
{











}

static int
pg_big5_dsplen(const unsigned char *s)
{











}

/*
 * GBK
 */
static int
pg_gbk_mblen(const unsigned char *s)
{











}

static int
pg_gbk_dsplen(const unsigned char *s)
{











}

/*
 * UHC
 */
static int
pg_uhc_mblen(const unsigned char *s)
{











}

static int
pg_uhc_dsplen(const unsigned char *s)
{











}

/*
 * GB18030
 *	Added by Bill Huang <bhuang@redhat.com>,<bill_huanghb@ybb.ne.jp>
 */

/*
 * Unlike all other mblen() functions, this also looks at the second byte of
 * the input.  However, if you only pass the first byte of a multi-byte
 * string, and \0 as the second byte, this still works in a predictable way:
 * a 4-byte character will be reported as two 2-byte characters.  That's
 * enough for all current uses, as a client-only encoding.  It works that
 * way, because in any valid 4-byte GB18030-encoded character, the third and
 * fourth byte look like a 2-byte encoded character, when looked at
 * separately.
 */
static int
pg_gb18030_mblen(const unsigned char *s)
{















}

static int
pg_gb18030_dsplen(const unsigned char *s)
{











}

/*
 *-------------------------------------------------------------------
 * multibyte sequence validators
 *
 * These functions accept "s", a pointer to the first byte of a string,
 * and "len", the remaining length of the string.  If there is a validly
 * encoded character beginning at *s, return its length in bytes; else
 * return -1.
 *
 * The functions can assume that len > 0 and that *s != '\0', but they must
 * test for and reject zeroes in any additional bytes of a multibyte character.
 *
 * Note that this definition allows the function for a single-byte
 * encoding to be just "return 1".
 *-------------------------------------------------------------------
 */

static int
pg_ascii_verifier(const unsigned char *s, int len)
{

}

#define IS_EUC_RANGE_VALID(c) ((c) >= 0xa1 && (c) <= 0xfe)

static int
pg_eucjp_verifier(const unsigned char *s, int len)
{

































































}

static int
pg_euckr_verifier(const unsigned char *s, int len)
{





























}

/* EUC-CN byte sequences are exactly same as EUC-KR */
#define pg_euccn_verifier pg_euckr_verifier

static int
pg_euctw_verifier(const unsigned char *s, int len)
{
























































}

static int
pg_johab_verifier(const unsigned char *s, int len)
{
























}

static int
pg_mule_verifier(const unsigned char *s, int len)
{



















}

static int
pg_latin1_verifier(const unsigned char *s, int len)
{

}

static int
pg_sjis_verifier(const unsigned char *s, int len)
{






















}

static int
pg_big5_verifier(const unsigned char *s, int len)
{


















}

static int
pg_gbk_verifier(const unsigned char *s, int len)
{


















}

static int
pg_uhc_verifier(const unsigned char *s, int len)
{


















}

static int
pg_gb18030_verifier(const unsigned char *s, int len)
{



































}

static int
pg_utf8_verifier(const unsigned char *s, int len)
{













}

/*
 * Check for validity of a single UTF-8 encoded character
 *
 * This directly implements the rules in RFC3629.  The bizarre-looking
 * restrictions on the second byte are meant to ensure that there isn't
 * more than one encoding of a given Unicode character point; that is,
 * you may not use a longer-than-necessary byte sequence with high order
 * zero bits to represent a character that would fit in fewer bytes.
 * To do otherwise is to create security hazards (eg, create an apparent
 * non-ASCII character that decodes to plain ASCII).
 *
 * length is assumed to have been obtained by pg_utf_mblen(), and the
 * caller must have checked that that many bytes are present in the buffer.
 */
bool
pg_utf8_islegal(const unsigned char *source, int length)
{






































































}

#ifndef FRONTEND

/*
 * Generic character incrementer function.
 *
 * Not knowing anything about the properties of the encoding in use, we just
 * keep incrementing the last byte until we get a validly-encoded result,
 * or we run out of values to try.  We don't bother to try incrementing
 * higher-order bytes, so there's no growth in runtime for wider characters.
 * (If we did try to do that, we'd need to consider the likelihood that 255
 * is not a valid final byte in the encoding.)
 */
static bool
pg_generic_charinc(unsigned char *charptr, int len)
{
















}

/*
 * UTF-8 character incrementer function.
 *
 * For a one-byte character less than 0x7F, we just increment the byte.
 *
 * For a multibyte character, every byte but the first must fall between 0x80
 * and 0xBF; and the first byte must be between 0xC0 and 0xF4.  We increment
 * the last byte that's not already at its maximum value.  If we can't find a
 * byte that's less than the maximum allowable value, we simply fail.  We also
 * need some special-case logic to skip regions used for surrogate pair
 * handling, as those should not occur in valid UTF-8.
 *
 * Note that we don't reset lower-order bytes back to their minimums, since
 * we can't afford to make an exhaustive search (see make_greater_string).
 */
static bool
pg_utf8_increment(unsigned char *charptr, int length)
{























































}

/*
 * EUC-JP character incrementer function.
 *
 * If the sequence starts with SS2 (0x8e), it must be a two-byte sequence
 * representing JIS X 0201 characters with the second byte ranging between
 * 0xa1 and 0xdf.  We just increment the last byte if it's less than 0xdf,
 * and otherwise rewrite the whole sequence to 0xa1 0xa1.
 *
 * If the sequence starts with SS3 (0x8f), it must be a three-byte sequence
 * in which the last two bytes range between 0xa1 and 0xfe.  The last byte
 * is incremented if possible, otherwise the second-to-last byte.
 *
 * If the sequence starts with a value other than the above and its MSB
 * is set, it must be a two-byte sequence representing JIS X 0208 characters
 * with both bytes ranging between 0xa1 and 0xfe.  The last byte is
 * incremented if possible, otherwise the second-to-last byte.
 *
 * Otherwise, the sequence is a single-byte ASCII character. It is
 * incremented up to 0x7f.
 */
static bool
pg_eucjp_increment(unsigned char *charptr, int length)
{



























































































}
#endif /* !FRONTEND */

/*
 *-------------------------------------------------------------------
 * encoding info table
 * XXX must be sorted by the same order as enum pg_enc (in mb/pg_wchar.h)
 *-------------------------------------------------------------------
 */
const pg_wchar_tbl pg_wchar_table[] = {
    {pg_ascii2wchar_with_len, pg_wchar2single_with_len, pg_ascii_mblen, pg_ascii_dsplen, pg_ascii_verifier, 1},     /* PG_SQL_ASCII */
    {pg_eucjp2wchar_with_len, pg_wchar2euc_with_len, pg_eucjp_mblen, pg_eucjp_dsplen, pg_eucjp_verifier, 3},        /* PG_EUC_JP */
    {pg_euccn2wchar_with_len, pg_wchar2euc_with_len, pg_euccn_mblen, pg_euccn_dsplen, pg_euccn_verifier, 2},        /* PG_EUC_CN */
    {pg_euckr2wchar_with_len, pg_wchar2euc_with_len, pg_euckr_mblen, pg_euckr_dsplen, pg_euckr_verifier, 3},        /* PG_EUC_KR */
    {pg_euctw2wchar_with_len, pg_wchar2euc_with_len, pg_euctw_mblen, pg_euctw_dsplen, pg_euctw_verifier, 4},        /* PG_EUC_TW */
    {pg_eucjp2wchar_with_len, pg_wchar2euc_with_len, pg_eucjp_mblen, pg_eucjp_dsplen, pg_eucjp_verifier, 3},        /* PG_EUC_JIS_2004 */
    {pg_utf2wchar_with_len, pg_wchar2utf_with_len, pg_utf_mblen, pg_utf_dsplen, pg_utf8_verifier, 4},               /* PG_UTF8 */
    {pg_mule2wchar_with_len, pg_wchar2mule_with_len, pg_mule_mblen, pg_mule_dsplen, pg_mule_verifier, 4},           /* PG_MULE_INTERNAL */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_LATIN1 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_LATIN2 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_LATIN3 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_LATIN4 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_LATIN5 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_LATIN6 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_LATIN7 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_LATIN8 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_LATIN9 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_LATIN10 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_WIN1256 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_WIN1258 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_WIN866 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_WIN874 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_KOI8R */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_WIN1251 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_WIN1252 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* ISO-8859-5 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* ISO-8859-6 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* ISO-8859-7 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* ISO-8859-8 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_WIN1250 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_WIN1253 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_WIN1254 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_WIN1255 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_WIN1257 */
    {pg_latin12wchar_with_len, pg_wchar2single_with_len, pg_latin1_mblen, pg_latin1_dsplen, pg_latin1_verifier, 1}, /* PG_KOI8U */
    {0, 0, pg_sjis_mblen, pg_sjis_dsplen, pg_sjis_verifier, 2},                                                     /* PG_SJIS */
    {0, 0, pg_big5_mblen, pg_big5_dsplen, pg_big5_verifier, 2},                                                     /* PG_BIG5 */
    {0, 0, pg_gbk_mblen, pg_gbk_dsplen, pg_gbk_verifier, 2},                                                        /* PG_GBK */
    {0, 0, pg_uhc_mblen, pg_uhc_dsplen, pg_uhc_verifier, 2},                                                        /* PG_UHC */
    {0, 0, pg_gb18030_mblen, pg_gb18030_dsplen, pg_gb18030_verifier, 4},                                            /* PG_GB18030 */
    {0, 0, pg_johab_mblen, pg_johab_dsplen, pg_johab_verifier, 3},                                                  /* PG_JOHAB */
    {0, 0, pg_sjis_mblen, pg_sjis_dsplen, pg_sjis_verifier, 2}                                                      /* PG_SHIFT_JIS_2004 */
};

/* returns the byte length of a word for mule internal code */
int
pg_mic_mblen(const unsigned char *mbstr)
{

}

/*
 * Returns the byte length of a multibyte character.
 */
int
pg_encoding_mblen(int encoding, const char *mbstr)
{

}

/*
 * Returns the display length of a multibyte character.
 */
int
pg_encoding_dsplen(int encoding, const char *mbstr)
{

}

/*
 * Verify the first multibyte character of the given string.
 * Return its byte length if good, -1 if bad.  (See comments above for
 * full details of the mbverify API.)
 */
int
pg_encoding_verifymb(int encoding, const char *mbstr, int len)
{

}

/*
 * fetch maximum length of a given encoding
 */
int
pg_encoding_max_length(int encoding)
{



}

#ifndef FRONTEND

/*
 * fetch maximum length of the encoding for the current database
 */
int
pg_database_encoding_max_length(void)
{

}

/*
 * get the character incrementer for the encoding for the current database
 */
mbcharacter_incrementer
pg_database_encoding_character_incrementer(void)
{















}

/*
 * Verify mbstr to make sure that it is validly encoded in the current
 * database encoding.  Otherwise same as pg_verify_mbstr().
 */
bool
pg_verifymbstr(const char *mbstr, int len, bool noError)
{

}

/*
 * Verify mbstr to make sure that it is validly encoded in the specified
 * encoding.
 */
bool
pg_verify_mbstr(int encoding, const char *mbstr, int len, bool noError)
{

}

/*
 * Verify mbstr to make sure that it is validly encoded in the specified
 * encoding.
 *
 * mbstr is not necessarily zero terminated; length of mbstr is
 * specified by len.
 *
 * If OK, return length of string in the encoding.
 * If a problem is found, return -1 when noError is
 * true; when noError is false, ereport() a descriptive message.
 */
int
pg_verify_mbstr_len(int encoding, const char *mbstr, int len, bool noError)
{

































































}

/*
 * check_encoding_conversion_args: check arguments of a conversion function
 *
 * "expected" arguments can be either an encoding ID or -1 to indicate that
 * the caller will check whether it accepts the ID.
 *
 * Note: the errors here are not really user-facing, so elog instead of
 * ereport seems sufficient.  Also, we trust that the "expected" encoding
 * arguments are valid encoding IDs, but we don't trust the actuals.
 */
void
check_encoding_conversion_args(int src_encoding, int dest_encoding, int len, int expected_src_encoding, int expected_dest_encoding)
{




















}

/*
 * report_invalid_encoding: complain about invalid multibyte character
 *
 * note: len is remaining length of string, not length of character;
 * len must be greater than zero, as we always examine the first byte.
 */
void
report_invalid_encoding(int encoding, const char *mbstr, int len)
{


















}

/*
 * report_untranslatable_char: complain about untranslatable character
 *
 * note: len is remaining length of string, not length of character;
 * len must be greater than zero, as we always examine the first byte.
 */
void
report_untranslatable_char(int src_encoding, int dest_encoding, const char *mbstr, int len)
{


















}

#endif /* !FRONTEND */