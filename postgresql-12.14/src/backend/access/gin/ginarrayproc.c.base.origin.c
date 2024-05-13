/*-------------------------------------------------------------------------
 *
 * ginarrayproc.c
 *	  support functions for GIN's indexing of any array
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *	  src/backend/access/gin/ginarrayproc.c
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/gin.h"
#include "access/stratnum.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"

#define GinOverlapStrategy 1
#define GinContainsStrategy 2
#define GinContainedStrategy 3
#define GinEqualStrategy 4

/*
 * extractValue support function
 */
Datum
ginarrayextract(PG_FUNCTION_ARGS)
{




















}

/*
 * Formerly, ginarrayextract had only two arguments.  Now it has three,
 * but we still need a pg_proc entry with two args to support reloading
 * pre-9.1 contrib/intarray opclass declarations.  This compatibility
 * function should go away eventually.
 */
Datum
ginarrayextract_2args(PG_FUNCTION_ARGS)
{





}

/*
 * extractQuery support function
 */
Datum
ginqueryarrayextract(PG_FUNCTION_ARGS)
{


























































}

/*
 * consistent support function
 */
Datum
ginarrayconsistent(PG_FUNCTION_ARGS)
{











































































}

/*
 * triconsistent support function
 */
Datum
ginarraytriconsistent(PG_FUNCTION_ARGS)
{












































































}