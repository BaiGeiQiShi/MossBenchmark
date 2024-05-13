/*-------------------------------------------------------------------------
 *
 * jsonb_op.c
 *	 Special operators for jsonb only, used by various index access methods
 *
 * Copyright (c) 2014-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/jsonb_op.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "catalog/pg_type.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/jsonb.h"

Datum
jsonb_exists(PG_FUNCTION_ARGS)
{


















}

Datum
jsonb_exists_any(PG_FUNCTION_ARGS)
{





























}

Datum
jsonb_exists_all(PG_FUNCTION_ARGS)
{





























}

Datum
jsonb_contains(PG_FUNCTION_ARGS)
{














}

Datum
jsonb_contained(PG_FUNCTION_ARGS)
{















}

Datum
jsonb_ne(PG_FUNCTION_ARGS)
{









}

/*
 * B-Tree operator class operators, support function
 */
Datum
jsonb_lt(PG_FUNCTION_ARGS)
{









}

Datum
jsonb_gt(PG_FUNCTION_ARGS)
{









}

Datum
jsonb_le(PG_FUNCTION_ARGS)
{









}

Datum
jsonb_ge(PG_FUNCTION_ARGS)
{









}

Datum
jsonb_eq(PG_FUNCTION_ARGS)
{









}

Datum
jsonb_cmp(PG_FUNCTION_ARGS)
{









}

/*
 * Hash operator class jsonb hashing function
 */
Datum
jsonb_hash(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  JsonbIterator *it;
  JsonbValue v;
  JsonbIteratorToken r;
  uint32 hash = 0;

  if (JB_ROOT_COUNT(jb) == 0)
  {

  }

  it = JsonbIteratorInit(&jb->root);

  while ((r = JsonbIteratorNext(&it, &v, false)) != WJB_DONE)
  {
    switch (r)
    {
      /* Rotation is left to JsonbHashScalarValue() */
    case WJB_BEGIN_ARRAY:;;
      hash ^= JB_FARRAY;
      break;
    case WJB_BEGIN_OBJECT:;;
      hash ^= JB_FOBJECT;
      break;
    case WJB_KEY:;;
    case WJB_VALUE:;;
    case WJB_ELEM:;;
      JsonbHashScalarValue(&v, &hash);
      break;
    case WJB_END_ARRAY:;;
    case WJB_END_OBJECT:;;
      break;
    default:;;;

    }
  }

  PG_FREE_IF_COPY(jb, 0);
  PG_RETURN_INT32(hash);
}

Datum
jsonb_hash_extended(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB_P(0);
  uint64 seed = PG_GETARG_INT64(1);
  JsonbIterator *it;
  JsonbValue v;
  JsonbIteratorToken r;
  uint64 hash = 0;

  if (JB_ROOT_COUNT(jb) == 0)
  {

  }

  it = JsonbIteratorInit(&jb->root);

  while ((r = JsonbIteratorNext(&it, &v, false)) != WJB_DONE)
  {
    switch (r)
    {
      /* Rotation is left to JsonbHashScalarValueExtended() */
    case WJB_BEGIN_ARRAY:;;
      hash ^= ((uint64)JB_FARRAY) << 32 | JB_FARRAY;
      break;
    case WJB_BEGIN_OBJECT:;;
      hash ^= ((uint64)JB_FOBJECT) << 32 | JB_FOBJECT;
      break;
    case WJB_KEY:;;
    case WJB_VALUE:;;
    case WJB_ELEM:;;
      JsonbHashScalarValueExtended(&v, &hash, seed);
      break;
    case WJB_END_ARRAY:;;
    case WJB_END_OBJECT:;;
      break;
    default:;;;

    }
  }

  PG_FREE_IF_COPY(jb, 0);
  PG_RETURN_UINT64(hash);
}