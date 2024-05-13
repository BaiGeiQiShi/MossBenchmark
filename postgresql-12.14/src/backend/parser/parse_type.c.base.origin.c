/*-------------------------------------------------------------------------
 *
 * parse_type.c
 *		handle type operations for parser
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/parser/parse_type.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/htup_details.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "lib/stringinfo.h"
#include "nodes/makefuncs.h"
#include "parser/parser.h"
#include "parser/parse_type.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"

static int32
typenameTypeMod(ParseState *pstate, const TypeName *typeName, Type typ);

/*
 * LookupTypeName
 *		Wrapper for typical case.
 */
Type
LookupTypeName(ParseState *pstate, const TypeName *typeName, int32 *typmod_p, bool missing_ok)
{

}

/*
 * LookupTypeNameExtended
 *		Given a TypeName object, lookup the pg_type syscache entry of
 *the type. Returns NULL if no such type can be found.  If the type is found,
 *		the typmod value represented in the TypeName struct is computed
 *and stored into *typmod_p.
 *
 * NB: on success, the caller must ReleaseSysCache the type tuple when done
 * with it.
 *
 * NB: direct callers of this function MUST check typisdefined before assuming
 * that the type is fully valid.  Most code should go through typenameType
 * or typenameTypeId instead.
 *
 * typmod_p can be passed as NULL if the caller does not care to know the
 * typmod value, but the typmod decoration (if any) will be validated anyway,
 * except in the case where the type is not found.  Note that if the type is
 * found but is a shell, and there is typmod decoration, an error will be
 * thrown --- this is intentional.
 *
 * If temp_ok is false, ignore types in the temporary namespace.  Pass false
 * when the caller will decide, using goodness of fit criteria, whether the
 * typeName is actually a type or something else.  If typeName always denotes
 * a type (or denotes nothing), pass true.
 *
 * pstate is only used for error location info, and may be NULL.
 */
Type
LookupTypeNameExtended(ParseState *pstate, const TypeName *typeName, int32 *typmod_p, bool temp_ok, bool missing_ok)
{











































































































































}

/*
 * LookupTypeNameOid
 *		Given a TypeName object, lookup the pg_type syscache entry of
 *the type. Returns InvalidOid if no such type can be found.  If the type is
 *found, return its Oid.
 *
 * NB: direct callers of this function need to be aware that the type OID
 * returned may correspond to a shell type.  Most code should go through
 * typenameTypeId instead.
 *
 * pstate is only used for error location info, and may be NULL.
 */
Oid
LookupTypeNameOid(ParseState *pstate, const TypeName *typeName, bool missing_ok)
{


















}

/*
 * typenameType - given a TypeName, return a Type structure and typmod
 *
 * This is equivalent to LookupTypeName, except that this will report
 * a suitable error message if the type cannot be found or is not defined.
 * Callers of this can therefore assume the result is a fully valid type.
 */
Type
typenameType(ParseState *pstate, const TypeName *typeName, int32 *typmod_p)
{












}

/*
 * typenameTypeId - given a TypeName, return the type's OID
 *
 * This is similar to typenameType, but we only hand back the type OID
 * not the syscache entry.
 */
Oid
typenameTypeId(ParseState *pstate, const TypeName *typeName)
{








}

/*
 * typenameTypeIdAndMod - given a TypeName, return the type's OID and typmod
 *
 * This is equivalent to typenameType, but we only hand back the type OID
 * and typmod, not the syscache entry.
 */
void
typenameTypeIdAndMod(ParseState *pstate, const TypeName *typeName, Oid *typeid_p, int32 *typmod_p)
{





}

/*
 * typenameTypeMod - given a TypeName, return the internal typmod value
 *
 * This will throw an error if the TypeName includes type modifiers that are
 * illegal for the data type.
 *
 * The actual type OID represented by the TypeName must already have been
 * looked up, and is passed as "typ".
 *
 * pstate is only used for error location info, and may be NULL.
 */
static int32
typenameTypeMod(ParseState *pstate, const TypeName *typeName, Type typ)
{























































































}

/*
 * appendTypeNameToBuffer
 *		Append a string representing the name of a TypeName to a
 *StringInfo. This is the shared guts of TypeNameToString and
 *TypeNameListToString.
 *
 * NB: this must work on TypeNames that do not describe any actual type;
 * it is mostly used for reporting lookup errors.
 */
static void
appendTypeNameToBuffer(const TypeName *typeName, StringInfo string)
{

































}

/*
 * TypeNameToString
 *		Produce a string representing the name of a TypeName.
 *
 * NB: this must work on TypeNames that do not describe any actual type;
 * it is mostly used for reporting lookup errors.
 */
char *
TypeNameToString(const TypeName *typeName)
{





}

/*
 * TypeNameListToString
 *		Produce a string representing the name(s) of a List of TypeNames
 */
char *
TypeNameListToString(List *typenames)
{















}

/*
 * LookupCollation
 *
 * Look up collation by name, return OID, with support for error location.
 */
Oid
LookupCollation(ParseState *pstate, List *collnames, int location)
{
















}

/*
 * GetColumnDefCollation
 *
 * Get the collation to be used for a column being defined, given the
 * ColumnDef node and the previously-determined column type OID.
 *
 * pstate is only used for error location purposes, and can be NULL.
 */
Oid
GetColumnDefCollation(ParseState *pstate, ColumnDef *coldef, Oid typeOid)
{




























}

/* return a Type structure, given a type id */
/* NB: caller must ReleaseSysCache the type tuple when done with it */
Type
typeidType(Oid id)
{








}

/* given type (as type struct), return the type OID */
Oid
typeTypeId(Type tp)
{





}

/* given type (as type struct), return the length of type */
int16
typeLen(Type t)
{




}

/* given type (as type struct), return its 'byval' attribute */
bool
typeByVal(Type t)
{




}

/* given type (as type struct), return the type's name */
char *
typeTypeName(Type t)
{





}

/* given type (as type struct), return its 'typrelid' attribute */
Oid
typeTypeRelid(Type typ)
{




}

/* given type (as type struct), return its 'typcollation' attribute */
Oid
typeTypeCollation(Type typ)
{




}

/*
 * Given a type structure and a string, returns the internal representation
 * of that string.  The "string" can be NULL to perform conversion of a NULL
 * (which might result in failure, if the input function rejects NULLs).
 */
Datum
stringTypeDatum(Type tp, char *string, int32 atttypmod)
{





}

/*
 * Given a typeid, return the type's typrelid (associated relation), if any.
 * Returns InvalidOid if type is not a composite type.
 */
Oid
typeidTypeRelid(Oid type_id)
{













}

/*
 * Given a typeid, return the type's typrelid (associated relation), if any.
 * Returns InvalidOid if type is not a composite type or a domain over one.
 * This is the same as typeidTypeRelid(getBaseType(type_id)), but faster.
 */
Oid
typeOrDomainTypeRelid(Oid type_id)
{
























}

/*
 * error context callback for parse failure during parseTypeString()
 */
static void
pts_error_callback(void *arg)
{










}

/*
 * Given a string that is supposed to be a SQL-compatible type declaration,
 * such as "int4" or "integer" or "character varying(32)", parse
 * the string and return the result as a TypeName.
 * If the string cannot be parsed as a type, an error is raised.
 */
TypeName *
typeStringToTypeName(const char *str)
{










































































}

/*
 * Given a string that is supposed to be a SQL-compatible type declaration,
 * such as "int4" or "integer" or "character varying(32)", parse
 * the string and convert it to a type OID and type modifier.
 * If missing_ok is true, InvalidOid is returned rather than raising an error
 * when the type name is not found.
 */
void
parseTypeString(const char *str, Oid *typeid_p, int32 *typmod_p, bool missing_ok)
{

























}