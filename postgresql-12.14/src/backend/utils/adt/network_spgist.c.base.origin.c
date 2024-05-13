/*-------------------------------------------------------------------------
 *
 * network_spgist.c
 *	  SP-GiST support for network types.
 *
 * We split inet index entries first by address family (IPv4 or IPv6).
 * If the entries below a given inner tuple are all of the same family,
 * we identify their common prefix and split by the next bit of the address,
 * and by whether their masklens exceed the length of the common prefix.
 *
 * An inner tuple that has both IPv4 and IPv6 children has a null prefix
 * and exactly two nodes, the first being for IPv4 and the second for IPv6.
 *
 * Otherwise, the prefix is a CIDR value representing the common prefix,
 * and there are exactly four nodes.  Node numbers 0 and 1 are for addresses
 * with the same masklen as the prefix, while node numbers 2 and 3 are for
 * addresses with larger masklen.  (We do not allow a tuple to contain
 * entries with masklen smaller than its prefix's.)  Node numbers 0 and 1
 * are distinguished by the next bit of the address after the common prefix,
 * and likewise for node numbers 2 and 3.  If there are no more bits in
 * the address family, everything goes into node 0 (which will probably
 * lead to creating an allTheSame tuple).
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *			src/backend/utils/adt/network_spgist.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/socket.h>

#include "access/spgist.h"
#include "catalog/pg_type.h"
#include "utils/builtins.h"
#include "utils/inet.h"

static int
inet_spg_node_number(const inet *val, int commonbits);
static int
inet_spg_consistent_bitmap(const inet *prefix, int nkeys, ScanKey scankeys, bool leaf);

/*
 * The SP-GiST configuration function
 */
Datum
inet_spg_config(PG_FUNCTION_ARGS)
{









}

/*
 * The SP-GiST choose function
 */
Datum
inet_spg_choose(PG_FUNCTION_ARGS)
{



















































































}

/*
 * The GiST PickSplit method
 */
Datum
inet_spg_picksplit(PG_FUNCTION_ARGS)
{


































































}

/*
 * The SP-GiST query consistency check for inner tuples
 */
Datum
inet_spg_inner_consistent(PG_FUNCTION_ARGS)
{



















































































}

/*
 * The SP-GiST query consistency check for leaf tuples
 */
Datum
inet_spg_leaf_consistent(PG_FUNCTION_ARGS)
{












}

/*
 * Calculate node number (within a 4-node, single-family inner index tuple)
 *
 * The value must have the same family as the node's prefix, and
 * commonbits is the mask length of the prefix.  We use even or odd
 * nodes according to the next address bit after the commonbits,
 * and low or high nodes according to whether the value's mask length
 * is larger than commonbits.
 */
static int
inet_spg_node_number(const inet *val, int commonbits)
{












}

/*
 * Calculate bitmap of node numbers that are consistent with the query
 *
 * This can be used either at a 4-way inner tuple, or at a leaf tuple.
 * In the latter case, we should return a boolean result (0 or 1)
 * not a bitmap.
 *
 * This definition is pretty odd, but the inner and leaf consistency checks
 * are mostly common and it seems best to keep them in one function.
 */
static int
inet_spg_consistent_bitmap(const inet *prefix, int nkeys, ScanKey scankeys, bool leaf)
{

























































































































































































































































































































































































































}