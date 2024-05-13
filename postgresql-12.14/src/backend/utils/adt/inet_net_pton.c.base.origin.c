/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *	  src/backend/utils/adt/inet_net_pton.c
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "Id: inet_net_pton.c,v 1.4.2.3 2004/03/17 00:40:11 marka Exp $";
#endif

#include "postgres.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>

#include "utils/builtins.h" /* pgrminclude ignore */ /* needed on some
														 * platforms */
#include "utils/inet.h"

static int
inet_net_pton_ipv4(const char *src, u_char *dst);
static int
inet_cidr_pton_ipv4(const char *src, u_char *dst, size_t size);
static int
inet_net_pton_ipv6(const char *src, u_char *dst);
static int
inet_cidr_pton_ipv6(const char *src, u_char *dst, size_t size);

/*
 * int
 * inet_net_pton(af, src, dst, size)
 *	convert network number from presentation to network format.
 *	accepts hex octets, hex strings, decimal octets, and /CIDR.
 *	"size" is in bytes and describes "dst".
 * return:
 *	number of bits, either imputed classfully or specified with /CIDR,
 *	or -1 if some failure occurred (check errno).  ENOENT means it was
 *	not a valid network specification.
 * author:
 *	Paul Vixie (ISC), June 1996
 *
 * Changes:
 *	I added the inet_cidr_pton function (also from Paul) and changed
 *	the names to reflect their current use.
 *
 */
int
inet_net_pton(int af, const char *src, void *dst, size_t size)
{










}

/*
 * static int
 * inet_cidr_pton_ipv4(src, dst, size)
 *	convert IPv4 network number from presentation to network format.
 *	accepts hex octets, hex strings, decimal octets, and /CIDR.
 *	"size" is in bytes and describes "dst".
 * return:
 *	number of bits, either imputed classfully or specified with /CIDR,
 *	or -1 if some failure occurred (check errno).  ENOENT means it was
 *	not an IPv4 network specification.
 * note:
 *	network byte order assumed.  this means 192.5.5.240/28 has
 *	0b11110000 in its fourth octet.
 * author:
 *	Paul Vixie (ISC), June 1996
 */
static int
inet_cidr_pton_ipv4(const char *src, u_char *dst, size_t size)
{























































































































































































}

/*
 * int
 * inet_net_pton(af, src, dst, *bits)
 *	convert network address from presentation to network format.
 *	accepts inet_pton()'s input for this "af" plus trailing "/CIDR".
 *	"dst" is assumed large enough for its "af".  "bits" is set to the
 *	/CIDR prefix length, which can have defaults (like /32 for IPv4).
 * return:
 *	-1 if an error occurred (inspect errno; ENOENT means bad format).
 *	0 if successful conversion occurred.
 * note:
 *	192.5.5.1/28 has a nonzero host part, which means it isn't a network
 *	as called for by inet_cidr_pton() but it can be a host address with
 *	an included netmask.
 * author:
 *	Paul Vixie (ISC), October 1998
 */
static int
inet_net_pton_ipv4(const char *src, u_char *dst)
{









































































































}

static int
getbits(const char *src, int *bitsp)
{


































}

static int
getv4(const char *src, u_char *dst, int *bitsp)
{






















































}

static int
inet_net_pton_ipv6(const char *src, u_char *dst)
{

}

#define NS_IN6ADDRSZ 16
#define NS_INT16SZ 2
#define NS_INADDRSZ 4

static int
inet_cidr_pton_ipv6(const char *src, u_char *dst, size_t size)
{














































































































































}