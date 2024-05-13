/*
 *	PostgreSQL type definitions for the INET and CIDR types.
 *
 *	src/backend/utils/adt/network.c
 *
 *	Jon Postel RIP 16 Oct 1998
 */

#include "postgres.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "access/stratnum.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_type.h"
#include "common/ip.h"
#include "libpq/libpq-be.h"
#include "libpq/pqformat.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "nodes/nodeFuncs.h"
#include "nodes/supportnodes.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/hashutils.h"
#include "utils/inet.h"
#include "utils/lsyscache.h"

static int32
network_cmp_internal(inet *a1, inet *a2);
static List *
match_network_function(Node *leftop, Node *rightop, int indexarg, Oid funcid, Oid opfamily);
static List *
match_network_subset(Node *leftop, Node *rightop, bool is_eq, Oid opfamily);
static bool
addressOK(unsigned char *a, int bits, int family);
static inet *
internal_inetpl(inet *ip, int64 addend);

/*
 * Common INET/CIDR input routine
 */
static inet *
network_in(char *src, bool is_cidr)
{









































}

Datum
inet_in(PG_FUNCTION_ARGS)
{



}

Datum
cidr_in(PG_FUNCTION_ARGS)
{



}

/*
 * Common INET/CIDR output routine
 */
static char *
network_out(inet *src, bool is_cidr)
{


















}

Datum
inet_out(PG_FUNCTION_ARGS)
{



}

Datum
cidr_out(PG_FUNCTION_ARGS)
{



}

/*
 *		network_recv		- converts external binary format to
 *inet
 *
 * The external representation is (one byte apiece for)
 * family, bits, is_cidr, address length, address in network byte order.
 *
 * Presence of is_cidr is largely for historical reasons, though it might
 * allow some code-sharing on the client side.  We send it correctly on
 * output, but ignore the value on input.
 */
static inet *
network_recv(StringInfo buf, bool is_cidr)
{














































}

Datum
inet_recv(PG_FUNCTION_ARGS)
{



}

Datum
cidr_recv(PG_FUNCTION_ARGS)
{



}

/*
 *		network_send		- converts inet to binary format
 */
static bytea *
network_send(inet *addr, bool is_cidr)
{




















}

Datum
inet_send(PG_FUNCTION_ARGS)
{



}

Datum
cidr_send(PG_FUNCTION_ARGS)
{



}

Datum
inet_to_cidr(PG_FUNCTION_ARGS)
{












}

Datum
inet_set_masklen(PG_FUNCTION_ARGS)
{





















}

Datum
cidr_set_masklen(PG_FUNCTION_ARGS)
{














}

/*
 * Copy src and set mask length to 'bits' (which must be valid for the family)
 */
inet *
cidr_set_masklen_internal(const inet *src, int bits)
{























}

/*
 *	Basic comparison function for sorting and inet/cidr comparisons.
 *
 * Comparison is first on the common bits of the network part, then on
 * the length of the network part, and then on the whole unmasked address.
 * The effect is that the network part is the major sort key, and for
 * equal network parts we sort on the host part.  Note this is only sane
 * for CIDR if address bits to the right of the mask are guaranteed zero;
 * otherwise logically-equal CIDRs might compare different.
 */

static int32
network_cmp_internal(inet *a1, inet *a2)
{


















}

Datum
network_cmp(PG_FUNCTION_ARGS)
{




}

/*
 *	Boolean ordering tests.
 */
Datum
network_lt(PG_FUNCTION_ARGS)
{




}

Datum
network_le(PG_FUNCTION_ARGS)
{




}

Datum
network_eq(PG_FUNCTION_ARGS)
{




}

Datum
network_ge(PG_FUNCTION_ARGS)
{




}

Datum
network_gt(PG_FUNCTION_ARGS)
{




}

Datum
network_ne(PG_FUNCTION_ARGS)
{




}

/*
 * MIN/MAX support functions.
 */
Datum
network_smaller(PG_FUNCTION_ARGS)
{











}

Datum
network_larger(PG_FUNCTION_ARGS)
{











}

/*
 * Support function for hash indexes on inet/cidr.
 */
Datum
hashinet(PG_FUNCTION_ARGS)
{





}

Datum
hashinetextended(PG_FUNCTION_ARGS)
{




}

/*
 *	Boolean network-inclusion tests.
 */
Datum
network_sub(PG_FUNCTION_ARGS)
{









}

Datum
network_subeq(PG_FUNCTION_ARGS)
{









}

Datum
network_sup(PG_FUNCTION_ARGS)
{









}

Datum
network_supeq(PG_FUNCTION_ARGS)
{









}

Datum
network_overlap(PG_FUNCTION_ARGS)
{









}

/*
 * Planner support function for network subset/superset operators
 */
Datum
network_subset_support(PG_FUNCTION_ARGS)
{

























}

/*
 * match_network_function
 *	  Try to generate an indexqual for a network subset/superset function.
 *
 * This layer is just concerned with identifying the function and swapping
 * the arguments if necessary.
 */
static List *
match_network_function(Node *leftop, Node *rightop, int indexarg, Oid funcid, Oid opfamily)
{











































}

/*
 * match_network_subset
 *	  Try to generate an indexqual for a network subset function.
 */
static List *
match_network_subset(Node *leftop, Node *rightop, bool is_eq, Oid opfamily)
{





















































































}

/*
 * Extract data from a network datatype.
 */
Datum
network_host(PG_FUNCTION_ARGS)
{

















}

/*
 * network_show implements the inet and cidr casts to text.  This is not
 * quite the same behavior as network_out, hence we can't drop it in favor
 * of CoerceViaIO.
 */
Datum
network_show(PG_FUNCTION_ARGS)
{

















}

Datum
inet_abbrev(PG_FUNCTION_ARGS)
{












}

Datum
cidr_abbrev(PG_FUNCTION_ARGS)
{












}

Datum
network_masklen(PG_FUNCTION_ARGS)
{



}

Datum
network_family(PG_FUNCTION_ARGS)
{














}

Datum
network_broadcast(PG_FUNCTION_ARGS)
{









































}

Datum
network_network(PG_FUNCTION_ARGS)
{






































}

Datum
network_netmask(PG_FUNCTION_ARGS)
{





































}

Datum
network_hostmask(PG_FUNCTION_ARGS)
{







































}

/*
 * Returns true if the addresses are from the same family, or false.  Used to
 * check that we can create a network which contains both of the networks.
 */
Datum
inet_same_family(PG_FUNCTION_ARGS)
{




}

/*
 * Returns the smallest CIDR which contains both of the inputs.
 */
Datum
inet_merge(PG_FUNCTION_ARGS)
{











}

/*
 * Convert a value of a network datatype to an approximate scalar value.
 * This is used for estimating selectivities of inequality operators
 * involving network types.
 *
 * On failure (e.g., unsupported typid), set *failure to true;
 * otherwise, that variable is not changed.
 */
double
convert_network_to_scalar(Datum value, Oid typid, bool *failure)
{






















































}

/*
 * int
 * bitncmp(l, r, n)
 *		compare bit masks l and r, for n bits.
 * return:
 *		<0, >0, or 0 in the libc tradition.
 * note:
 *		network byte order assumed.  this means 192.5.5.240/28 has
 *		0x11110000 in its fourth octet.
 * author:
 *		Paul Vixie (ISC), June 1996
 */
int
bitncmp(const unsigned char *l, const unsigned char *r, int n)
{


























}

/*
 * bitncommon: compare bit masks l and r, for up to n bits.
 *
 * Returns the number of leading bits that match (0 to n).
 */
int
bitncommon(const unsigned char *l, const unsigned char *r, int n)
{






























}

/*
 * Verify a CIDR address is OK (doesn't have bits set past the masklen)
 */
static bool
addressOK(unsigned char *a, int bits, int family)
{











































}

/*
 * These functions are used by planner to generate indexscan limits
 * for clauses a << b and a <<= b
 */

/* return the minimal value for an IP on a given network */
Datum
network_scan_first(Datum in)
{

}

/*
 * return "last" IP on a given network. It's the broadcast address,
 * however, masklen has to be set to its max bits, since
 * 192.168.0.255/24 is considered less than 192.168.0.255/32
 *
 * inet_set_masklen() hacked to max out the masklength to 128 for IPv6
 * and 32 for IPv4 when given '-1' as argument.
 */
Datum
network_scan_last(Datum in)
{

}

/*
 * IP address that the client is connecting from (NULL if Unix socket)
 */
Datum
inet_client_addr(PG_FUNCTION_ARGS)
{































}

/*
 * port that the client is connecting from (NULL if Unix socket)
 */
Datum
inet_client_port(PG_FUNCTION_ARGS)
{





























}

/*
 * IP address that the server accepted the connection on (NULL if Unix socket)
 */
Datum
inet_server_addr(PG_FUNCTION_ARGS)
{































}

/*
 * port that the server accepted the connection on (NULL if Unix socket)
 */
Datum
inet_server_port(PG_FUNCTION_ARGS)
{





























}

Datum
inetnot(PG_FUNCTION_ARGS)
{





















}

Datum
inetand(PG_FUNCTION_ARGS)
{




























}

Datum
inetor(PG_FUNCTION_ARGS)
{




























}

static inet *
internal_inetpl(inet *ip, int64 addend)
{













































}

Datum
inetpl(PG_FUNCTION_ARGS)
{




}

Datum
inetmi_int8(PG_FUNCTION_ARGS)
{




}

Datum
inetmi(PG_FUNCTION_ARGS)
{



























































}

/*
 * clean_ipv6_addr --- remove any '%zone' part from an IPv6 address string
 *
 * XXX This should go away someday!
 *
 * This is a kluge needed because we don't yet support zones in stored inet
 * values.  Since the result of getnameinfo() might include a zone spec,
 * call this to remove it anywhere we want to feed getnameinfo's output to
 * network_in.  Beats failing entirely.
 *
 * An alternative approach would be to let network_in ignore %-parts for
 * itself, but that would mean we'd silently drop zone specs in user input,
 * which seems not such a good idea.
 */
void
clean_ipv6_addr(int addr_family, char *addr)
{











}