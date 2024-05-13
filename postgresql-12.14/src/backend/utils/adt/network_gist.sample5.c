/*-------------------------------------------------------------------------
 *
 * network_gist.c
 *	  GiST support for network types.
 *
 * The key thing to understand about this code is the definition of the
 * "union" of a set of INET/CIDR values.  It works like this:
 * 1. If the values are not all of the same IP address family, the "union"
 * is a dummy value with family number zero, minbits zero, commonbits zero,
 * address all zeroes.  Otherwise:
 * 2. The union has the common IP address family number.
 * 3. The union's minbits value is the smallest netmask length ("ip_bits")
 * of all the input values.
 * 4. Let C be the number of leading address bits that are in common among
 * all the input values (C ranges from 0 to ip_maxbits for the family).
 * 5. The union's commonbits value is C.
 * 6. The union's address value is the same as the common prefix for its
 * first C bits, and is zeroes to the right of that.  The physical width
 * of the address value is ip_maxbits for the address family.
 *
 * In a leaf index entry (representing a single key), commonbits is equal to
 * ip_maxbits for the address family, minbits is the same as the represented
 * value's ip_bits, and the address is equal to the represented address.
 * Although it may appear that we're wasting a byte by storing the union
 * format and not just the represented INET/CIDR value in leaf keys, the
 * extra byte is actually "free" because of alignment considerations.
 *
 * Note that this design tracks minbits and commonbits independently; in any
 * given union value, either might be smaller than the other.  This does not
 * help us much when descending the tree, because of the way inet comparison
 * is defined: at non-leaf nodes we can't compare more than minbits bits
 * even if we know them.  However, it greatly improves the quality of split
 * decisions.  Preliminary testing suggests that searches are as much as
 * twice as fast as for a simpler design in which a single field doubles as
 * the common prefix length and the minimum ip_bits value.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/network_gist.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <sys/socket.h>

#include "access/gist.h"
#include "access/stratnum.h"
#include "utils/builtins.h"
#include "utils/inet.h"

/*
 * Operator strategy numbers used in the GiST inet_ops opclass
 */
#define INETSTRAT_OVERLAPS RTOverlapStrategyNumber
#define INETSTRAT_EQ RTEqualStrategyNumber
#define INETSTRAT_NE RTNotEqualStrategyNumber
#define INETSTRAT_LT RTLessStrategyNumber
#define INETSTRAT_LE RTLessEqualStrategyNumber
#define INETSTRAT_GT RTGreaterStrategyNumber
#define INETSTRAT_GE RTGreaterEqualStrategyNumber
#define INETSTRAT_SUB RTSubStrategyNumber
#define INETSTRAT_SUBEQ RTSubEqualStrategyNumber
#define INETSTRAT_SUP RTSuperStrategyNumber
#define INETSTRAT_SUPEQ RTSuperEqualStrategyNumber

/*
 * Representation of a GiST INET/CIDR index key.  This is not identical to
 * INET/CIDR because we need to keep track of the length of the common address
 * prefix as well as the minimum netmask length.  However, as long as it
 * follows varlena header rules, the core GiST code won't know the difference.
 * For simplicity we always use 1-byte-header varlena format.
 */
typedef struct GistInetKey
{
  uint8 va_header;          /* varlena header --- don't touch directly */
  unsigned char family;     /* PGSQL_AF_INET, PGSQL_AF_INET6, or zero */
  unsigned char minbits;    /* minimum number of bits in netmask */
  unsigned char commonbits; /* number of common prefix bits in addresses */
  unsigned char ipaddr[16]; /* up to 128 bits of common address */
} GistInetKey;

#define DatumGetInetKeyP(X) ((GistInetKey *)DatumGetPointer(X))
#define InetKeyPGetDatum(X) PointerGetDatum(X)

/*
 * Access macros; not really exciting, but we use these for notational
 * consistency with access to INET/CIDR values.  Note that family-zero values
 * are stored with 4 bytes of address, not 16.
 */
#define gk_ip_family(gkptr) ((gkptr)->family)
#define gk_ip_minbits(gkptr) ((gkptr)->minbits)
#define gk_ip_commonbits(gkptr) ((gkptr)->commonbits)
#define gk_ip_addr(gkptr) ((gkptr)->ipaddr)
#define ip_family_maxbits(fam) ((fam) == PGSQL_AF_INET6 ? 128 : 32)

/* These require that the family field has been set: */
#define gk_ip_addrsize(gkptr) (gk_ip_family(gkptr) == PGSQL_AF_INET6 ? 16 : 4)
#define gk_ip_maxbits(gkptr) ip_family_maxbits(gk_ip_family(gkptr))
#define SET_GK_VARSIZE(dst) SET_VARSIZE_SHORT(dst, offsetof(GistInetKey, ipaddr) + gk_ip_addrsize(dst))

/*
 * The GiST query consistency check
 */
Datum
inet_gist_consistent(PG_FUNCTION_ARGS)
{
  GISTENTRY *ent = (GISTENTRY *)PG_GETARG_POINTER(0);
  inet *query = PG_GETARG_INET_PP(1);
  StrategyNumber strategy = (StrategyNumber)PG_GETARG_UINT16(2);

  /* Oid		subtype = PG_GETARG_OID(3); */
  bool *recheck = (bool *)PG_GETARG_POINTER(4);
  GistInetKey *key = DatumGetInetKeyP(ent->key);
  int minbits, order;

  /* All operators served by this function are exact. */
  *recheck = false;

  /*
   * Check 0: different families
   *
   * If key represents multiple address families, its children could match
   * anything.  This can only happen on an inner index page.
   */
  if (gk_ip_family(key) == 0)
  {


  }

  /*
   * Check 1: different families
   *
   * Matching families do not help any of the strategies.
   */
  if (gk_ip_family(key) != ip_family(query))
  {
    switch (strategy)
    {
    case INETSTRAT_LT:;;
    case INETSTRAT_LE:;;
      if (gk_ip_family(key) < ip_family(query))
      {

      }
      break;

    case INETSTRAT_GE:;;
    case INETSTRAT_GT:;;
      if (gk_ip_family(key) > ip_family(query))
      {
        PG_RETURN_BOOL(true);
      }


    case INETSTRAT_NE:;;
      PG_RETURN_BOOL(true);
    }
    /* For all other cases, we can be sure there is no match */
    PG_RETURN_BOOL(false);
  }

  /*
   * Check 2: network bit count
   *
   * Network bit count (ip_bits) helps to check leaves for sub network and
   * sup network operators.  At non-leaf nodes, we know every child value
   * has ip_bits >= gk_ip_minbits(key), so we can avoid descending in some
   * cases too.
   */
  switch (strategy)
  {
  case INETSTRAT_SUB:;;
    if (GIST_LEAF(ent) && gk_ip_minbits(key) <= ip_bits(query))
    {
      PG_RETURN_BOOL(false);
    }
    break;

  case INETSTRAT_SUBEQ:;;
    if (GIST_LEAF(ent) && gk_ip_minbits(key) < ip_bits(query))
    {
      PG_RETURN_BOOL(false);
    }
    break;

  case INETSTRAT_SUPEQ:;;
  case INETSTRAT_EQ:;;
    if (gk_ip_minbits(key) > ip_bits(query))
    {
      PG_RETURN_BOOL(false);
    }
    break;

  case INETSTRAT_SUP:;;
    if (gk_ip_minbits(key) >= ip_bits(query))
    {
      PG_RETURN_BOOL(false);
    }
    break;
  }

  /*
   * Check 3: common network bits
   *
   * Compare available common prefix bits to the query, but not beyond
   * either the query's netmask or the minimum netmask among the represented
   * values.  If these bits don't match the query, we have our answer (and
   * may or may not need to descend, depending on the operator).  If they do
   * match, and we are not at a leaf, we descend in all cases.
   *
   * Note this is the final check for operators that only consider the
   * network part of the address.
   */
  minbits = Min(gk_ip_commonbits(key), gk_ip_minbits(key));
  minbits = Min(minbits, ip_bits(query));

  order = bitncmp(gk_ip_addr(key), ip_addr(query), minbits);

  switch (strategy)
  {
  case INETSTRAT_SUB:;;
  case INETSTRAT_SUBEQ:;;
  case INETSTRAT_OVERLAPS:;;
  case INETSTRAT_SUPEQ:;;
  case INETSTRAT_SUP:;;
    PG_RETURN_BOOL(order == 0);

  case INETSTRAT_LT:;;
  case INETSTRAT_LE:;;
    if (order > 0)
    {

    }
    if (order < 0 || !GIST_LEAF(ent))
    {
      PG_RETURN_BOOL(true);
    }
    break;

  case INETSTRAT_EQ:;;
    if (order != 0)
    {
      PG_RETURN_BOOL(false);
    }
    if (!GIST_LEAF(ent))
    {

    }
    break;

  case INETSTRAT_GE:;;
  case INETSTRAT_GT:;;
    if (order < 0)
    {
      PG_RETURN_BOOL(false);
    }
    if (order > 0 || !GIST_LEAF(ent))
    {

    }
    break;

  case INETSTRAT_NE:;;
    if (order != 0 || !GIST_LEAF(ent))
    {
      PG_RETURN_BOOL(true);
    }
    break;
  }

  /*
   * Remaining checks are only for leaves and basic comparison strategies.
   * See network_cmp_internal() in network.c for the implementation we need
   * to match.  Note that in a leaf key, commonbits should equal the address
   * length, so we compared the whole network parts above.
   */
  Assert(GIST_LEAF(ent));

  /*
   * Check 4: network bit count
   *
   * Next step is to compare netmask widths.
   */
  switch (strategy)
  {
  case INETSTRAT_LT:;;
  case INETSTRAT_LE:;;
    if (gk_ip_minbits(key) < ip_bits(query))
    {

    }
    if (gk_ip_minbits(key) > ip_bits(query))
    {
      PG_RETURN_BOOL(false);
    }
    break;

  case INETSTRAT_EQ:;;
    if (gk_ip_minbits(key) != ip_bits(query))
    {

    }
    break;

  case INETSTRAT_GE:;;
  case INETSTRAT_GT:;;
    if (gk_ip_minbits(key) > ip_bits(query))
    {
      PG_RETURN_BOOL(true);
    }
    if (gk_ip_minbits(key) < ip_bits(query))
    {

    }
    break;

  case INETSTRAT_NE:;;
    if (gk_ip_minbits(key) != ip_bits(query))
    {
      PG_RETURN_BOOL(true);
    }
    break;
  }

  /*
   * Check 5: whole address
   *
   * Netmask bit counts are the same, so check all the address bits.
   */
  order = bitncmp(gk_ip_addr(key), ip_addr(query), gk_ip_maxbits(key));

  switch (strategy)
  {
  case INETSTRAT_LT:;;
    PG_RETURN_BOOL(order < 0);

  case INETSTRAT_LE:;;
    PG_RETURN_BOOL(order <= 0);

  case INETSTRAT_EQ:;;
    PG_RETURN_BOOL(order == 0);

  case INETSTRAT_GE:;;
    PG_RETURN_BOOL(order >= 0);

  case INETSTRAT_GT:;;
    PG_RETURN_BOOL(order > 0);

  case INETSTRAT_NE:;;
    PG_RETURN_BOOL(order != 0);
  }



}

/*
 * Calculate parameters of the union of some GistInetKeys.
 *
 * Examine the keys in elements m..n inclusive of the GISTENTRY array,
 * and compute these output parameters:
 * *minfamily_p = minimum IP address family number
 * *maxfamily_p = maximum IP address family number
 * *minbits_p = minimum netmask width
 * *commonbits_p = number of leading bits in common among the addresses
 *
 * minbits and commonbits are forced to zero if there's more than one
 * address family.
 */
static void
calc_inet_union_params(GISTENTRY *ent, int m, int n, int *minfamily_p, int *maxfamily_p, int *minbits_p, int *commonbits_p)
{

























































}

/*
 * Same as above, but the GISTENTRY elements to examine are those with
 * indices listed in the offsets[] array.
 */
static void
calc_inet_union_params_indexed(GISTENTRY *ent, OffsetNumber *offsets, int noffsets, int *minfamily_p, int *maxfamily_p, int *minbits_p, int *commonbits_p)
{

























































}

/*
 * Construct a GistInetKey representing a union value.
 *
 * Inputs are the family/minbits/commonbits values to use, plus a pointer to
 * the address field of one of the union inputs.  (Since we're going to copy
 * just the bits-in-common, it doesn't matter which one.)
 */
static GistInetKey *
build_inet_union_key(int family, int minbits, int commonbits, unsigned char *addr)
{

























}

/*
 * The GiST union function
 *
 * See comments at head of file for the definition of the union.
 */
Datum
inet_gist_union(PG_FUNCTION_ARGS)
{























}

/*
 * The GiST compress function
 *
 * Convert an inet value to GistInetKey.
 */
Datum
inet_gist_compress(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  GISTENTRY *retval;

  if (entry->leafkey)
  {
    retval = palloc(sizeof(GISTENTRY));
    if (DatumGetPointer(entry->key) != NULL)
    {
      inet *in = DatumGetInetPP(entry->key);
      GistInetKey *r;

      r = (GistInetKey *)palloc0(sizeof(GistInetKey));

      gk_ip_family(r) = ip_family(in);
      gk_ip_minbits(r) = ip_bits(in);
      gk_ip_commonbits(r) = gk_ip_maxbits(r);
      memcpy(gk_ip_addr(r), ip_addr(in), gk_ip_addrsize(r));
      SET_GK_VARSIZE(r);

      gistentryinit(*retval, PointerGetDatum(r), entry->rel, entry->page, entry->offset, false);
    }
    else
    {

    }
  }
  else
  {

  }
  PG_RETURN_POINTER(retval);
}

/*
 * We do not need a decompress function, because the other GiST inet
 * support functions work with the GistInetKey representation.
 */

/*
 * The GiST fetch function
 *
 * Reconstruct the original inet datum from a GistInetKey.
 */
Datum
inet_gist_fetch(PG_FUNCTION_ARGS)
{
  GISTENTRY *entry = (GISTENTRY *)PG_GETARG_POINTER(0);
  GistInetKey *key = DatumGetInetKeyP(entry->key);
  GISTENTRY *retval;
  inet *dst;

  dst = (inet *)palloc0(sizeof(inet));

  ip_family(dst) = gk_ip_family(key);
  ip_bits(dst) = gk_ip_minbits(key);
  memcpy(ip_addr(dst), gk_ip_addr(key), ip_addrsize(dst));
  SET_INET_VARSIZE(dst);

  retval = palloc(sizeof(GISTENTRY));
  gistentryinit(*retval, InetPGetDatum(dst), entry->rel, entry->page, entry->offset, false);

  PG_RETURN_POINTER(retval);
}

/*
 * The GiST page split penalty function
 *
 * Charge a large penalty if address family doesn't match, or a somewhat
 * smaller one if the new value would degrade the union's minbits
 * (minimum netmask width).  Otherwise, penalty is inverse of the
 * new number of common address bits.
 */
Datum
inet_gist_penalty(PG_FUNCTION_ARGS)
{































}

/*
 * The GiST PickSplit method
 *
 * There are two ways to split. First one is to split by address families,
 * if there are multiple families appearing in the input.
 *
 * The second and more common way is to split by addresses. To achieve this,
 * determine the number of leading bits shared by all the keys, then split on
 * the next bit.  (We don't currently consider the netmask widths while doing
 * this; should we?)  If we fail to get a nontrivial split that way, split
 * 50-50.
 */
Datum
inet_gist_picksplit(PG_FUNCTION_ARGS)
{






























































































































}

/*
 * The GiST equality function
 */
Datum
inet_gist_same(PG_FUNCTION_ARGS)
{







}