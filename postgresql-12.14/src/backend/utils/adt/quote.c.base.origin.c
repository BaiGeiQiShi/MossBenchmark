/*-------------------------------------------------------------------------
 *
 * quote.c
 *	  Functions for quoting identifiers and literals
 *
 * Portions Copyright (c) 2000-2019, PostgreSQL Global Development Group
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/quote.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "utils/builtins.h"

/*
 * quote_ident -
 *	  returns a properly quoted identifier
 */
Datum
quote_ident(PG_FUNCTION_ARGS)
{







}

/*
 * quote_literal_internal -
 *	  helper function for quote_literal and quote_literal_cstr
 *
 * NOTE: think not to make this function's behavior change with
 * standard_conforming_strings.  We don't know where the result
 * literal will be used, and so we must generate a result that
 * will work with either setting.  Take a look at what dblink
 * uses this for before thinking you know better.
 */
static size_t
quote_literal_internal(char *dst, const char *src, size_t len)
{
























}

/*
 * quote_literal -
 *	  returns a properly quoted literal
 */
Datum
quote_literal(PG_FUNCTION_ARGS)
{
















}

/*
 * quote_literal_cstr -
 *	  returns a properly quoted literal
 */
char *
quote_literal_cstr(const char *rawstr)
{












}

/*
 * quote_nullable -
 *	  Returns a properly quoted literal, with null values returned
 *	  as the text string 'NULL'.
 */
Datum
quote_nullable(PG_FUNCTION_ARGS)
{








}