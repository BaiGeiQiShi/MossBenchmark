/*-------------------------------------------------------------------------
 *
 * opclasscmds.c
 *
 *	  Routines for opclass (and opfamily) manipulation commands
 *
 * Portions Copyright (c) 1996-2019, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/commands/opclasscmds.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include <limits.h>

#include "access/genam.h"
#include "access/hash.h"
#include "access/nbtree.h"
#include "access/htup_details.h"
#include "access/sysattr.h"
#include "access/table.h"
#include "catalog/catalog.h"
#include "catalog/dependency.h"
#include "catalog/indexing.h"
#include "catalog/objectaccess.h"
#include "catalog/opfam_internal.h"
#include "catalog/pg_am.h"
#include "catalog/pg_amop.h"
#include "catalog/pg_amproc.h"
#include "catalog/pg_namespace.h"
#include "catalog/pg_opclass.h"
#include "catalog/pg_operator.h"
#include "catalog/pg_opfamily.h"
#include "catalog/pg_proc.h"
#include "catalog/pg_type.h"
#include "commands/alter.h"
#include "commands/defrem.h"
#include "commands/event_trigger.h"
#include "miscadmin.h"
#include "parser/parse_func.h"
#include "parser/parse_oper.h"
#include "parser/parse_type.h"
#include "utils/builtins.h"
#include "utils/fmgroids.h"
#include "utils/lsyscache.h"
#include "utils/rel.h"
#include "utils/syscache.h"

static void
AlterOpFamilyAdd(AlterOpFamilyStmt *stmt, Oid amoid, Oid opfamilyoid, int maxOpNumber, int maxProcNumber, List *items);
static void
AlterOpFamilyDrop(AlterOpFamilyStmt *stmt, Oid amoid, Oid opfamilyoid, int maxOpNumber, int maxProcNumber, List *items);
static void
processTypesSpec(List *args, Oid *lefttype, Oid *righttype);
static void
assignOperTypes(OpFamilyMember *member, Oid amoid, Oid typeoid);
static void
assignProcTypes(OpFamilyMember *member, Oid amoid, Oid typeoid);
static void
addFamilyMember(List **list, OpFamilyMember *member, bool isProc);
static void
storeOperators(List *opfamilyname, Oid amoid, Oid opfamilyoid, Oid opclassoid, List *operators, bool isAdd);
static void
storeProcedures(List *opfamilyname, Oid amoid, Oid opfamilyoid, Oid opclassoid, List *procedures, bool isAdd);
static void
dropOperators(List *opfamilyname, Oid amoid, Oid opfamilyoid, List *operators);
static void
dropProcedures(List *opfamilyname, Oid amoid, Oid opfamilyoid, List *procedures);

/*
 * OpFamilyCacheLookup
 *		Look up an existing opfamily by name.
 *
 * Returns a syscache tuple reference, or NULL if not found.
 */
static HeapTuple
OpFamilyCacheLookup(Oid amID, List *opfamilyname, bool missing_ok)
{


















































}

/*
 * get_opfamily_oid
 *	  find an opfamily OID by possibly qualified name
 *
 * If not found, returns InvalidOid if missing_ok, else throws error.
 */
Oid
get_opfamily_oid(Oid amID, List *opfamilyname, bool missing_ok)
{














}

/*
 * OpClassCacheLookup
 *		Look up an existing opclass by name.
 *
 * Returns a syscache tuple reference, or NULL if not found.
 */
static HeapTuple
OpClassCacheLookup(Oid amID, List *opclassname, bool missing_ok)
{


















































}

/*
 * get_opclass_oid
 *	  find an opclass OID by possibly qualified name
 *
 * If not found, returns InvalidOid if missing_ok, else throws error.
 */
Oid
get_opclass_oid(Oid amID, List *opclassname, bool missing_ok)
{














}

/*
 * CreateOpFamily
 *		Internal routine to make the catalog entry for a new operator
 *family.
 *
 * Caller must have done permissions checks etc. already.
 */
static ObjectAddress
CreateOpFamily(CreateOpFamilyStmt *stmt, const char *opfname, Oid namespaceoid, Oid amoid)
{









































































}

/*
 * DefineOpClass
 *		Define a new index operator class.
 */
ObjectAddress
DefineOpClass(CreateOpClassStmt *stmt)
{































































































































































































































































































































































































}

/*
 * DefineOpFamily
 *		Define a new index operator family.
 */
ObjectAddress
DefineOpFamily(CreateOpFamilyStmt *stmt)
{































}

/*
 * AlterOpFamily
 *		Add or remove operators/procedures within an existing operator
 *family.
 *
 * Note: this implements only ALTER OPERATOR FAMILY ... ADD/DROP.  Some
 * other commands called ALTER OPERATOR FAMILY exist, but go through
 * different code paths.
 */
Oid
AlterOpFamily(AlterOpFamilyStmt *stmt)
{
























































}

/*
 * ADD part of ALTER OP FAMILY
 */
static void
AlterOpFamilyAdd(AlterOpFamilyStmt *stmt, Oid amoid, Oid opfamilyoid, int maxOpNumber, int maxProcNumber, List *items)
{

















































































































}

/*
 * DROP part of ALTER OP FAMILY
 */
static void
AlterOpFamilyDrop(AlterOpFamilyStmt *stmt, Oid amoid, Oid opfamilyoid, int maxOpNumber, int maxProcNumber, List *items)
{




























































}

/*
 * Deal with explicit arg types used in ALTER ADD/DROP
 */
static void
processTypesSpec(List *args, Oid *lefttype, Oid *righttype)
{





















}

/*
 * Determine the lefttype/righttype to assign to an operator,
 * and do any validity checking we can manage.
 */
static void
assignOperTypes(OpFamilyMember *member, Oid amoid, Oid typeoid)
{






























































}

/*
 * Determine the lefttype/righttype to assign to a support procedure,
 * and do any validity checking we can manage.
 */
static void
assignProcTypes(OpFamilyMember *member, Oid amoid, Oid typeoid)
{














































































































































}

/*
 * Add a new family member to the appropriate list, after checking for
 * duplicated strategy or proc number.
 */
static void
addFamilyMember(List **list, OpFamilyMember *member, bool isProc)
{



















}

/*
 * Dump the operators to pg_amop
 *
 * We also make dependency entries in pg_depend for the opfamily entries.
 * If opclassoid is valid then make an INTERNAL dependency on that opclass,
 * else make an AUTO dependency on the opfamily.
 */
static void
storeOperators(List *opfamilyname, Oid amoid, Oid opfamilyoid, Oid opclassoid, List *operators, bool isAdd)
{




























































































}

/*
 * Dump the procedures (support routines) to pg_amproc
 *
 * We also make dependency entries in pg_depend for the opfamily entries.
 * If opclassoid is valid then make an INTERNAL dependency on that opclass,
 * else make an AUTO dependency on the opfamily.
 */
static void
storeProcedures(List *opfamilyname, Oid amoid, Oid opfamilyoid, Oid opclassoid, List *procedures, bool isAdd)
{













































































}

/*
 * Remove operator entries from an opfamily.
 *
 * Note: this is only allowed for "loose" members of an opfamily, hence
 * behavior is always RESTRICT.
 */
static void
dropOperators(List *opfamilyname, Oid amoid, Oid opfamilyoid, List *operators)
{




















}

/*
 * Remove procedure entries from an opfamily.
 *
 * Note: this is only allowed for "loose" members of an opfamily, hence
 * behavior is always RESTRICT.
 */
static void
dropProcedures(List *opfamilyname, Oid amoid, Oid opfamilyoid, List *procedures)
{




















}

/*
 * Deletion subroutines for use by dependency.c.
 */
void
RemoveOpFamilyById(Oid opfamilyOid)
{
















}

void
RemoveOpClassById(Oid opclassOid)
{
















}

void
RemoveAmOpEntryById(Oid entryOid)
{






















}

void
RemoveAmProcEntryById(Oid entryOid)
{






















}

/*
 * Subroutine for ALTER OPERATOR CLASS SET SCHEMA/RENAME
 *
 * Is there an operator class with the given name and signature already
 * in the given namespace?	If so, raise an appropriate error message.
 */
void
IsThereOpClassInNamespace(const char *opcname, Oid opcmethod, Oid opcnamespace)
{





}

/*
 * Subroutine for ALTER OPERATOR FAMILY SET SCHEMA/RENAME
 *
 * Is there an operator family with the given name and signature already
 * in the given namespace?	If so, raise an appropriate error message.
 */
void
IsThereOpFamilyInNamespace(const char *opfname, Oid opfmethod, Oid opfnamespace)
{





}