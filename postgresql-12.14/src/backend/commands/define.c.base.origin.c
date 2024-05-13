/*-------------------------------------------------------------------------
 *
 * define.c
 *	  Support routines for various kinds of object creation.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/define.c
 *
 * DESCRIPTION
 *	  The "DefineFoo" routines take the parse tree and pick out the
 *	  appropriate arguments/flags, passing the results to the
 *	  corresponding "FooDefine" routines (in src/catalog) that do
 *	  the actual catalog-munging.  These routines also verify permission
 *	  of the user to execute the command.
 *
 * NOTES
 *	  These things must be defined and committed in the following order:
 *		"create function":
 *				input/output, recv/send procedures
 *		"create type":
 *				type
 *		"create operator":
 *				operators
 *
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>
#include <math.h>

#include "catalog/namespace.h"
#include "commands/defrem.h"
#include "nodes/makefuncs.h"
#include "parser/parse_type.h"
#include "parser/scansup.h"
#include "utils/builtins.h"

/*
 * Extract a string value (otherwise uninterpreted) from a DefElem.
 */
char *
defGetString(DefElem *def)
{



























}

/*
 * Extract a numeric value (actually double) from a DefElem.
 */
double
defGetNumeric(DefElem *def)
{














}

/*
 * Extract a boolean value from a DefElem.
 */
bool
defGetBoolean(DefElem *def)
{






















































}

/*
 * Extract an int32 value from a DefElem.
 */
int32
defGetInt32(DefElem *def)
{












}

/*
 * Extract an int64 value from a DefElem.
 */
int64
defGetInt64(DefElem *def)
{




















}

/*
 * Extract a possibly-qualified name (as a List of Strings) from a DefElem.
 */
List *
defGetQualifiedName(DefElem *def)
{

















}

/*
 * Extract a TypeName from a DefElem.
 *
 * Note: we do not accept a List arg here, because the parser will only
 * return a bare List when the name looks like an operator name.
 */
TypeName *
defGetTypeName(DefElem *def)
{















}

/*
 * Extract a type length indicator (either absolute bytes, or
 * -1 for "variable") from a DefElem.
 */
int
defGetTypeLength(DefElem *def)
{
































}

/*
 * Extract a list of string values (otherwise uninterpreted) from a DefElem.
 */
List *
defGetStringList(DefElem *def)
{






















}