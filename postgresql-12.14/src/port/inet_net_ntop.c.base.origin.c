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
 *	  src/port/inet_net_ntop.c
 */

#if defined(LIBC_SCCS) && !defined(lint)
static const char rcsid[] = "Id: inet_net_ntop.c,v 1.1.2.2 2004/03/09 09:17:27 marka Exp $";
#endif

#ifndef FRONTEND
#include "postgres.h"
#else
#include "postgres_fe.h"
#endif

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef FRONTEND
#include "utils/inet.h"
#else
/*
 * In a frontend build, we can't include inet.h, but we still need to have
 * sensible definitions of these two constants.  Note that inet_net_ntop()
 * assumes that PGSQL_AF_INET is equal to AF_INET.
 */
#define PGSQL_AF_INET (AF_INET + 0)
#define PGSQL_AF_INET6 (AF_INET + 1)
#endif

#define NS_IN6ADDRSZ 16
#define NS_INT16SZ 2

#ifdef SPRINTF_CHAR
#define SPRINTF(x) strlen(sprintf /**/ x)
#else
#define SPRINTF(x) ((size_t)sprintf x)
#endif

static char *
inet_net_ntop_ipv4(const u_char *src, int bits, char *dst, size_t size);
static char *
inet_net_ntop_ipv6(const u_char *src, int bits, char *dst, size_t size);

/*
 * char *
 * inet_net_ntop(af, src, bits, dst, size)
 *	convert host/network address from network to presentation format.
 *	"src"'s size is determined from its "af".
 * return:
 *	pointer to dst, or NULL if an error occurred (check errno).
 * note:
 *	192.5.5.1/28 has a nonzero host part, which means it isn't a network
 *	as called for by inet_net_pton() but it can be a host address with
 *	an included netmask.
 * author:
 *	Paul Vixie (ISC), October 1998
 */
char *
inet_net_ntop(int af, const void *src, int bits, char *dst, size_t size)
{




















}

/*
 * static char *
 * inet_net_ntop_ipv4(src, bits, dst, size)
 *	convert IPv4 network address from network to presentation format.
 *	"src"'s size is determined from its "af".
 * return:
 *	pointer to dst, or NULL if an error occurred (check errno).
 * note:
 *	network byte order assumed.  this means 192.5.5.240/28 has
 *	0b11110000 in its fourth octet.
 * author:
 *	Paul Vixie (ISC), October 1998
 */
static char *
inet_net_ntop_ipv4(const u_char *src, int bits, char *dst, size_t size)
{










































}

static int
decoct(const u_char *src, int bytes, char *dst, size_t size)
{




















}

static char *
inet_net_ntop_ipv6(const u_char *src, int bits, char *dst, size_t size)
{


































































































































}