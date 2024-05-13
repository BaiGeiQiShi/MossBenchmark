/*
 *	md5.c
 *
 *	Implements	the  MD5 Message-Digest Algorithm as specified in
 *	RFC  1321.  This  implementation  is a simple one, in that it
 *	needs  every  input  byte  to  be  buffered  before doing any
 *	calculations.  I  do  not  expect  this  file  to be used for
 *	general  purpose  MD5'ing  of large amounts of data, only for
 *	generating hashed passwords from limited input.
 *
 *	Sverre H. Huseby <sverrehu@online.no>
 *
 *	Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *	Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/common/md5.c
 */

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include "common/md5.h"

/*
 *	PRIVATE FUNCTIONS
 */

/*
 *	The returned array is allocated using malloc.  the caller should free it
 *	when it is no longer needed.
 */
static uint8 *
createPaddedCopyWithLength(const uint8 *b, uint32 *l)
{





















































}

#define F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | ~(z)))
#define ROT_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void
doTheRounds(uint32 X[16], uint32 state[4])
{



















































































}

static int
calculateDigestFromBuffer(const uint8 *b, uint32 len, uint8 sum[16])
{























































}

static void
bytesToHex(uint8 b[16], char *s)
{









}

/*
 *	PUBLIC FUNCTIONS
 */

/*
 *	pg_md5_hash
 *
 *	Calculates the MD5 sum of the bytes in a buffer.
 *
 *	SYNOPSIS	  #include "md5.h"
 *				  int pg_md5_hash(const void *buff, size_t len,*char *hexsum)
 *
 *	INPUT		  buff	  the buffer containing the bytes that you want
 *						  the MD5 sum of.
 *				  len	  number of bytes in the buffer.
 *
 *	OUTPUT		  hexsum  the MD5 sum as a '\0'-terminated string of
 *						  hexadecimal digits.  an MD5
 *sum is 16 bytes long. each byte is represented by two heaxadecimal characters.
 *you thus need to provide an array of 33 characters, including the trailing
 *'\0'.
 *
 *	RETURNS		  false on failure (out of memory for internal buffers)
 *or true on success.
 *
 *	STANDARDS	  MD5 is described in RFC 1321.
 *
 *	AUTHOR		  Sverre H. Huseby <sverrehu@online.no>
 *
 */
bool
pg_md5_hash(const void *buff, size_t len, char *hexsum)
{









}

bool
pg_md5_binary(const void *buff, size_t len, void *outbuf)
{





}

/*
 * Computes MD5 checksum of "passwd" (a null-terminated string) followed
 * by "salt" (which need not be null-terminated).
 *
 * Output format is "md5" followed by a 32-hex-digit MD5 checksum.
 * Hence, the output buffer "buf" must be at least 36 bytes long.
 *
 * Returns true if okay, false on error (out of memory).
 */
bool
pg_md5_encrypt(const char *passwd, const char *salt, size_t salt_len, char *buf)
{
























}