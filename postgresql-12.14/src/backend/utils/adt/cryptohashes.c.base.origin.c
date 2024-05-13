/*-------------------------------------------------------------------------
 *
 * cryptohashes.c
 *	  Cryptographic hash functions
 *
 * Portions Copyright (c) 2018-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/cryptohashes.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "common/md5.h"
#include "common/sha2.h"
#include "utils/builtins.h"

/*
 * MD5
 */

/* MD5 produces a 16 byte (128 bit) hash; double it for hex */
#define MD5_HASH_LEN 32

/*
 * Create an MD5 hash of a text value and return it as hex string.
 */
Datum
md5_text(PG_FUNCTION_ARGS)
{















}

/*
 * Create an MD5 hash of a bytea value and return it as a hex string.
 */
Datum
md5_bytea(PG_FUNCTION_ARGS)
{











}

/*
 * SHA-2 variants
 */

Datum
sha224_bytea(PG_FUNCTION_ARGS)
{



















}

Datum
sha256_bytea(PG_FUNCTION_ARGS)
{



















}

Datum
sha384_bytea(PG_FUNCTION_ARGS)
{



















}

Datum
sha512_bytea(PG_FUNCTION_ARGS)
{



















}