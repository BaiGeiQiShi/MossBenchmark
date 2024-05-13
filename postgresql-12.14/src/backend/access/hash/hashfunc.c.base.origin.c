/*-------------------------------------------------------------------------
 *
 * hashfunc.c
 *	  Support functions for hash access method.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/access/hash/hashfunc.c
 *
 * NOTES
 *	  These functions are stored in pg_amproc.  For each operator class
 *	  defined for hash indexes, they compute the hash value of the argument.
 *
 *	  Additional hash functions appear in /utils/adt/ files for various
 *	  specialized datatypes.
 *
 *	  It is expected that every bit of a hash function's 32-bit result is
 *	  as random as every other; failure to ensure this is likely to lead
 *	  to poor performance of hash joins, for example.  In most cases a hash
 *	  function should use hash_any() or its variant hash_uint32().
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/hash.h"
#include "catalog/pg_collation.h"
#include "utils/builtins.h"
#include "utils/float.h"
#include "utils/hashutils.h"
#include "utils/pg_locale.h"

/*
 * Datatype-specific hash functions.
 *
 * These support both hash indexes and hash joins.
 *
 * NOTE: some of these are also used by catcache operations, without
 * any direct connection to hash indexes.  Also, the common hash_any
 * routine is also used by dynahash tables.
 */

/* Note: this is used for both "char" and boolean datatypes */
Datum
hashchar(PG_FUNCTION_ARGS)
{

}

Datum
hashcharextended(PG_FUNCTION_ARGS)
{

}

Datum
hashint2(PG_FUNCTION_ARGS)
{

}

Datum
hashint2extended(PG_FUNCTION_ARGS)
{

}

Datum
hashint4(PG_FUNCTION_ARGS)
{

}

Datum
hashint4extended(PG_FUNCTION_ARGS)
{

}

Datum
hashint8(PG_FUNCTION_ARGS)
{















}

Datum
hashint8extended(PG_FUNCTION_ARGS)
{








}

Datum
hashoid(PG_FUNCTION_ARGS)
{

}

Datum
hashoidextended(PG_FUNCTION_ARGS)
{

}

Datum
hashenum(PG_FUNCTION_ARGS)
{

}

Datum
hashenumextended(PG_FUNCTION_ARGS)
{

}

Datum
hashfloat4(PG_FUNCTION_ARGS)
{



































}

Datum
hashfloat4extended(PG_FUNCTION_ARGS)
{
















}

Datum
hashfloat8(PG_FUNCTION_ARGS)
{























}

Datum
hashfloat8extended(PG_FUNCTION_ARGS)
{














}

Datum
hashoidvector(PG_FUNCTION_ARGS)
{



}

Datum
hashoidvectorextended(PG_FUNCTION_ARGS)
{



}

Datum
hashname(PG_FUNCTION_ARGS)
{



}

Datum
hashnameextended(PG_FUNCTION_ARGS)
{



}

Datum
hashtext(PG_FUNCTION_ARGS)
{


















































}

Datum
hashtextextended(PG_FUNCTION_ARGS)
{

















































}

/*
 * hashvarlena() can be used for any varlena datatype in which there are
 * no non-significant bits, ie, distinct bitpatterns never compare as equal.
 */
Datum
hashvarlena(PG_FUNCTION_ARGS)
{









}

Datum
hashvarlenaextended(PG_FUNCTION_ARGS)
{








}