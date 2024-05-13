/*-------------------------------------------------------------------------
 *
 * amcmds.c
 *	  Routines for SQL commands that manipulate access methods.
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/amcmds.c
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/pg_am.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "miscadmin.h"
#include "parser/parse_func.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static Oid
lookup_am_handler_func(List *handler_name, char amtype);
static const char *
get_am_type_string(char amtype);

/*
 * CreateAccessMethod
 *		Registers a new access method.
 */
ObjectAddress
CreateAccessMethod(CreateAmStmt *stmt)
{






























































}

/*
 * Guts of access method deletion.
 */
void
RemoveAccessMethodById(Oid amOid)
{





















}

/*
 * get_am_type_oid
 *		Worker for various get_am_*_oid variants
 *
 * If missing_ok is false, throw an error if access method not found.  If
 * true, just return InvalidOid.
 *
 * If amtype is not '\0', an error is raised if the AM found is not of the
 * given type.
 */
static Oid
get_am_type_oid(const char *amname, char amtype, bool missing_ok)
{






















}

/*
 * get_index_am_oid - given an access method name, look up its OID
 *		and verify it corresponds to an index AM.
 */
Oid
get_index_am_oid(const char *amname, bool missing_ok)
{

}

/*
 * get_table_am_oid - given an access method name, look up its OID
 *		and verify it corresponds to an table AM.
 */
Oid
get_table_am_oid(const char *amname, bool missing_ok)
{

}

/*
 * get_am_oid - given an access method name, look up its OID.
 *		The type is not checked.
 */
Oid
get_am_oid(const char *amname, bool missing_ok)
{

}

/*
 * get_am_name - given an access method OID name and type, look up its name.
 */
char *
get_am_name(Oid amOid)
{












}

/*
 * Convert single-character access method type into string for error reporting.
 */
static const char *
get_am_type_string(char amtype)
{











}

/*
 * Convert a handler function name to an Oid.  If the return type of the
 * function doesn't match the given AM type, an error is raised.
 *
 * This function either return valid function Oid or throw an error.
 */
static Oid
lookup_am_handler_func(List *handler_name, char amtype)
{































}