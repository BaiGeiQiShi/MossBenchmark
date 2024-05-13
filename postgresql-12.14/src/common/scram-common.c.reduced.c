/*-------------------------------------------------------------------------
 * scram-common.c
 *		Shared frontend/backend code for SCRAM authentication
 *
 * This contains the common low-level functions needed in both frontend and
 * backend, for implement the Salted Challenge Response Authentication
 * Mechanism (SCRAM), per IETF's RFC 5802.
 *
 * Portions Copyright (c) 2017-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/common/scram-common.c
 *
 *-------------------------------------------------------------------------
 */
#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "common/base64.h"
#include "common/scram-common.h"
#include "port/pg_bswap.h"




/*
 * Calculate HMAC per RFC2104.
 *
 * The hash function used is SHA-256.
 */
void
scram_HMAC_init(scram_HMAC_ctx *ctx, const uint8 *key, int keylen)
{





























}

/*
 * Update HMAC calculation
 * The hash function used is SHA-256.
 */
void
scram_HMAC_update(scram_HMAC_ctx *ctx, const char *str, int slen)
{

}

/*
 * Finalize HMAC calculation.
 * The hash function used is SHA-256.
 */
void
scram_HMAC_final(uint8 *result, scram_HMAC_ctx *ctx)
{









}

/*
 * Calculate SaltedPassword.
 *
 * The password should already be normalized by SASLprep.
 */
void
scram_SaltedPassword(const char *password, const char *salt, int saltlen,
                     int iterations, uint8 *result)
{






























}

/*
 * Calculate SHA-256 hash for a NULL-terminated string. (The NULL terminator is
 * not included in the hash).
 */
void
scram_H(const uint8 *input, int len, uint8 *result)
{





}

/*
 * Calculate ClientKey.
 */
void
scram_ClientKey(const uint8 *salted_password, uint8 *result)
{





}

/*
 * Calculate ServerKey.
 */
void
scram_ServerKey(const uint8 *salted_password, uint8 *result)
{





}

/*
 * Construct a verifier string for SCRAM, stored in pg_authid.rolpassword.
 *
 * The password should already have been processed with SASLprep, if necessary!
 *
 * If iterations is 0, default number of iterations is used.  The result is
 * palloc'd or malloc'd, so caller is responsible for freeing it.
 */
char *
scram_build_verifier(const char *salt, int saltlen, int iterations,
                     const char *password)
{

















































}
