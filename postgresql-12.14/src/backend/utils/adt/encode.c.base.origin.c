/*-------------------------------------------------------------------------
 *
 * encode.c
 *	  Various data encoding/decoding things.
 *
 * Copyright (c) 2001-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/encode.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "utils/builtins.h"

struct pg_encoding
{
  unsigned (*encode_len)(const char *data, unsigned dlen);
  unsigned (*decode_len)(const char *data, unsigned dlen);
  unsigned (*encode)(const char *data, unsigned dlen, char *res);
  unsigned (*decode)(const char *data, unsigned dlen, char *res);
};

static const struct pg_encoding *
pg_find_encoding(const char *name);

/*
 * SQL functions.
 */

Datum
binary_encode(PG_FUNCTION_ARGS)
{































}

Datum
binary_decode(PG_FUNCTION_ARGS)
{































}

/*
 * HEX
 */

static const char hextbl[] = "0123456789abcdef";

static const int8 hexlookup[128] = {
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

unsigned
hex_encode(const char *src, unsigned len, char *dst)
{









}

static inline char
get_hex(char c)
{













}

unsigned
hex_decode(const char *src, unsigned len, char *dst)
{
























}

static unsigned
hex_enc_len(const char *src, unsigned srclen)
{

}

static unsigned
hex_dec_len(const char *src, unsigned srclen)
{

}

/*
 * BASE64
 */

static const char _base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const int8 b64lookup[128] = {
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
    62,
    -1,
    -1,
    -1,
    63,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
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
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    22,
    23,
    24,
    25,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    50,
    51,
    -1,
    -1,
    -1,
    -1,
    -1,
};

static unsigned
pg_base64_encode(const char *src, unsigned len, char *dst)
{








































}

static unsigned
pg_base64_decode(const char *src, unsigned len, char *dst)
{









































































}

static unsigned
pg_base64_enc_len(const char *src, unsigned srclen)
{


}

static unsigned
pg_base64_dec_len(const char *src, unsigned srclen)
{

}

/*
 * Escape
 * Minimally escape bytea to text.
 * De-escape text to bytea.
 *
 * We must escape zero bytes and high-bit-set bytes to avoid generating
 * text that might be invalid in the current encoding, or that might
 * change to something else if passed through an encoding conversion
 * (leading to failing to de-escape to the original bytea value).
 * Also of course backslash itself has to be escaped.
 *
 * De-escaping processes \\ and any \### octal
 */

#define VAL(CH) ((CH) - '0')
#define DIG(VAL) ((VAL) + '0')

static unsigned
esc_encode(const char *src, unsigned srclen, char *dst)
{


































}

static unsigned
esc_decode(const char *src, unsigned srclen, char *dst)
{







































}

static unsigned
esc_enc_len(const char *src, unsigned srclen)
{






















}

static unsigned
esc_dec_len(const char *src, unsigned srclen)
{


































}

/*
 * Common
 */

static const struct
{
  const char *name;
  struct pg_encoding enc;
} enclist[] =

    {{"hex", {hex_enc_len, hex_dec_len, hex_encode, hex_decode}}, {"base64", {pg_base64_enc_len, pg_base64_dec_len, pg_base64_encode, pg_base64_decode}}, {"escape", {esc_enc_len, esc_dec_len, esc_encode, esc_decode}}, {NULL, {NULL, NULL, NULL, NULL}}};

static const struct pg_encoding *
pg_find_encoding(const char *name)
{











}