/*-------------------------------------------------------------------------
 *
 * dropcmds.c
 *	  handle various "DROP" operations
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/dropcmds.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "access/table.h"
#include "access/xact.h"
#include "catalog/dependency.h"
#include "catalog/namespace.h"
#include "catalog/objectaddress.h"
#include "catalog/pg_class.h"
#include "catalog/pg_proc.h"
#include "commands/defrem.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

static void
does_not_exist_skipping(ObjectType objtype, Node *object);
static bool
owningrel_does_not_exist_skipping(List *object, const char **msg, char **name);
static bool
schema_does_not_exist_skipping(List *object, const char **msg, char **name);
static bool
type_in_list_does_not_exist_skipping(List *typenames, const char **msg, char **name);

/*
 * Drop one or more objects.
 *
 * We don't currently handle all object types here.  Relations, for example,
 * require special handling, because (for example) indexes have additional
 * locking requirements.
 *
 * We look up all the objects first, and then delete them in a single
 * performMultipleDeletions() call.  This avoids unnecessary DROP RESTRICT
 * errors if there are dependencies between them.
 */
void
RemoveObjects(DropStmt *stmt)
{





































































}

/*
 * owningrel_does_not_exist_skipping
 *		Subroutine for RemoveObjects
 *
 * After determining that a specification for a rule or trigger returns that
 * the specified object does not exist, test whether its owning relation, and
 * its schema, exist or not; if they do, return false --- the trigger or rule
 * itself is missing instead.  If the owning relation or its schema do not
 * exist, fill the error message format string and name, and return true.
 */
static bool
owningrel_does_not_exist_skipping(List *object, const char **msg, char **name)
{





















}

/*
 * schema_does_not_exist_skipping
 *		Subroutine for RemoveObjects
 *
 * After determining that a specification for a schema-qualifiable object
 * refers to an object that does not exist, test whether the specified schema
 * exists or not.  If no schema was specified, or if the schema does exist,
 * return false -- the object itself is missing instead.  If the specified
 * schema does not exist, fill the error message format string and the
 * specified schema name, and return true.
 */
static bool
schema_does_not_exist_skipping(List *object, const char **msg, char **name)
{













}

/*
 * type_in_list_does_not_exist_skipping
 *		Subroutine for RemoveObjects
 *
 * After determining that a specification for a function, cast, aggregate or
 * operator returns that the specified object does not exist, test whether the
 * involved datatypes, and their schemas, exist or not; if they do, return
 * false --- the original object itself is missing instead.  If the datatypes
 * or schemas do not exist, fill the error message format string and the
 * missing name, and return true.
 *
 * First parameter is a list of TypeNames.
 */
static bool
type_in_list_does_not_exist_skipping(List *typenames, const char **msg, char **name)
{

























}

/*
 * does_not_exist_skipping
 *		Subroutine for RemoveObjects
 *
 * Generate a NOTICE stating that the named object was not found, and is
 * being skipped.  This is only relevant when "IF EXISTS" is used; otherwise,
 * get_object_address() in RemoveObjects would have thrown an ERROR.
 */
static void
does_not_exist_skipping(ObjectType objtype, Node *object)
{














































































































































































































































}