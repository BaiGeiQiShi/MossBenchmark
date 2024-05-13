/*-------------------------------------------------------------------------
 *
 * base64.c
 *	  Encoding and decoding routines for base64 without whitespace.
 *
 * Copyright (c) 2001-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/common/base64.c
 *
 *-------------------------------------------------------------------------
 */

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "common/base64.h"

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

/*
 * pg_b64_encode
 *
 * Encode into base64 the given string.  Returns the length of the encoded
 * string.
 */
int
pg_b64_encode(const char *src, int len, char *dst)
{



































}

/*
 * pg_b64_decode
 *
 * Decode the given base64 string.  Returns the length of the decoded
 * string on success, and -1 in the event of an error.
 */
int
pg_b64_decode(const char *src, int len, char *dst)
{



















































































}

/*
 * pg_b64_enc_len
 *
 * Returns to caller the length of the string if it were encoded with
 * base64 based on the length provided by caller.  This is useful to
 * estimate how large a buffer allocation needs to be done before doing
 * the actual encoding.
 */
int
pg_b64_enc_len(int srclen)
{


}

/*
 * pg_b64_dec_len
 *
 * Returns to caller the length of the string if it were to be decoded
 * with base64, based on the length given by caller.  This is useful to
 * estimate how large a buffer allocation needs to be done before doing
 * the actual decoding.
 */
int
pg_b64_dec_len(int srclen)
{

}