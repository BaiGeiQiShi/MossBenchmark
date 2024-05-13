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







































}

Datum
jsonb_hash_extended(PG_FUNCTION_ARGS)
{








































}