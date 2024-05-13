/*-------------------------------------------------------------------------
 *
 * regproc.c
 *	  Functions for the built-in types regproc, regclass, regtype, etc.
 *
 * These types are all binary-compatible with type Oid, and rely on Oid
 * for comparison and so forth.  Their only interesting behavior is in
 * special I/O conversion routines.
 *
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/utils/adt/regproc.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <ctype.h>

#include "access/htup_details.h"
#include "catalog/namespace.h"
#include "catalog/pg_class.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_ts_config.h"
#include "catalog/pg_ts_dict.h"
#include "catalog/pg_type.h"
#include "lib/stringinfo.h"
#include "miscadmin.h"
#include "parser/parse_type.h"
#include "parser/scansup.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/syscache.h"
#include "utils/acl.h"
#include "utils/regproc.h"
#include "utils/varlena.h"

static char *
format_operator_internal(Oid operator_oid, bool force_qualify);
static char *
format_procedure_internal(Oid procedure_oid, bool force_qualify);
static void
parseNameAndArgTypes(const char *string, bool allowNone, List **names, int *nargs, Oid *argtypes);

/*****************************************************************************
 *	 USER I/O ROUTINES
 **
 *****************************************************************************/

/*
 * regprocin		- converts "proname" to proc OID
 *
 * We also accept a numeric OID, for symmetry with the output routine.
 *
 * '-' signifies unknown (OID 0).  In all other cases, the input must
 * match an existing pg_proc entry.
 */
Datum
regprocin(PG_FUNCTION_ARGS)
{
















































}

/*
 * to_regproc	- converts "proname" to proc OID
 *
 * If the name is not found, we return NULL.
 */
Datum
to_regproc(PG_FUNCTION_ARGS)
{

















}

/*
 * regprocout		- converts proc OID to "pro_name"
 */
Datum
regprocout(PG_FUNCTION_ARGS)
{


























































}

/*
 *		regprocrecv			- converts external binary
 *format to regproc
 */
Datum
regprocrecv(PG_FUNCTION_ARGS)
{


}

/*
 *		regprocsend			- converts regproc to binary
 *format
 */
Datum
regprocsend(PG_FUNCTION_ARGS)
{


}

/*
 * regprocedurein		- converts "proname(args)" to proc OID
 *
 * We also accept a numeric OID, for symmetry with the output routine.
 *
 * '-' signifies unknown (OID 0).  In all other cases, the input must
 * match an existing pg_proc entry.
 */
Datum
regprocedurein(PG_FUNCTION_ARGS)
{




















































}

/*
 * to_regprocedure	- converts "proname(args)" to proc OID
 *
 * If the name is not found, we return NULL.
 */
Datum
to_regprocedure(PG_FUNCTION_ARGS)
{
























}

/*
 * format_procedure		- converts proc OID to "pro_name(args)"
 *
 * This exports the useful functionality of regprocedureout for use
 * in other backend modules.  The result is a palloc'd string.
 */
char *
format_procedure(Oid procedure_oid)
{

}

char *
format_procedure_qualified(Oid procedure_oid)
{

}

/*
 * Routine to produce regprocedure names; see format_procedure above.
 *
 * force_qualify says whether to schema-qualify; if true, the name is always
 * qualified regardless of search_path visibility.  Otherwise the name is only
 * qualified if the function is not in path.
 */
static char *
format_procedure_internal(Oid procedure_oid, bool force_qualify)
{

























































}

/*
 * Output an objname/objargs representation for the procedure with the
 * given OID.  If it doesn't exist, an error is thrown.
 *
 * This can be used to feed get_object_address.
 */
void
format_procedure_parts(Oid procedure_oid, List **objnames, List **objargs)
{

























}

/*
 * regprocedureout		- converts proc OID to "pro_name(args)"
 */
Datum
regprocedureout(PG_FUNCTION_ARGS)
{













}

/*
 *		regprocedurerecv			- converts external
 *binary format to regprocedure
 */
Datum
regprocedurerecv(PG_FUNCTION_ARGS)
{


}

/*
 *		regproceduresend			- converts regprocedure
 *to binary format
 */
Datum
regproceduresend(PG_FUNCTION_ARGS)
{


}

/*
 * regoperin		- converts "oprname" to operator OID
 *
 * We also accept a numeric OID, for symmetry with the output routine.
 *
 * '0' signifies unknown (OID 0).  In all other cases, the input must
 * match an existing pg_operator entry.
 */
Datum
regoperin(PG_FUNCTION_ARGS)
{













































}

/*
 * to_regoper		- converts "oprname" to operator OID
 *
 * If the name is not found, we return NULL.
 */
Datum
to_regoper(PG_FUNCTION_ARGS)
{

















}

/*
 * regoperout		- converts operator OID to "opr_name"
 */
Datum
regoperout(PG_FUNCTION_ARGS)
{






























































}

/*
 *		regoperrecv			- converts external binary
 *format to regoper
 */
Datum
regoperrecv(PG_FUNCTION_ARGS)
{


}

/*
 *		regopersend			- converts regoper to binary
 *format
 */
Datum
regopersend(PG_FUNCTION_ARGS)
{


}

/*
 * regoperatorin		- converts "oprname(args)" to operator OID
 *
 * We also accept a numeric OID, for symmetry with the output routine.
 *
 * '0' signifies unknown (OID 0).  In all other cases, the input must
 * match an existing pg_operator entry.
 */
Datum
regoperatorin(PG_FUNCTION_ARGS)
{

















































}

/*
 * to_regoperator	- converts "oprname(args)" to operator OID
 *
 * If the name is not found, we return NULL.
 */
Datum
to_regoperator(PG_FUNCTION_ARGS)
{





























}

/*
 * format_operator		- converts operator OID to "opr_name(args)"
 *
 * This exports the useful functionality of regoperatorout for use
 * in other backend modules.  The result is a palloc'd string.
 */
static char *
format_operator_internal(Oid operator_oid, bool force_qualify)
{





























































}

char *
format_operator(Oid operator_oid)
{

}

char *
format_operator_qualified(Oid operator_oid)
{

}

void
format_operator_parts(Oid operator_oid, List **objnames, List **objargs)
{






















}

/*
 * regoperatorout		- converts operator OID to "opr_name(args)"
 */
Datum
regoperatorout(PG_FUNCTION_ARGS)
{













}

/*
 *		regoperatorrecv			- converts external binary
 *format to regoperator
 */
Datum
regoperatorrecv(PG_FUNCTION_ARGS)
{


}

/*
 *		regoperatorsend			- converts regoperator to binary
 *format
 */
Datum
regoperatorsend(PG_FUNCTION_ARGS)
{


}

/*
 * regclassin		- converts "classname" to class OID
 *
 * We also accept a numeric OID, for symmetry with the output routine.
 *
 * '-' signifies unknown (OID 0).  In all other cases, the input must
 * match an existing pg_class entry.
 */
Datum
regclassin(PG_FUNCTION_ARGS)
{



































}

/*
 * to_regclass		- converts "classname" to class OID
 *
 * If the name is not found, we return NULL.
 */
Datum
to_regclass(PG_FUNCTION_ARGS)
{





















}

/*
 * regclassout		- converts class OID to "class_name"
 */
Datum
regclassout(PG_FUNCTION_ARGS)
{























































}

/*
 *		regclassrecv			- converts external binary
 *format to regclass
 */
Datum
regclassrecv(PG_FUNCTION_ARGS)
{


}

/*
 *		regclasssend			- converts regclass to binary
 *format
 */
Datum
regclasssend(PG_FUNCTION_ARGS)
{


}

/*
 * regtypein		- converts "typename" to type OID
 *
 * The type name can be specified using the full type syntax recognized by
 * the parser; for example, DOUBLE PRECISION and INTEGER[] will work and be
 * translated to the correct type names.  (We ignore any typmod info
 * generated by the parser, however.)
 *
 * We also accept a numeric OID, for symmetry with the output routine,
 * and for possible use in bootstrap mode.
 *
 * '-' signifies unknown (OID 0).  In all other cases, the input must
 * match an existing pg_type entry.
 */
Datum
regtypein(PG_FUNCTION_ARGS)
{
































}

/*
 * to_regtype		- converts "typename" to type OID
 *
 * If the name is not found, we return NULL.
 */
Datum
to_regtype(PG_FUNCTION_ARGS)
{

















}

/*
 * regtypeout		- converts type OID to "typ_name"
 */
Datum
regtypeout(PG_FUNCTION_ARGS)
{










































}

/*
 *		regtyperecv			- converts external binary
 *format to regtype
 */
Datum
regtyperecv(PG_FUNCTION_ARGS)
{


}

/*
 *		regtypesend			- converts regtype to binary
 *format
 */
Datum
regtypesend(PG_FUNCTION_ARGS)
{


}

/*
 * regconfigin		- converts "tsconfigname" to tsconfig OID
 *
 * We also accept a numeric OID, for symmetry with the output routine.
 *
 * '-' signifies unknown (OID 0).  In all other cases, the input must
 * match an existing pg_ts_config entry.
 */
Datum
regconfigin(PG_FUNCTION_ARGS)
{
































}

/*
 * regconfigout		- converts tsconfig OID to "tsconfigname"
 */
Datum
regconfigout(PG_FUNCTION_ARGS)
{










































}

/*
 *		regconfigrecv			- converts external binary
 *format to regconfig
 */
Datum
regconfigrecv(PG_FUNCTION_ARGS)
{


}

/*
 *		regconfigsend			- converts regconfig to binary
 *format
 */
Datum
regconfigsend(PG_FUNCTION_ARGS)
{


}

/*
 * regdictionaryin		- converts "tsdictionaryname" to tsdictionary
 * OID
 *
 * We also accept a numeric OID, for symmetry with the output routine.
 *
 * '-' signifies unknown (OID 0).  In all other cases, the input must
 * match an existing pg_ts_dict entry.
 */
Datum
regdictionaryin(PG_FUNCTION_ARGS)
{
































}

/*
 * regdictionaryout		- converts tsdictionary OID to
 * "tsdictionaryname"
 */
Datum
regdictionaryout(PG_FUNCTION_ARGS)
{











































}

/*
 *		regdictionaryrecv	- converts external binary format to
 *regdictionary
 */
Datum
regdictionaryrecv(PG_FUNCTION_ARGS)
{


}

/*
 *		regdictionarysend	- converts regdictionary to binary
 *format
 */
Datum
regdictionarysend(PG_FUNCTION_ARGS)
{


}

/*
 * regrolein	- converts "rolename" to role OID
 *
 * We also accept a numeric OID, for symmetry with the output routine.
 *
 * '-' signifies unknown (OID 0).  In all other cases, the input must
 * match an existing pg_authid entry.
 */
Datum
regrolein(PG_FUNCTION_ARGS)
{


































}

/*
 * to_regrole		- converts "rolename" to role OID
 *
 * If the name is not found, we return NULL.
 */
Datum
to_regrole(PG_FUNCTION_ARGS)
{





















}

/*
 * regroleout		- converts role OID to "role_name"
 */
Datum
regroleout(PG_FUNCTION_ARGS)
{
























}

/*
 *		regrolerecv - converts external binary format to regrole
 */
Datum
regrolerecv(PG_FUNCTION_ARGS)
{


}

/*
 *		regrolesend - converts regrole to binary format
 */
Datum
regrolesend(PG_FUNCTION_ARGS)
{


}

/*
 * regnamespacein		- converts "nspname" to namespace OID
 *
 * We also accept a numeric OID, for symmetry with the output routine.
 *
 * '-' signifies unknown (OID 0).  In all other cases, the input must
 * match an existing pg_namespace entry.
 */
Datum
regnamespacein(PG_FUNCTION_ARGS)
{


































}

/*
 * to_regnamespace		- converts "nspname" to namespace OID
 *
 * If the name is not found, we return NULL.
 */
Datum
to_regnamespace(PG_FUNCTION_ARGS)
{





















}

/*
 * regnamespaceout		- converts namespace OID to "nsp_name"
 */
Datum
regnamespaceout(PG_FUNCTION_ARGS)
{
























}

/*
 *		regnamespacerecv	- converts external binary format to
 *regnamespace
 */
Datum
regnamespacerecv(PG_FUNCTION_ARGS)
{


}

/*
 *		regnamespacesend		- converts regnamespace to
 *binary format
 */
Datum
regnamespacesend(PG_FUNCTION_ARGS)
{


}

/*
 * text_regclass: convert text to regclass
 *
 * This could be replaced by CoerceViaIO, except that we need to treat
 * text-to-regclass as an implicit cast to support legacy forms of nextval()
 * and related functions.
 */
Datum
text_regclass(PG_FUNCTION_ARGS)
{










}

/*
 * Given a C string, parse it into a qualified-name list.
 */
List *
stringToQualifiedNameList(const char *string)
{





























}

/*****************************************************************************
 *	 SUPPORT ROUTINES
 **
 *****************************************************************************/

/*
 * Given a C string, parse it into a qualified function or operator name
 * followed by a parenthesized list of type names.  Reduce the
 * type names to an array of OIDs (returned into *nargs and *argtypes;
 * the argtypes array should be of size FUNC_MAX_ARGS).  The function or
 * operator name is returned to *names as a List of Strings.
 *
 * If allowNone is true, accept "NONE" and return it as InvalidOid (this is
 * for unary operators).
 */
static void
parseNameAndArgTypes(const char *string, bool allowNone, List **names, int *nargs, Oid *argtypes)
{




















































































































































}