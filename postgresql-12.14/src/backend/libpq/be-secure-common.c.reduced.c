/*-------------------------------------------------------------------------
 *
 * be-secure-common.c
 *
 * common implementation-independent SSL support code
 *
 * While be-secure.c contains the interfaces that the rest of the
 * communications code calls, this file contains support routines that are
 * used by the library-specific implementations such as be-secure-openssl.c.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/libpq/be-secure-common.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <sys/stat.h>
#include <unistd.h>

#include "libpq/libpq.h"
#include "storage/fd.h"

/*
 * Run ssl_passphrase_command
 *
 * prompt will be substituted for %p.  is_server_start determines the loglevel
 * of error messages.
 *
 * The result will be put in buffer buf, which is of size size.  The return
 * value is the length of the actual result.
 */
int
run_ssl_passphrase_command(const char *prompt, bool is_server_start, char *buf,
                           int size)
{







































































}

/*
 * Check permissions for SSL key files.
 */
bool
check_ssl_key_file_permissions(const char *ssl_key_file, bool isServerStart)
{

























































}
